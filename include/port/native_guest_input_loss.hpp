#pragma once
#include "port/paused_state.hpp"

namespace partyboard::net {
// Caller must bind this message to the original authenticated host channel.
// The notice identifies an assigned remote owner; it never authorizes resume.
inline uint32_t receiveNativeGuestInputLoss(const nlohmann::ordered_json& packet,
        const RecoverySession& identity,const std::string& localPeer,const std::string& boundHost){
    bool localAssigned=false,hostAssigned=false;
    for(const auto& player:identity.players){
        localAssigned|=player.peer==localPeer;
        hostAssigned|=player.peer==boundHost&&player.slot==0;
    }
    if(!localAssigned||!hostAssigned||localPeer==boundHost||!packet.is_object()||packet.size()!=4||
       packet.value("type","")!="session-input-loss"||packet.value("version",0)!=1||
       packet.value("session","")!=identity.session||!packet.contains("peer")||!packet.at("peer").is_string())
        throw std::invalid_argument("Original assigned input loss notice required");
    const auto peer=packet.at("peer").get<std::string>();
    for(const auto& player:identity.players)
        if(player.peer==peer&&peer!=localPeer&&player.slot<4)return uint32_t(player.slot)+1;
    throw std::invalid_argument("Assigned remote input loss owner required");
}
}
