#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <WinSock2.h>
#endif
#include "port/native_miniupnp.hpp"
#include "port/native_router_io.hpp"
#include <miniupnpc.h>
#include <upnpcommands.h>
#include <upnperrors.h>
static_assert(MINIUPNPC_API_VERSION==21,"Pinned MiniUPnPc 2.3.3 API required");

namespace partyboard::net {
namespace {
struct NetworkLifetime {
    NetworkLifetime(){
#if defined(_WIN32)
        WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)!=0)throw std::runtime_error("Router sockets unavailable");
#endif
    }
    ~NetworkLifetime(){
#if defined(_WIN32)
        WSACleanup();
#endif
    }
};
void ensureNetwork(){static NetworkLifetime network;}
std::optional<uint32_t> number(std::string_view text){
    uint32_t value=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(text.empty()||parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size())return {};
    return value;
}
}
std::optional<NativeRouterGateway> discoverNativeRouterGateway(std::stop_token cancellation){
    NativeRouterIoBudget budget(cancellation,5000,1048576);
    if(!budget.ok())return {};
    ensureNetwork();int error=0;
    auto* devices=upnpDiscover(1000,nullptr,nullptr,UPNP_LOCAL_PORT_ANY,0,2,&error);
    if(!devices)return {};
    struct DeviceList {UPNPDev* value;~DeviceList(){freeUPNPDevlist(value);}} list{devices};
    UPNPUrls urls{};IGDdatas data{};char local[64]{},external[64]{};
    struct Urls {UPNPUrls* value;~Urls(){FreeUPNPUrls(value);}} cleanup{&urls};
    if(!budget.ok())return {};
    const int status=UPNP_GetValidIGD(devices,&urls,&data,local,sizeof(local),external,sizeof(external));
    if(!budget.ok()||status!=UPNP_CONNECTED_IGD||!urls.controlURL||!routerIpv4(local)||!routerIpv4(external))return {};
    return NativeRouterGateway{urls.controlURL,data.first.servicetype,local,external};
}
NativeMiniupnpBackend::NativeMiniupnpBackend(NativeRouterGateway gateway,std::stop_token cancellation,uint32_t operationTimeoutMs)
    :gateway_(std::move(gateway)),cancellation_(cancellation),operationTimeoutMs_(operationTimeoutMs){
    ensureNetwork();
    if(operationTimeoutMs<50||operationTimeoutMs>5000||!routerIpv4(gateway_.internalAddress)||!routerIpv4(gateway_.externalAddress)||gateway_.controlUrl.rfind("http://",0)!=0||
       (gateway_.service!="urn:schemas-upnp-org:service:WANIPConnection:1"&&gateway_.service!="urn:schemas-upnp-org:service:WANIPConnection:2"&&gateway_.service!="urn:schemas-upnp-org:service:WANPPPConnection:1"))
        throw std::invalid_argument("Valid discovered IPv4 router service required");
}
RouterMappingQuery NativeMiniupnpBackend::query(uint16_t externalPort){
    NativeRouterIoBudget budget(cancellation_,operationTimeoutMs_);
    if(!externalPort)throw std::invalid_argument("UDP external port required");
    const auto port=std::to_string(externalPort);
    char client[16]{},internal[6]{},description[80]{},enabled[4]{},duration[16]{};
    const int error=UPNP_GetSpecificPortMappingEntry(gateway_.controlUrl.c_str(),gateway_.service.c_str(),port.c_str(),"UDP","",client,internal,description,enabled,duration);
    if(!budget.ok())return {RouterMappingQuery::State::Unavailable,{},UPNPCOMMAND_HTTP_ERROR};
    if(error==714)return {RouterMappingQuery::State::Absent,{},error};
    if(error!=UPNPCOMMAND_SUCCESS)return {RouterMappingQuery::State::Unavailable,{},error};
    const auto internalPort=number(internal);
    if(!routerIpv4(client)||!internalPort||!*internalPort||*internalPort>65535||
       (std::string_view(enabled)!="0"&&std::string_view(enabled)!="1"))return {RouterMappingQuery::State::Unavailable,{},UPNPCOMMAND_INVALID_RESPONSE};
    return {RouterMappingQuery::State::Found,{client,description,static_cast<uint16_t>(*internalPort),std::string_view(enabled)=="1",number(duration)},0};
}
int NativeMiniupnpBackend::add(const RouterMappingRequest& request,const std::string& owner,uint32_t leaseSeconds){
    NativeRouterIoBudget budget(cancellation_,operationTimeoutMs_);
    if(request.internalAddress!=gateway_.internalAddress||!request.internalPort||!request.externalPort||!leaseSeconds||leaseSeconds>3600||owner.size()!=43||owner.substr(0,11)!="Partyboard-"||owner.find_first_not_of("0123456789abcdef",11)!=std::string::npos)
        throw std::invalid_argument("Owned finite router-local UDP mapping required");
    const auto external=std::to_string(request.externalPort),internal=std::to_string(request.internalPort),lease=std::to_string(leaseSeconds);
    const int error=UPNP_AddPortMapping(gateway_.controlUrl.c_str(),gateway_.service.c_str(),external.c_str(),internal.c_str(),request.internalAddress.c_str(),owner.c_str(),"UDP","",lease.c_str());
    return budget.ok()?error:UPNPCOMMAND_HTTP_ERROR;
}
int NativeMiniupnpBackend::remove(uint16_t externalPort){
    NativeRouterIoBudget budget(cancellation_,operationTimeoutMs_);
    if(!externalPort)throw std::invalid_argument("UDP external port required");
    const auto port=std::to_string(externalPort);
    const int error=UPNP_DeletePortMapping(gateway_.controlUrl.c_str(),gateway_.service.c_str(),port.c_str(),"UDP","");
    return budget.ok()?error:UPNPCOMMAND_HTTP_ERROR;
}
}
