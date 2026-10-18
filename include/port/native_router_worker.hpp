#pragma once
#include "port/native_miniupnp.hpp"
#include "port/native_router_io.hpp"
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace partyboard::net {
struct NativeRouterMappedPort {
    RouterMappingRequest request;
    std::string externalAddress;
    uint64_t expiresAt=0;
};
struct NativeRouterTicketSnapshot {
    bool complete=false,closed=false;
    std::vector<NativeRouterMappedPort> mapped;
};
class NativeRouterTicket {
    struct State {std::mutex mutex;NativeRouterTicketSnapshot snapshot;};
    std::shared_ptr<State> state_;
    explicit NativeRouterTicket(std::shared_ptr<State> state):state_(std::move(state)){}
    friend class NativeRouterWorker;
public:
    NativeRouterTicket()=default;
    NativeRouterTicket(const NativeRouterTicket&)=delete;
    NativeRouterTicket& operator=(const NativeRouterTicket&)=delete;
    NativeRouterTicket(NativeRouterTicket&&)=default;
    NativeRouterTicket& operator=(NativeRouterTicket&& other)noexcept{
        if(this!=&other){close();state_=std::move(other.state_);}return *this;
    }
    NativeRouterTicketSnapshot snapshot()const{
        if(!state_)return {true,true,{}};
        std::lock_guard lock(state_->mutex);auto result=state_->snapshot;
        const auto now=NativeRouterWorkerClock();
        result.mapped.erase(std::remove_if(result.mapped.begin(),result.mapped.end(),[&](const auto& port){return now>=port.expiresAt;}),result.mapped.end());
        return result;
    }
    void close(){if(state_){std::lock_guard lock(state_->mutex);state_->snapshot={true,true,{}};}}
    ~NativeRouterTicket(){close();}
    static uint64_t NativeRouterWorkerClock(){return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
};
class NativeRouterWorker {
public:
    struct Backend {
        NativeRouterGateway gateway;
        std::unique_ptr<RouterMappingBackend> operations;
    };
    using Factory=std::function<std::optional<Backend>(std::stop_token)>;
private:
    struct Submission {std::weak_ptr<NativeRouterTicket::State> ticket;std::vector<RouterMappingRequest> requests;};
    struct Shared {
        std::mutex mutex;std::condition_variable changed;
        std::vector<Submission> incoming;
        bool closed=false,cleanupComplete=false;
    };
    std::shared_ptr<Shared> shared_=std::make_shared<Shared>();
    std::jthread worker_;
    static void publish(const Submission& submission,bool complete,const std::vector<NativeRouterMappedPort>& mapped){
        if(const auto ticket=submission.ticket.lock()){
            std::lock_guard lock(ticket->mutex);
            if(!ticket->snapshot.closed)ticket->snapshot={complete,false,mapped};
        }
    }
    static bool live(const Submission& submission){
        if(const auto ticket=submission.ticket.lock()){std::lock_guard lock(ticket->mutex);return !ticket->snapshot.closed;}
        return false;
    }
    static void run(std::shared_ptr<Shared> shared,Factory factory,std::stop_token stop){
        std::vector<Submission> active;
        std::map<uint16_t,std::unique_ptr<NativeRouterMappingLease>> leases;
        std::optional<Backend> backend;
        try{backend=factory(stop);}catch(...){/* Optional traversal failure must not fail the game. */}
        if(backend&&backend->operations)backend->operations->setCancellation(stop);
        else backend.reset();
        while(!stop.stop_requested()){
            {
                std::unique_lock lock(shared->mutex);
                if(shared->incoming.empty())shared->changed.wait_for(lock,std::chrono::milliseconds(50));
                for(auto& submission:shared->incoming)active.push_back(std::move(submission));
                shared->incoming.clear();
            }
            if(stop.stop_requested())break;
            active.erase(std::remove_if(active.begin(),active.end(),[](const auto& submission){return !live(submission);}),active.end());
            std::map<uint16_t,RouterMappingRequest> desired;
            if(backend)for(const auto& submission:active)for(const auto& request:submission.requests)
                if(request.internalAddress==backend->gateway.internalAddress)desired.emplace(request.externalPort,request);
            for(auto it=leases.begin();it!=leases.end();){
                if(!desired.contains(it->first)||it->second->state()==RouterLeaseState::CleanupPending||it->second->state()==RouterLeaseState::Closed){
                    try{if(it->second->close()){it=leases.erase(it);continue;}}catch(...){}
                }
                ++it;
            }
            if(backend){
                for(const auto& [port,request]:desired){
                    if(stop.stop_requested())break;
                    try{
                        auto found=leases.find(port);
                        if(found==leases.end()){
                            auto lease=std::make_unique<NativeRouterMappingLease>(*backend->operations,request);
                            auto* owned=lease.get();leases.emplace(port,std::move(lease));
                            owned->acquire(NativeRouterTicket::NativeRouterWorkerClock());
                        }else found->second->tick(NativeRouterTicket::NativeRouterWorkerClock());
                    }catch(...){/* Cleanup retains a possibly owned mapping. */}
                }
            }
            for(const auto& submission:active){
                std::vector<NativeRouterMappedPort> mapped;
                if(backend)for(const auto& request:submission.requests){
                    const auto found=leases.find(request.externalPort);
                    if(request.internalAddress==backend->gateway.internalAddress&&found!=leases.end()&&found->second->active(NativeRouterTicket::NativeRouterWorkerClock()))
                        mapped.push_back({request,backend->gateway.externalAddress,found->second->expiresAt()});
                }
                publish(submission,true,mapped);
            }
        }
        // Stop invalidates every ticket before any blocking cleanup. Cancelled
        // in-flight operations finish; cleanup gets a separate total deadline.
        {
            std::lock_guard lock(shared->mutex);for(auto& submission:shared->incoming)active.push_back(std::move(submission));shared->incoming.clear();
        }
        for(const auto& submission:active)if(const auto ticket=submission.ticket.lock()){
            std::lock_guard lock(ticket->mutex);ticket->snapshot={true,true,{}};
        }
        if(backend)backend->operations->setCancellation({});
        NativeRouterIoBudget cleanup({},3000);
        while(!leases.empty()&&cleanup.ok()){
            for(auto it=leases.begin();it!=leases.end();){
                if(!cleanup.ok())break;
                bool closed=false;try{closed=it->second->close();}catch(...){}
                if(closed)it=leases.erase(it);else ++it;
            }
            if(!leases.empty()&&cleanup.ok())std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        // Lease destructors also inherit the exhausted cleanup budget. They
        // cannot reopen an unbounded socket wait after the worker deadline.
        {std::lock_guard lock(shared->mutex);shared->cleanupComplete=leases.empty();}
        leases.clear();
    }
public:
    explicit NativeRouterWorker(Factory factory=[](std::stop_token stop)->std::optional<Backend>{
#if !defined(_WIN32)
        // Other platforms retain normal ICE until their router socket hooks
        // provide the same operation/cancellation bounds as Windows.
        return {};
#else
        auto gateway=discoverNativeRouterGateway(stop);if(!gateway)return {};
        return Backend{*gateway,std::make_unique<NativeMiniupnpBackend>(*gateway,stop)};
#endif
    }):worker_([state=shared_,factory=std::move(factory)](std::stop_token stop){run(state,factory,stop);}){}
    NativeRouterWorker(const NativeRouterWorker&)=delete;
    NativeRouterWorker& operator=(const NativeRouterWorker&)=delete;
    NativeRouterTicket retain(std::vector<RouterMappingRequest> requests){
        if(requests.size()>16)throw std::invalid_argument("Too many gathered router ports");
        for(const auto& request:requests)if(!routerIpv4(request.internalAddress)||!request.internalPort||request.internalPort!=request.externalPort)
            throw std::invalid_argument("Actual gathered same-port UDP mapping required");
        auto ticket=std::make_shared<NativeRouterTicket::State>();
        {
            std::lock_guard lock(shared_->mutex);
            if(shared_->closed)ticket->snapshot={true,true,{}};
            else shared_->incoming.push_back({ticket,std::move(requests)});
        }
        shared_->changed.notify_one();return NativeRouterTicket(std::move(ticket));
    }
    bool close(){
        {std::lock_guard lock(shared_->mutex);shared_->closed=true;}
        worker_.request_stop();shared_->changed.notify_all();
        if(worker_.joinable())worker_.join();
        std::lock_guard lock(shared_->mutex);return shared_->cleanupComplete;
    }
    ~NativeRouterWorker(){close();}
};
}
