#pragma once
#include "port/session_start_barrier.hpp"
#include <optional>

namespace partyboard::net {
// The caller supplies the latest authenticated lobby and actual stopped engine.
// Agreement never resumes the engine or installs input transport.
class NativeGuestStartAgreement {
    using Json=nlohmann::ordered_json;
    std::string assignedPeer_;
    Json configuration_;
    std::optional<Json> proposal_;
    bool committed_=false;
public:
    NativeGuestStartAgreement(std::string assignedPeer,Json configuration)
        :assignedPeer_(std::move(assignedPeer)),configuration_(std::move(configuration)){}
    Json acknowledge(const std::string& packet,const PreparedStartLobby& lobby,
            const PreparedStartRuntime& captured){
        if(proposal_||committed_||packet.size()>65536)throw std::runtime_error("Single bounded Join start proposal required");
        const auto expected=SessionStartBarrier::fromConfiguration(lobby,configuration_,captured).proposal();
        const auto received=Json::parse(packet);
        if(nlohmann::json(received)!=nlohmann::json(expected))throw std::invalid_argument("Join proposal differs from prepared lobby/runtime");
        bool assigned=false,host=false;
        for(const auto& player:expected.at("players")){
            if(player.at("slot")==0)host=true;
            if(player.at("peer")==assignedPeer_){
                if(player.at("slot")==0)throw std::invalid_argument("Join cannot own host slot");
                assigned=true;
            }
        }
        if(!assigned||!host)throw std::invalid_argument("Original Join assignment required");
        proposal_=expected;
        auto ack=expected;ack["type"]="session-start-ack";return ack;
    }
    Json acceptCommit(const std::string& packet,const PreparedStartLobby& lobby,
            const PreparedStartRuntime& captured){
        if(!proposal_||committed_||packet.size()>65536)throw std::runtime_error("Acknowledged bounded Join commit required");
        const auto current=SessionStartBarrier::fromConfiguration(lobby,configuration_,captured).proposal();
        if(nlohmann::json(current)!=nlohmann::json(*proposal_))throw std::runtime_error("Join preparation changed before commit");
        auto expected=*proposal_;expected["type"]="session-start-commit";
        if(nlohmann::json(Json::parse(packet))!=nlohmann::json(expected))throw std::invalid_argument("Join commit differs from acknowledgment");
        committed_=true;return expected;
    }
    bool committed()const{return committed_;}
};
}
