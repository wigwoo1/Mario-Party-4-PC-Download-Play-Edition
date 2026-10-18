#pragma once
#include <rtc/rtc.hpp>
#include "port/native_router_mapping.hpp"
#include <set>

namespace partyboard::net {
inline std::vector<RouterMappingRequest> nativeRouterGatheredPorts(const rtc::Description& description){
    std::vector<RouterMappingRequest> result;std::set<std::pair<std::string,uint16_t>> ports;
    for(auto candidate:description.candidates()){
        if(candidate.type()!=rtc::Candidate::Type::Host||candidate.transportType()!=rtc::Candidate::TransportType::Udp||
           !candidate.resolve(rtc::Candidate::ResolveMode::Simple)||candidate.family()!=rtc::Candidate::Family::Ipv4)continue;
        const auto address=candidate.address();const auto port=candidate.port();
        if(address&&routerIpv4(*address)&&port&&*port&&ports.emplace(*address,*port).second)result.push_back({*address,*port,*port});
    }
    return result;
}
// Read only gathered ICE host candidates belonging to the gateway's LAN
// interface. Never allocate/guess a UDP port or map another interface/transport.
inline std::vector<RouterMappingRequest> nativeRouterPortRequests(const rtc::Description& description,const std::string& internalAddress){
    if(!routerIpv4(internalAddress))throw std::invalid_argument("Router LAN IPv4 address required");
    auto result=nativeRouterGatheredPorts(description);
    result.erase(std::remove_if(result.begin(),result.end(),[&](const auto& request){return request.internalAddress!=internalAddress;}),result.end());
    return result;
}
}
