#pragma once
#include "native_room_client.hpp"
#include "native_host_lobby_session.hpp"

namespace partyboard::net {
// Owns the correspondence between broker membership and actual RTC peers.
// The room worker owns HTTP. tick() only consumes events and pumps existing RTC
// setup; it never waits for an answer on the same queue that must deliver it.
class NativeRoomHostLobby {
    NativeHostLobbySession& session_;
    NativeRoomClient room_;
    std::map<std::string,std::string> bindings_;
    std::deque<std::string> waiting_;
    struct Pending {std::string broker,transport;bool offered=false;std::chrono::steady_clock::time_point deadline;};
    std::optional<Pending> pending_;
    std::deque<nlohmann::ordered_json> recovery_;
    std::string error_,message_;
    bool sealedRequested_=false,frozen_=false;
    void remove(const std::string& broker,bool notify){
        auto found=bindings_.find(broker);
        if(found!=bindings_.end()){session_.removeGuest(found->second);bindings_.erase(found);}
        std::erase(waiting_,broker);if(pending_&&pending_->broker==broker)pending_.reset();if(notify)room_.remove(broker);
    }
public:
    NativeRoomHostLobby(NativeHostLobbySession& session,NativeRoomAddress address):session_(session),room_(std::move(address),true){session_.allowGuestDepartures();}
    NativeRoomHostLobby(const NativeRoomHostLobby&)=delete;
    std::string joinLink()const{return room_.joinLink();}
    bool ready()const{return room_.ready()&&error_.empty();}
    bool sealed()const{return room_.sealed();}
    bool idle()const{return !pending_&&waiting_.empty();}
    std::string error()const{return error_.empty()?room_.error():error_;}
    const std::string& message()const{return message_;}
    void requestSeal(){if(!ready()||!idle())throw std::runtime_error("Wait for joining players before starting");if(!sealedRequested_){room_.seal();sealedRequested_=true;}}
    void freeze(){if(!sealed())throw std::runtime_error("Sealed room required before start");frozen_=true;}
    void tick(){
        if(!error_.empty())return;
        try{
            if(!room_.error().empty())throw std::runtime_error(room_.error());
            while(auto departed=session_.takeDeparture()){
                auto found=std::find_if(bindings_.begin(),bindings_.end(),[&](const auto& item){return item.second==*departed;});
                if(found!=bindings_.end()){const auto broker=found->first;bindings_.erase(found);if(pending_&&pending_->broker==broker)pending_.reset();room_.remove(broker);}
            }
            for(unsigned count=0;count<64;++count){auto event=room_.take();if(!event)break;
                const std::string type=event->at("type");
                if(type=="joined"){const std::string peer=event->at("peer");if(frozen_||sealedRequested_)room_.remove(peer);else waiting_.push_back(peer);}
                else if(type=="left"){if(!frozen_)remove(event->at("peer"),false);}
                else if(event->at("kind")=="recovery-answer"){
                    const auto found=bindings_.find(event->at("from"));if(!frozen_||found==bindings_.end()||recovery_.size()>=8)throw std::runtime_error("Unexpected assigned reconnect reply");
                    recovery_.push_back({{"peer",found->second},{"payload",event->at("payload")}});
                }else{
                    if(frozen_||event->at("kind")!="answer"||!pending_||event->at("from")!=pending_->broker||!pending_->offered)throw std::runtime_error("Unexpected guest connection reply");
                    session_.acceptReply(pending_->transport,event->at("payload"));pending_.reset();message_="Guest connected. Assets and shaders prepare automatically.";
                }
            }
            if(frozen_||!room_.ready()||!session_.prepared())return;
            if(pending_&&std::chrono::steady_clock::now()>=pending_->deadline){const auto peer=pending_->broker;remove(peer,true);message_="A guest did not finish connecting. They can reopen the lobby link.";}
            if(!pending_&&!waiting_.empty()&&!sealedRequested_){const auto broker=waiting_.front();waiting_.pop_front();const auto peer=session_.addGuest("Guest");bindings_[broker]=peer;pending_=Pending{broker,peer,false,std::chrono::steady_clock::now()+std::chrono::seconds(45)};}
            if(pending_&&!pending_->offered)if(const auto code=session_.invitation(pending_->transport)){room_.send(pending_->broker,"offer",*code);pending_->offered=true;}
        }catch(const std::exception& error){error_=error.what();}
    }
    void sendRecovery(const std::string& peer,const nlohmann::ordered_json& packet){
        const auto found=std::find_if(bindings_.begin(),bindings_.end(),[&](const auto& item){return item.second==peer;});
        if(!frozen_||found==bindings_.end())throw std::runtime_error("Original assigned room peer required for recovery");room_.send(found->first,"recovery-offer",packet.dump());
    }
    std::optional<nlohmann::ordered_json> takeRecovery(){if(recovery_.empty())return std::nullopt;auto value=std::move(recovery_.front());recovery_.pop_front();return value;}
};
}
