#pragma once
#include "port/native_rtc_session_control.hpp"
#include "port/native_start_coordinator.hpp"

namespace partyboard::net {
// Game-thread bridge. Actual engine capture/input installation/resume remain
// with the runtime owner; this binds authenticated transport events to agreement.
class NativeHostStartControl {
    NativeRtcSessionControl& transport_;
    NativeStartCoordinator& coordinator_;
public:
    NativeHostStartControl(NativeRtcSessionControl& transport,NativeStartCoordinator& coordinator):
        transport_(transport),coordinator_(coordinator){}
    bool pumpPrepared(){
        try{
            while(auto message=transport_.take(NativeRtcControlChannel::Purpose::Preparation))
                if(!coordinator_.receivePrepared(message->route.peer,message->text)){coordinator_.cancel();return false;}
            return true;
        }catch(...){coordinator_.cancel();return false;}
    }
    bool pumpAcknowledgments(){
        try{
            while(auto message=transport_.take(NativeRtcControlChannel::Purpose::Start))
                if(!coordinator_.acknowledge(message->route.peer,message->text)){coordinator_.cancel();return false;}
            return true;
        }catch(...){coordinator_.cancel();return false;}
    }
    bool propose(const PreparedStartRuntime& captured,const nlohmann::ordered_json& hostReceipt,const std::string& localHost){
        try{
            auto proposal=coordinator_.begin(captured,hostReceipt);
            bool host=false;
            for(const auto& player:proposal.at("players")){
                const auto peer=player.at("peer").get<std::string>();
                if(peer==localHost){if(player.at("slot")!=0)throw std::invalid_argument("Original local host required");host=true;}
                else if(!transport_.connected(peer))throw std::runtime_error("Assigned native control channels unavailable");
            }
            if(!host)throw std::invalid_argument("Original local host absent");
            auto ack=proposal;ack["type"]="session-start-ack";
            if(!coordinator_.acknowledge(localHost,ack.dump()))throw std::runtime_error("Native host acknowledgment rejected");
            for(const auto& player:proposal.at("players")){
                const auto peer=player.at("peer").get<std::string>();
                if(peer!=localHost&&!transport_.send(peer,NativeRtcControlChannel::Purpose::Start,proposal.dump()))
                    throw std::runtime_error("Native start proposal delivery rejected");
            }
            return true;
        }catch(...){coordinator_.cancel();return false;}
    }
    bool sendCommit(const std::string& peer,const std::string& packet){
        return transport_.connected(peer)&&transport_.send(peer,NativeRtcControlChannel::Purpose::Start,packet);
    }
};
}
