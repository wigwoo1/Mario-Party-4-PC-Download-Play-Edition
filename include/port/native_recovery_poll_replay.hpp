#pragma once
#include "paused_state.hpp"
#include "lockstep.hpp"
#include <vector>

namespace partyboard::net {
struct NativePollReplay {
    std::vector<std::pair<uint32_t,Packet>> packets;
    nlohmann::ordered_json target;
};
// Validates evidence only. This does not advance input or resume the engine.
inline NativePollReplay validateNativePollReplay(const RecoverySession& identity,
    const PausedCapture& captured,const std::string& peer,const std::string& remotePeer,
    const std::string& nonce,const nlohmann::ordered_json& report,
    const nlohmann::ordered_json& receipt,const std::array<uint32_t,4>& owners,
    std::span<const Packet> pendingPackets){
    const auto local=nativePausedStateReport(identity,captured,peer,nonce);
    bool remoteAssigned=false;for(const auto& player:identity.players)remoteAssigned|=player.peer==remotePeer;
    if(peer==remotePeer||!remoteAssigned||pendingPackets.empty()||report.size()!=local.size()||
       captured.inputFrame==UINT64_MAX||captured.frame==UINT32_MAX)
        throw std::invalid_argument("Assigned original paused replay required");
    auto expected=local;expected["reporter"]=remotePeer;expected["frame"]=captured.frame+1;
    expected["inputFrame"]=std::to_string(captured.inputFrame+1);
    for(const auto* key:{"checkpoint","frand","rand8"}){
        const auto& value=report.at(key);
        if(!value.is_number_integer()||value.get<int64_t>()<0||value.get<uint64_t>()>UINT32_MAX)
            throw std::invalid_argument("Replay target state invalid");
        expected[key]=value;
    }
    if(nlohmann::json(report)!=nlohmann::json(expected)||receipt.size()!=5||receipt.at("frame")!=local.at("inputFrame"))
        throw std::invalid_argument("Replay original identity or one-frame boundary differs");
    for(const auto* key:{"checkpoint","frand","rand8"})if(receipt.at(key)!=local.at(key))
        throw std::invalid_argument("Replay pending checkpoint or RNG differs");
    const auto& raw=receipt.at("pads");if(!raw.is_array()||raw.size()!=44)throw std::invalid_argument("Replay PAD layout invalid");
    std::array<uint8_t,44> pads{};
    for(size_t i=0;i<pads.size();++i){const auto& value=raw[i];
        if(!value.is_number_integer()||value.get<int64_t>()<0||value.get<uint64_t>()>255)throw std::invalid_argument("Replay PAD byte invalid");
        pads[i]=value.get<uint8_t>();}
    SessionId session{};const auto nibble=[](char c){return c<='9'?c-'0':c-'a'+10;};
    for(size_t i=0;i<session.size();++i)session[i]=uint8_t((nibble(identity.session[2*i])<<4)|nibble(identity.session[2*i+1]));
    uint32_t localOwner=0;std::set<uint8_t> localSlots;
    for(const auto& bytes:pendingPackets){const auto packet=decode(bytes);
        if(!packet||packet->session!=session||packet->frame!=captured.inputFrame||packet->checkpoint!=captured.checkpoint||
           !owners[packet->slot]||!localSlots.insert(packet->slot).second)throw std::invalid_argument("Original replay local packet invalid");
        if(!localOwner)localOwner=owners[packet->slot];
        if(localOwner!=owners[packet->slot])throw std::invalid_argument("Original replay local owner differs");
        for(size_t i=0;i<11;++i)if(packet->pad[i]!=pads[packet->slot*11+i])throw std::invalid_argument("Replay changes frozen local input");
    }
    NativePollReplay result;result.target=report;bool missingAssigned=false;
    for(uint8_t slot=0;slot<4;++slot){bool assigned=false;for(const auto& player:identity.players)assigned|=player.slot==slot;
        if(assigned!=bool(owners[slot])||(owners[slot]==localOwner&&!localSlots.contains(slot)))throw std::invalid_argument("Replay controller membership differs");
        missingAssigned|=owners[slot]==captured.missingOwner;
        Pad pad{};for(size_t i=0;i<11;++i)pad[i]=pads[slot*11+i];
        if(!owners[slot]){for(size_t i=0;i<11;++i)if(pad[i]!=(i==10?255:0))throw std::invalid_argument("Replay changes inactive controller");}
        else result.packets.push_back({owners[slot],encode({session,captured.inputFrame,captured.checkpoint,slot,pad})});
    }
    if(!missingAssigned)throw std::invalid_argument("Assigned missing replay owner required");return result;
}
}
