#pragma once
#include "port/session_start_barrier.hpp"
#include <optional>
namespace partyboard::net {
// Retained snapshots come only from the original host's assigned Lobby channel.
class NativeGuestLobbySnapshot {
    std::string asset_,peer_;
    uint64_t total_;
    std::optional<PreparedStartLobby> current_;
    std::optional<nlohmann::json> wire_;
    static uint64_t integer(const nlohmann::ordered_json& value){
        if(!value.is_number_integer()||value.get<int64_t>()<0||value.get<uint64_t>()>9007199254740991ULL)
            throw std::invalid_argument("Safe lobby integer required");
        return value.get<uint64_t>();
    }
public:
    NativeGuestLobbySnapshot(std::string asset,uint64_t total,std::string peer)
        :asset_(std::move(asset)),peer_(std::move(peer)),total_(total){
        if(asset_.size()!=64||!std::all_of(asset_.begin(),asset_.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');})||
            !total_||total_>2147483648ULL||peer_.empty()||peer_.size()>128)throw std::invalid_argument("Bounded guest lobby identity required");
    }
    void receive(const std::string& packet){
        if(packet.size()>65536)throw std::invalid_argument("Bounded lobby snapshot required");
        const auto envelope=nlohmann::ordered_json::parse(packet);
        if(envelope.at("type")!="lobby-snapshot"||envelope.at("version")!=1)throw std::invalid_argument("Lobby envelope required");
        const auto& snapshot=envelope.at("snapshot");
        if(snapshot.at("assetId")!=asset_||integer(snapshot.at("total"))!=total_||
            snapshot.at("saveRequired")!=false||!snapshot.at("saveHash").is_null())throw std::invalid_argument("Original no-save lobby required");
        PreparedStartLobby next{asset_,total_,integer(snapshot.at("revision")),integer(snapshot.at("membership")),{}};
        if(next.membership>next.revision||!snapshot.at("players").is_array()||snapshot.at("players").empty()||snapshot.at("players").size()>4)
            throw std::invalid_argument("Bounded lobby membership required");
        if(current_&&next.revision==current_->revision&&wire_&&*wire_==nlohmann::json(snapshot))return;
        if(current_&&(next.revision<=current_->revision||next.membership<current_->membership))throw std::invalid_argument("Stale lobby snapshot");
        std::set<std::string> peers;std::set<uint64_t> slots;bool assigned=false,host=false;
        for(const auto& player:snapshot.at("players")){
            const auto peer=player.at("peer").get<std::string>();const auto slot=integer(player.at("slot")),bytes=integer(player.at("bytes"));
            if(peer.empty()||peer.size()>128||!peers.insert(peer).second||slot>3||!slots.insert(slot).second||bytes>total_||!player.at("saveHash").is_null())throw std::invalid_argument("Invalid lobby player");
            for(const auto key:{"ready","verified","shaders"})if(!player.at(key).is_boolean())throw std::invalid_argument("Boolean lobby readiness required");
            const bool ready=player.at("ready"),verified=player.at("verified"),shaders=player.at("shaders");
            if((verified&&bytes!=total_)||(shaders&&!verified)||(ready&&!shaders))throw std::invalid_argument("Invalid lobby preparation");
            if(slot==0)host=true;
            if(peer==peer_){if(slot==0)throw std::invalid_argument("Guest cannot occupy host slot");assigned=true;}
            next.players.push_back({peer,uint8_t(slot),ready,verified,shaders,bytes});
        }
        if(!assigned||!host)throw std::invalid_argument("Original host/guest membership required");
        current_=std::move(next);wire_=snapshot;
    }
    const PreparedStartLobby& current()const{if(!current_)throw std::runtime_error("Host lobby snapshot unavailable");return *current_;}
};
}
