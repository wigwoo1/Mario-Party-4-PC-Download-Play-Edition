#include "port/native_miniupnp.hpp"
#include "port/native_router_worker.hpp"
#include "port/native_router_io.hpp"
#include <iostream>
using namespace partyboard::net;
int main(int argc,char** argv){try{
    if(argc!=3)throw std::invalid_argument("Usage: router-mapping-http-probe LOOPBACK_CONTROL_URL SCENARIO");
    const std::string url=argv[1],scenario=argv[2];
    if(url.rfind("http://127.0.0.1:",0)!=0)throw std::invalid_argument("Test fixture must be loopback");
    if(scenario=="worker-shared"||scenario=="worker-cancel"||scenario=="worker-cleanup-stall"){
        NativeRouterWorker worker([&](std::stop_token stop)->std::optional<NativeRouterWorker::Backend>{
            NativeRouterGateway gateway{url,"urn:schemas-upnp-org:service:WANIPConnection:1","192.168.1.23","203.0.113.8"};
            return NativeRouterWorker::Backend{gateway,std::make_unique<NativeMiniupnpBackend>(gateway,stop)};
        });
        auto first=worker.retain({{"192.168.1.23",45678,45678}});
        auto second=worker.retain({{"192.168.1.23",45678,45678}});
        if(scenario=="worker-cancel")std::this_thread::sleep_for(std::chrono::milliseconds(150));
        else{
            const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
            while(first.snapshot().mapped.empty()||second.snapshot().mapped.empty()){
                if(std::chrono::steady_clock::now()>=until)throw std::runtime_error("Worker mapping did not become ready");
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            first.close();std::this_thread::sleep_for(std::chrono::milliseconds(150));
            if(second.snapshot().mapped.size()!=1)throw std::runtime_error("Replacement lost shared original port");
        }
        const auto start=std::chrono::steady_clock::now();const bool cleaned=worker.close();
        const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
        if(!first.snapshot().closed||!second.snapshot().closed)throw std::runtime_error("Worker stop left an active ticket");
        if(scenario=="worker-cleanup-stall"){
            if(cleaned||elapsed<2500||elapsed>4500)throw std::runtime_error("Worker failed to report bounded unknown cleanup");
        }else if(!cleaned||elapsed>1000)throw std::runtime_error("Worker cancellation/cleanup did not finish promptly");
        std::cout<<"PASS actual router worker "<<scenario<<"; closeMs="<<elapsed<<"; cleanupVerified="<<cleaned<<'\n';return 0;
    }
    if(scenario=="stall"||scenario=="trickle"||scenario=="oversized"||scenario=="cancel"||scenario=="outer-deadline"||scenario=="dns-blocked"){
        std::stop_source stop;
        auto control=url;
        if(scenario=="dns-blocked")control.replace(7,9,"localhost");
        NativeMiniupnpBackend bounded({control,"urn:schemas-upnp-org:service:WANIPConnection:1","192.168.1.23","203.0.113.8"},stop.get_token(),scenario=="cancel"||scenario=="outer-deadline"?3000:250);
        std::jthread cancel;
        if(scenario=="cancel")cancel=std::jthread([&]{std::this_thread::sleep_for(std::chrono::milliseconds(100));stop.request_stop();});
        const auto start=std::chrono::steady_clock::now();
        RouterMappingQuery value;
        if(scenario=="outer-deadline"){NativeRouterIoBudget overall({},150);value=bounded.query(45678);}
        else value=bounded.query(45678);
        const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
        if(value.state!=RouterMappingQuery::State::Unavailable||elapsed>1000)throw std::runtime_error("Bounded query/cancellation failed");
        if((scenario=="stall"||scenario=="trickle")&&elapsed<150)throw std::runtime_error("Stall did not exercise the request deadline");
        std::cout<<"PASS actual bounded MiniUPnPc SOAP "<<scenario<<"; elapsedMs="<<elapsed<<'\n';return 0;
    }
    NativeMiniupnpBackend backend({url,"urn:schemas-upnp-org:service:WANIPConnection:1","192.168.1.23","203.0.113.8"});
    NativeRouterMappingLease lease(backend,{"192.168.1.23",45678,45678},"Partyboard-0123456789abcdef0123456789abcdef");
    lease.acquire(0);
    const auto require=[](bool ok){if(!ok)throw std::runtime_error("HTTP router scenario did not satisfy expected state");};
    if(scenario=="normal"||scenario=="foreign-on-renew"||scenario=="foreign-on-close"){
        require(lease.active(0));
        if(scenario!="foreign-on-close")lease.tick(300000);
        require(scenario=="foreign-on-renew"?lease.state()==RouterLeaseState::Conflict:lease.active(300000));
    }else if(scenario=="conflict")require(lease.state()==RouterLeaseState::Conflict);
    else if(scenario=="permanent"||scenario=="missing-lease")require(lease.state()==RouterLeaseState::UnsupportedLease);
    else if(scenario=="query-error"||scenario=="malformed-response")require(lease.state()==RouterLeaseState::Unavailable);
    else if(scenario=="permanent-only"||scenario=="add-error")require(lease.state()==RouterLeaseState::Lost);
    else if(scenario=="lost-add-reply")require(lease.active(0));
    else throw std::invalid_argument("Unknown test scenario");
    require(lease.close());
    std::cout<<"PASS actual MiniUPnPc SOAP adapter: "<<scenario<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
