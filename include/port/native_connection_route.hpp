#pragma once
#include <rtc/rtc.hpp>
#include <exception>

namespace partyboard::net {
enum class NativeConnectionRoute { Connecting, Unknown, Direct, Relayed, Interrupted, Unreachable, Closed };
inline NativeConnectionRoute classifyNativeConnectionRoute(rtc::PeerConnection::State state,
        bool selected=false,rtc::Candidate::Type local=rtc::Candidate::Type::Unknown,
        rtc::Candidate::Type remote=rtc::Candidate::Type::Unknown){
    using State=rtc::PeerConnection::State;using Type=rtc::Candidate::Type;
    switch(state){
    case State::Failed:return NativeConnectionRoute::Unreachable;
    case State::Closed:return NativeConnectionRoute::Closed;
    case State::Disconnected:return NativeConnectionRoute::Interrupted;
    case State::New:case State::Connecting:return NativeConnectionRoute::Connecting;
    case State::Connected:break;
    default:return NativeConnectionRoute::Unknown;
    }
    if(!selected)return NativeConnectionRoute::Unknown;
    if(local==Type::Relayed||remote==Type::Relayed)return NativeConnectionRoute::Relayed;
    const auto known=[](Type type){return type==Type::Host||type==Type::ServerReflexive||type==Type::PeerReflexive;};
    return known(local)&&known(remote)?NativeConnectionRoute::Direct:NativeConnectionRoute::Unknown;
}
inline NativeConnectionRoute nativeConnectionRoute(rtc::PeerConnection& connection){
    const auto state=connection.state();
    if(state!=rtc::PeerConnection::State::Connected)return classifyNativeConnectionRoute(state);
    rtc::Candidate local,remote;
    try{const bool selected=connection.getSelectedCandidatePair(&local,&remote);
        return classifyNativeConnectionRoute(connection.state(),selected,local.type(),remote.type());}
    catch(const std::exception&){return classifyNativeConnectionRoute(connection.state());}
}
inline const char* nativeConnectionRouteLabel(NativeConnectionRoute route){
    switch(route){
    case NativeConnectionRoute::Connecting:return "Connecting";
    case NativeConnectionRoute::Direct:return "Direct";
    case NativeConnectionRoute::Relayed:return "Relayed";
    case NativeConnectionRoute::Interrupted:return "Interrupted";
    case NativeConnectionRoute::Unreachable:return "Unreachable";
    case NativeConnectionRoute::Closed:return "Closed";
    default:return "Unknown";
    }
}
}
