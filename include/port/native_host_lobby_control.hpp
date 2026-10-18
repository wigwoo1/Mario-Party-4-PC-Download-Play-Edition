#pragma once
#include "port/native_lobby_state.hpp"
#include "port/native_rtc_session_control.hpp"
#include <set>
#include <functional>
namespace partyboard::net {
// Game-thread lobby signaling owner. Caller joins the local host first and
// supplies assigned transport peers. Freeze before committed game execution;
// recovery then owns membership/loss and must not call lobby detach.
class NativeHostLobbyControl {
    NativeLobbyState& state_;
    NativeRtcSessionControl& transport_;
    std::set<std::string> peers_;
    std::set<std::string> connected_;
    bool frozen_=false,failed_=false;
    std::string error_;
    std::function<bool(const std::string&)> preparationGate_;
    std::map<std::string,nlohmann::ordered_json> deferred_;
public:
    const std::string& error()const{return error_;}
    NativeHostLobbyControl(NativeLobbyState& state,NativeRtcSessionControl& transport):state_(state),transport_(transport){}
    void setPreparationGate(std::function<bool(const std::string&)> gate){
        if(frozen_||failed_)throw std::runtime_error("Native readiness gate unavailable");
        preparationGate_=std::move(gate);
    }
    void attachAssigned(const std::string& peer,const std::string& name){
        if(frozen_||failed_||peers_.contains(peer)||!transport_.hasPeer(peer))throw std::invalid_argument("Assigned native lobby transport required");
        state_.join(peer,name);peers_.insert(peer);
    }
    bool broadcast(){
        if(failed_)return false;
        const auto packet=nlohmann::ordered_json{{"type","lobby-snapshot"},{"version",1},{"snapshot",state_.snapshot()}}.dump();
        for(const auto& peer:peers_)if(transport_.connected(peer)&&!transport_.send(peer,NativeRtcControlChannel::Purpose::Lobby,packet)){error_="Native lobby snapshot delivery failed for "+peer;failed_=true;return false;}
        return true;
    }
    bool pump(){
        if(failed_)return false;
        if(frozen_)return true;
        try{
            for(const auto& peer:peers_){
                if(transport_.connected(peer))connected_.insert(peer);
                else if(connected_.contains(peer))throw std::runtime_error("Native lobby control connection lost for "+peer);
            }
            while(auto message=transport_.take(NativeRtcControlChannel::Purpose::Lobby)){
                const auto& peer=message->route.peer;
                if(!peers_.contains(peer))throw std::invalid_argument("Unassigned lobby report");
                const auto packet=nlohmann::ordered_json::parse(message->text);
                if(message->text.size()>2048||packet.at("type")!="lobby-report"||packet.at("version")!=1)
                    throw std::invalid_argument("Invalid native lobby report envelope");
                const auto& report=packet.at("report");
                auto validation=state_;
                if(auto pending=deferred_.find(peer);pending!=deferred_.end())validation.report(peer,pending->second);
                validation.report(peer,report);
                if(preparationGate_&&(report.at("shaders")==true||report.at("ready")==true)&&!preparationGate_(peer)){
                    deferred_[peer]=report;continue;
                }
                deferred_.erase(peer);state_.report(peer,report);
                if(!broadcast())return false;
            }
            for(auto pending=deferred_.begin();pending!=deferred_.end();){
                if(preparationGate_&&!preparationGate_(pending->first)){++pending;continue;}
                state_.report(pending->first,pending->second);
                pending=deferred_.erase(pending);
                if(!broadcast())return false;
            }
            return true;
        }catch(const std::exception& error){error_=error.what();failed_=true;return false;}
        catch(...){error_="Unknown native lobby control failure";failed_=true;return false;} // Runtime cancels/tears down on false.
    }
    bool reportLocal(const std::string& peer,const nlohmann::ordered_json& report){
        if(frozen_||failed_)throw std::runtime_error("Native lobby reports unavailable");
        state_.report(peer,report);return broadcast();
    }
    void detachAssigned(const std::string& peer){
        if(frozen_||failed_)throw std::runtime_error("Native lobby membership is frozen or failed");
        if(!peers_.erase(peer))return;connected_.erase(peer);deferred_.erase(peer);
        state_.leave(peer);if(!broadcast())throw std::runtime_error("Native guest departure publication failed");
    }
    nlohmann::ordered_json freezeMembership(){
        if(failed_)throw std::runtime_error("Failed native lobby cannot freeze");
        if(!deferred_.empty())throw std::runtime_error("Native prepared receipts pending");
        for(const auto& peer:peers_)if(!transport_.connected(peer)){failed_=true;throw std::runtime_error("Native lobby peer unavailable at freeze");}
        frozen_=true;return state_.snapshot();
    }
};
}
