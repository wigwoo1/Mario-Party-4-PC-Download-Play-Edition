#pragma once
#include "port/native_router_mapping.hpp"
#include <memory>

namespace partyboard::net {
struct NativeRouterGateway {
    std::string controlUrl,service,internalAddress,externalAddress;
};
// Synchronous adapter for a dedicated router worker. Discovery is explicit;
// constructing the adapter or policy alone never contacts a physical router.
std::optional<NativeRouterGateway> discoverNativeRouterGateway(std::stop_token cancellation={});
class NativeMiniupnpBackend final:public RouterMappingBackend {
    NativeRouterGateway gateway_;
    std::stop_token cancellation_;
    uint32_t operationTimeoutMs_;
public:
    explicit NativeMiniupnpBackend(NativeRouterGateway gateway,std::stop_token cancellation={},uint32_t operationTimeoutMs=3000);
    void setCancellation(std::stop_token cancellation)override{cancellation_=cancellation;}
    const NativeRouterGateway& gateway()const{return gateway_;}
    RouterMappingQuery query(uint16_t externalPort)override;
    int add(const RouterMappingRequest&,const std::string& owner,uint32_t leaseSeconds)override;
    int remove(uint16_t externalPort)override;
};
}
