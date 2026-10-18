#pragma once
#include "port/native_prepared_report.hpp"

namespace partyboard::net {
// Game-thread owner. The transport supplies authenticated peer names. This
// owns preparation/start agreement, not engine resume or transport lifetime.
class NativeStartCoordinator {
    using Json=nlohmann::ordered_json;
    PreparedStartLobby lobby_;
    Json configuration_;
    std::map<std::string,Json> receipts_;
    std::optional<SessionStartBarrier> barrier_;
    bool cancelled_=false;
    bool delivered_=false;
    bool assigned(const std::string& peer)const{
        return std::any_of(lobby_.players.begin(),lobby_.players.end(),[&](const auto& player){return player.peer==peer;});
    }
public:
    NativeStartCoordinator(PreparedStartLobby lobby,Json configuration):
        lobby_(std::move(lobby)),configuration_(std::move(configuration)){}
    bool preparedReceiptsComplete()const{
        if(cancelled_||barrier_||lobby_.players.size()<2||lobby_.players.size()>4||receipts_.size()!=lobby_.players.size())return false;
        return std::all_of(lobby_.players.begin(),lobby_.players.end(),[&](const auto& player){return receipts_.contains(player.peer);});
    }
    bool receivePrepared(const std::string& boundPeer,const std::string& packet){
        if(cancelled_||!assigned(boundPeer))return false;
        try{
            const auto report=receiveNativePreparedRuntimeReport(packet,configuration_);
            const auto found=receipts_.find(boundPeer);
            if(found!=receipts_.end()&&found->second!=report){cancel();return false;}
            if(barrier_&&found==receipts_.end()){cancel();return false;}
            receipts_[boundPeer]=report;return true;
        }catch(...){cancel();return false;}
    }
    Json begin(const PreparedStartRuntime& runtime,const Json& actualHostReceipt){
        if(cancelled_||barrier_)throw std::runtime_error("Native start proposal unavailable");
        barrier_=nativeStartBarrierFromPreparedReceipts(lobby_,configuration_,runtime,actualHostReceipt,receipts_);
        return barrier_->proposal();
    }
    bool acknowledge(const std::string& boundPeer,const std::string& packet){
        if(cancelled_||!barrier_||packet.size()>65536)return false;
        try{return barrier_->acknowledge(boundPeer,Json::parse(packet),lobby_);}
        catch(...){return false;}
    }
    bool update(const PreparedStartLobby& current){
        if(cancelled_)return false;
        // All readiness/asset/membership changes invalidate the owned agreement.
        bool same=current.assetId==lobby_.assetId&&current.total==lobby_.total&&
            current.revision==lobby_.revision&&current.membership==lobby_.membership&&current.players.size()==lobby_.players.size();
        std::set<std::string> peers;std::set<uint8_t> slots;
        for(const auto& player:current.players){
            same=same&&peers.insert(player.peer).second&&slots.insert(player.slot).second;
            auto found=std::find_if(lobby_.players.begin(),lobby_.players.end(),[&](const auto& original){return original.peer==player.peer;});
            same=same&&found!=lobby_.players.end();
            if(found!=lobby_.players.end())same=same&&found->slot==player.slot&&found->ready==player.ready&&
                found->verified==player.verified&&found->shaders==player.shaders&&found->bytes==player.bytes;
        }
        if(!same){cancel();return false;}return true;
    }
    Json prepareCommit(const PreparedStartRuntime& captured){
        if(cancelled_||delivered_||!barrier_)throw std::runtime_error("Native start agreement unavailable");
        return barrier_->prepareCommit(lobby_,captured);
    }
    // Sender is owned by the coordinator and must return true only when the
    // assigned peer's current authenticated control channel accepts the send.
    // Delivery does not itself resume an engine or prove remote commit receipt.
    template<class Sender> bool deliverCommit(const PreparedStartRuntime& captured,
            const std::string& localHost,Sender&& send){
        try{
            const auto host=std::find_if(lobby_.players.begin(),lobby_.players.end(),
                [&](const auto& player){return player.peer==localHost&&player.slot==0;});
            if(host==lobby_.players.end())throw std::invalid_argument("Original local host required");
            const auto packet=prepareCommit(captured).dump();
            for(const auto& player:lobby_.players)if(player.peer!=localHost&&!send(player.peer,packet)){
                cancel();return false;
            }
            delivered_=true;return true;
        }catch(...){cancel();return false;}
    }
    bool commitDelivered()const{return delivered_&&!cancelled_;}
    bool acknowledgmentsComplete()const{return !cancelled_&&barrier_&&barrier_->state()==SessionStartBarrier::State::Agreed;}
    void cancel(){cancelled_=true;receipts_.clear();if(barrier_)barrier_->cancel();}
};
}
