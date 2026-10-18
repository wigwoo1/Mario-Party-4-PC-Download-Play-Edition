#include "port/native_router_mapping.hpp"
#include <iostream>
#include <source_location>
#include <deque>
#include <functional>
#include <vector>
using namespace partyboard::net;
void check(bool ok,std::source_location location=std::source_location::current()){
    if(!ok)throw std::runtime_error("router policy assertion at line "+std::to_string(location.line()));
}
const std::string token="Partyboard-0123456789abcdef0123456789abcdef";
const RouterMappingRequest request{"192.168.1.23",45678,45678};
struct Backend:RouterMappingBackend {
    std::optional<RouterMappingEntry> mapping;
    std::deque<RouterMappingQuery> responses;
    unsigned adds=0,removes=0,queries=0;
    bool queryFailure=false,addFailure=false,deleteFailure=false,permanent=false,malformedLease=false;
    std::function<void()> afterAdd;
    RouterMappingQuery query(uint16_t port)override{
        check(port==45678);++queries;
        if(!responses.empty()){auto value=responses.front();responses.pop_front();return value;}
        if(queryFailure)return {};
        return mapping?RouterMappingQuery{RouterMappingQuery::State::Found,*mapping,0}:RouterMappingQuery{RouterMappingQuery::State::Absent,{},714};
    }
    int add(const RouterMappingRequest& value,const std::string& owner,uint32_t seconds)override{
        check(value.internalAddress==request.internalAddress&&value.internalPort==45678&&seconds==600);++adds;
        if(addFailure)return -1;
        mapping=RouterMappingEntry{value.internalAddress,owner,value.internalPort,true,malformedLease?std::nullopt:std::optional<uint32_t>(permanent?0:seconds)};
        if(afterAdd)afterAdd();return 0;
    }
    int remove(uint16_t port)override{check(port==45678);++removes;if(deleteFailure)return -1;mapping.reset();return 0;}
};
int main(){try{
    check(routerIpv4("192.168.1.23")&&routerIpv4("127.0.0.1")&&!routerIpv4("192.168.1.256")&&!routerIpv4("01.2.3.4")&&!routerIpv4("1.2.3")&&!routerIpv4("1.2.3.4.5")&&!routerIpv4("1.2.3.-4"));
    check(newRouterMappingOwner()!=newRouterMappingOwner());
    {
        Backend backend;NativeRouterMappingLease lease(backend,request,token);lease.acquire(1000);
        check(lease.active(1000)&&lease.expiresAt()==601000&&lease.renewAt()==301000&&backend.adds==1);
        lease.tick(300999);check(backend.adds==1);lease.tick(301000);check(backend.adds==2&&lease.expiresAt()==901000);
        check(lease.close()&&!lease.active(301000)&&backend.removes==1);check(lease.close()&&backend.removes==1);
        bool rejected=false;try{lease.acquire(301000);}catch(const std::logic_error&){rejected=true;}check(rejected);
    }
    for(const auto& foreign:std::vector<RouterMappingEntry>{{request.internalAddress,"Other application",45678,true,600},{"192.168.1.24",token,45678,true,600},{request.internalAddress,token,45679,true,600},{request.internalAddress,"Partyboard-other-session",45678,true,600},{request.internalAddress,token,45678,true,600}}){
        Backend backend;backend.mapping=foreign;NativeRouterMappingLease lease(backend,request,token);lease.acquire(0);
        check(lease.state()==RouterLeaseState::Conflict&&backend.adds==0);check(lease.close()&&backend.removes==0);
    }
    {
        Backend backend;backend.queryFailure=true;NativeRouterMappingLease lease(backend,request,token);lease.acquire(0);
        check(lease.state()==RouterLeaseState::Unavailable&&backend.adds==0&&lease.close()&&backend.removes==0);
    }
    for(bool onRenew:{false,true}){
        Backend backend;NativeRouterMappingLease lease(backend,request,token);lease.acquire(0);
        backend.mapping->description="Another instance";
        if(onRenew){lease.tick(300000);check(lease.state()==RouterLeaseState::Conflict&&backend.adds==1);}
        check(lease.close()&&backend.removes==0&&backend.mapping.has_value());
    }
    for(bool malformed:{false,true}){
        Backend backend;backend.permanent=!malformed;backend.malformedLease=malformed;
        NativeRouterMappingLease lease(backend,request,token);lease.acquire(0);
        check(!lease.active(0)&&backend.adds==1&&backend.removes==1&&!backend.mapping);
    }
    {
        Backend backend;NativeRouterMappingLease lease(backend,request,token);lease.acquire(0);
        backend.queryFailure=true;check(!lease.close()&&lease.state()==RouterLeaseState::CleanupPending&&backend.removes==0);
        backend.queryFailure=false;backend.deleteFailure=true;check(!lease.close()&&backend.removes==1);
        backend.mapping->description="Replaced while cleanup was pending";check(lease.close()&&backend.removes==1);
    }
    {
        Backend backend;backend.afterAdd=[&]{backend.queryFailure=true;};
        NativeRouterMappingLease lease(backend,request,token);lease.acquire(0);
        check(!lease.active(0)&&lease.state()==RouterLeaseState::Unavailable);
        backend.queryFailure=false;check(lease.close()&&backend.removes==1);
    }
    {
        Backend backend;NativeRouterMappingLease lease(backend,request,token);lease.acquire(0);
        backend.queryFailure=true;lease.tick(300000);check(!lease.active(300000)&&backend.adds==1);
        backend.queryFailure=false;lease.tick(301000);check(lease.active(301000)&&backend.adds==2);
        check(!lease.active(901000));lease.tick(901000);check(lease.state()==RouterLeaseState::Lost&&backend.adds==2);
    }
    {
        Backend backend;NativeRouterMappingLease lease(backend,request,token);lease.acquire(0);
        backend.mapping.reset();lease.tick(300000);check(lease.state()==RouterLeaseState::Lost&&backend.adds==1);
    }
    {
        Backend backend;{NativeRouterMappingLease lease(backend,request,token);lease.acquire(0);}check(backend.removes==1);
    }
    {
        Backend backend;backend.addFailure=true;NativeRouterMappingLease lease(backend,request,token);lease.acquire(0);
        check(!lease.active(0)&&backend.adds==1&&backend.removes==0&&lease.state()==RouterLeaseState::Lost);
    }
    std::cout<<"PASS router lease acquisition, finite duration, half-life renewal, conflicts, ownership loss, expiry, ambiguous replies, retryable cleanup and destructor cleanup\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
