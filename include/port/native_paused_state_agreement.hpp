#pragma once
#include "port/paused_state.hpp"
#include <functional>
#include <optional>

namespace partyboard::net {
// Original authenticated channel only. Completion proves two-sided paused
// state equality; caller still needs every assigned peer's recovery commit.
class NativePausedStateAgreement {
public:
    using Json=nlohmann::ordered_json;
private:
    std::string peer_,remotePeer_,nonce_;
    std::function<Json()> capture_;
    std::function<bool()> current_;
    std::function<uint64_t()> now_;
    std::function<bool(const Json&)> send_;
    uint64_t deadline_;
    Json local_,remote_;
    bool report_=false,ack_=false,failed_=false;
    static bool same(const Json& a,const Json& b){return nlohmann::json(a)==nlohmann::json(b);}
    void valid(){if(failed_||now_()>=deadline_||!current_()||!same(capture_(),local_))throw std::runtime_error("Original paused state agreement changed or expired");}
    void send(const Json& packet){if(!send_(packet))throw std::runtime_error("Paused state delivery failed");}
public:
    NativePausedStateAgreement(const RecoverySession& identity,std::string peer,std::string remotePeer,std::string nonce,
        std::function<PausedCapture()> capture,std::function<bool()> current,std::function<uint64_t()> now,
        uint64_t deadline,std::function<bool(const Json&)> send)
        :peer_(std::move(peer)),remotePeer_(std::move(remotePeer)),nonce_(std::move(nonce)),current_(std::move(current)),
         now_(std::move(now)),send_(std::move(send)),deadline_(deadline){
        if(!capture||!current_||!now_||!send_||peer_==remotePeer_)throw std::invalid_argument("Owned live paused agreement required");
        bool assigned=false;for(const auto& player:identity.players)assigned|=player.peer==remotePeer_;
        if(!assigned)throw std::invalid_argument("Original assigned peer required");
        capture_=[identity,peer=peer_,nonce=nonce_,capture=std::move(capture)]{return nativePausedStateReport(identity,capture(),peer,nonce);};
        local_=capture_();remote_=local_;remote_["reporter"]=remotePeer_;valid();
    }
    bool complete()const{return report_&&ack_&&!failed_;}
    const Json& report()const{return local_;}
    void tick(){try{valid();if(!ack_)send(local_);}catch(...){failed_=true;throw;}}
    void receive(const std::string& boundPeer,const Json& packet){try{
        valid();if(boundPeer!=remotePeer_||!packet.is_object())throw std::runtime_error("Bound original peer required");
        const auto type=packet.value("type","");
        if((type=="paused-poll-report"||type=="paused-poll-ack")&&packet.value("nonce","")==nonce_&&packet.value("peer","")==remotePeer_)return;
        if(type=="paused-state-report"){
            if(!same(packet,remote_))throw std::runtime_error("Remote paused state differs");report_=true;
            send({{"type","paused-state-ack"},{"version",1},{"nonce",nonce_},{"peer",peer_},{"accepted",packet}});
        }else if(type=="paused-state-ack"){
            if(packet.size()!=5||packet.value("version",0)!=1||packet.value("nonce","")!=nonce_||
               packet.value("peer","")!=remotePeer_||!same(packet.at("accepted"),local_))
                throw std::runtime_error("Exact paused acknowledgment required");
            ack_=true;
        }else throw std::runtime_error("Unexpected paused agreement packet");
    }catch(...){failed_=true;throw;}}
};
}
