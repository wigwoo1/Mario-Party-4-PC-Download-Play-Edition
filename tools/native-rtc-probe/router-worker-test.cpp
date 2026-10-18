#include "port/native_router_worker.hpp"
#include <iostream>
#include <source_location>
using namespace partyboard::net;
void check(bool ok,std::source_location location=std::source_location::current()){
    if(!ok)throw std::runtime_error("router worker assertion at line "+std::to_string(location.line()));
}
template<class Predicate>void wait(Predicate predicate){
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(!predicate()){if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Router worker test timed out");std::this_thread::sleep_for(std::chrono::milliseconds(10));}
}
struct State {
    std::mutex mutex;std::map<uint16_t,RouterMappingEntry> mappings;
    unsigned adds=0,removes=0;std::thread::id thread;
};
class Backend:public RouterMappingBackend {
    std::shared_ptr<State> state_;
public:
    explicit Backend(std::shared_ptr<State> state):state_(std::move(state)){}
    RouterMappingQuery query(uint16_t port)override{
        std::lock_guard lock(state_->mutex);state_->thread=std::this_thread::get_id();const auto found=state_->mappings.find(port);
        return found==state_->mappings.end()?RouterMappingQuery{RouterMappingQuery::State::Absent,{},714}:RouterMappingQuery{RouterMappingQuery::State::Found,found->second,0};
    }
    int add(const RouterMappingRequest& request,const std::string& owner,uint32_t lease)override{
        std::lock_guard lock(state_->mutex);++state_->adds;
        state_->mappings[request.externalPort]={request.internalAddress,owner,request.internalPort,true,lease};return 0;
    }
    int remove(uint16_t port)override{std::lock_guard lock(state_->mutex);++state_->removes;state_->mappings.erase(port);return 0;}
};
int main(){try{
    auto state=std::make_shared<State>();const auto mainThread=std::this_thread::get_id();
    NativeRouterWorker worker([=](std::stop_token)->std::optional<NativeRouterWorker::Backend>{return NativeRouterWorker::Backend{{"http://127.0.0.1:1/control","urn:schemas-upnp-org:service:WANIPConnection:1","192.168.1.23","203.0.113.8"},std::make_unique<Backend>(state)};});
    auto original=worker.retain({{"192.168.1.23",45678,45678}}),replacement=worker.retain({{"192.168.1.23",45678,45678}});
    wait([&]{return original.snapshot().mapped.size()==1&&replacement.snapshot().mapped.size()==1;});
    {std::lock_guard lock(state->mutex);check(state->adds==1&&state->removes==0&&state->thread!=mainThread);}
    original.close();std::this_thread::sleep_for(std::chrono::milliseconds(150));check(original.snapshot().closed&&replacement.snapshot().mapped.size()==1);
    {std::lock_guard lock(state->mutex);check(state->adds==1&&state->removes==0);}
    auto changedPort=worker.retain({{"192.168.1.23",45679,45679},{"192.168.2.23",45680,45680}});
    wait([&]{return changedPort.snapshot().complete&&changedPort.snapshot().mapped.size()==1;});
    replacement.close();wait([&]{std::lock_guard lock(state->mutex);return state->removes==1;});
    check(changedPort.snapshot().mapped.size()==1);check(worker.close());check(changedPort.snapshot().closed);
    {std::lock_guard lock(state->mutex);check(state->adds==2&&state->removes==2&&state->mappings.empty());}
    check(worker.retain({{"192.168.1.23",45678,45678}}).snapshot().closed);check(worker.close());
    {
        NativeRouterWorker unavailable([](std::stop_token)->std::optional<NativeRouterWorker::Backend>{return {};});
        auto ticket=unavailable.retain({{"192.168.1.23",45678,45678}});wait([&]{return ticket.snapshot().complete;});
        check(ticket.snapshot().mapped.empty()&&unavailable.close());
    }
    {
        std::mutex mutex;std::condition_variable changed;bool entered=false;
        NativeRouterWorker cancel([&](std::stop_token stop)->std::optional<NativeRouterWorker::Backend>{
            std::unique_lock lock(mutex);entered=true;changed.notify_all();std::stop_callback wake(stop,[&]{changed.notify_all();});
            changed.wait(lock,[&]{return stop.stop_requested();});return {};
        });
        {std::unique_lock lock(mutex);check(changed.wait_for(lock,std::chrono::seconds(2),[&]{return entered;}));}
        auto queued=cancel.retain({{"192.168.1.23",45678,45678}});
        const auto start=std::chrono::steady_clock::now();check(cancel.close());check(queued.snapshot().closed);
        check(std::chrono::steady_clock::now()-start<std::chrono::seconds(1));
    }
    std::cout<<"PASS worker off-thread I/O, shared-port original/replacement lifetime, changed ports, interface filtering, no-router fallback, queued cancellation and clean shutdown\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
