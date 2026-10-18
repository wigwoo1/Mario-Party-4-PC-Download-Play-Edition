#pragma once
#include "port/recoverable_pad_exchange.hpp"
#include <functional>

namespace partyboard::net {
// Game-thread protocol on an already authenticated original peer's channel.
// Capture returns null only while a validated single frame is executing. This
// protocol never resumes ordinary input or changes the original deadline.
class NativePausedPollReconciliation {
public:
    using Json=nlohmann::ordered_json;
    using Capture=std::function<std::optional<PausedCapture>()>;
private:
    RecoverablePadExchange& exchange_;
    RecoverySession identity_;
    std::string peer_,remotePeer_,nonce_;
    Capture capture_;
    std::function<bool()> current_;
    std::function<uint64_t()> now_;
    std::function<bool(const Json&)> send_;
    uint64_t deadline_,epoch_,revision_;
    Json initial_,local_,receipt_;
    std::optional<Json> remote_,remoteAck_,target_;
    bool ready_=false,replaying_=false,done_=false,failed_=false;
    static bool same(const Json& left,const Json& right){return nlohmann::json(left)==nlohmann::json(right);}
    static bool matches(Json left,Json right){left.erase("reporter");right.erase("reporter");return same(left,right);}
    void valid(){
        if(failed_||now_()>=deadline_||!current_()||exchange_.epoch()!=epoch_||exchange_.revision()!=revision_||
           exchange_.tick(now_())!=ReconnectPolicy::State::Paused)
            throw std::runtime_error("Original paused reconciliation expired or changed");
    }
    void send(const Json& packet){if(!send_(packet))throw std::runtime_error("Paused reconciliation delivery failed");}
    void check(){
        valid();if(!remote_)return;
        const auto captured=capture_();if(!captured){if(replaying_)return;throw std::runtime_error("Actual paused capture unavailable");}
        const auto actual=nativePausedStateReport(identity_,*captured,peer_,nonce_);
        if(target_){
            if(actual.at("frame")!=target_->at("frame")||actual.at("inputFrame")!=target_->at("inputFrame"))return;
            if(!matches(actual,*target_))throw std::runtime_error("Replayed actual paused state differs");
            ready_=true;
        }else if(!same(actual,initial_))throw std::runtime_error("Original paused state drifted");
        if(!ready_)return;
        send({{"type","paused-poll-ack"},{"version",1},{"nonce",nonce_},{"peer",peer_},{"accepted",*remote_},{"state",actual}});
        if(remoteAck_){if(!matches(remoteAck_->at("state"),actual))throw std::runtime_error("Reconciled remote state differs");done_=true;}
    }
    template<class F>void guard(F action){try{action();}catch(...){failed_=true;exchange_.cancel();throw;}}
public:
    NativePausedPollReconciliation(RecoverablePadExchange& exchange,const RecoverySession& identity,
        std::string peer,std::string remotePeer,std::string nonce,Capture capture,
        std::function<bool()> current,std::function<uint64_t()> now,uint64_t deadline,
        std::function<bool(const Json&)> send)
        :exchange_(exchange),identity_(identity),peer_(std::move(peer)),remotePeer_(std::move(remotePeer)),nonce_(std::move(nonce)),
         capture_(std::move(capture)),current_(std::move(current)),now_(std::move(now)),send_(std::move(send)),
         deadline_(deadline),epoch_(exchange.epoch()),revision_(exchange.revision()){
        if(!capture_||!current_||!now_||!send_||peer_==remotePeer_||!exchange.originalSessionMatches(identity))
            throw std::invalid_argument("Original authenticated reconciliation required");
        bool assigned=false;for(const auto& player:identity.players)assigned|=player.peer==remotePeer_;
        if(!assigned)throw std::invalid_argument("Assigned remote reconciliation peer required");
        valid();const auto captured=capture_();if(!captured)throw std::runtime_error("Suspended original poll required");
        initial_=nativePausedStateReport(identity_,*captured,peer_,nonce_);
        receipt_=nullptr;
        if(const auto& completed=exchange.completedPoll();completed)
            receipt_={{"frame",std::to_string(completed->frame)},{"checkpoint",completed->state.checkpoint},
                {"frand",completed->state.frand},{"rand8",completed->state.rand8},{"pads",completed->pads}};
        local_={{"type","paused-poll-report"},{"version",1},{"nonce",nonce_},{"peer",peer_},{"report",initial_},{"receipt",receipt_}};
    }
    const Json& report()const{return local_;}
    bool complete()const noexcept{return done_&&!failed_;}
    void tick(){if(done_||failed_)return;guard([&]{valid();if(!remoteAck_)send(local_);check();});}
    void receive(const std::string& boundPeer,const Json& packet){
        if(done_||failed_)throw std::runtime_error("Reconciliation already completed or failed");
        guard([&]{
            valid();
            // The next phase can arrive before this endpoint's final poll ack.
            // Ignore it here; strict agreement retransmits and validates it.
            const auto earlyType=packet.value("type","");
            if(boundPeer==remotePeer_&&packet.value("nonce","")==nonce_&&
               ((earlyType=="paused-state-report"&&packet.value("reporter","")==remotePeer_)||
                (earlyType=="paused-state-ack"&&packet.value("peer","")==remotePeer_)))return;
            if(boundPeer!=remotePeer_||!packet.is_object()||packet.value("version",0)!=1||
                packet.value("nonce","")!=nonce_||packet.value("peer","")!=remotePeer_||packet.size()!=6)
                throw std::runtime_error("Bound original reconciliation packet required");
            const auto type=packet.value("type","");
            if(type=="paused-poll-report"){
                if(packet.at("report").value("reporter","")!=remotePeer_)throw std::runtime_error("Assigned report author required");
                if(remote_){if(!same(*remote_,packet))throw std::runtime_error("Original report changed");check();return;}
                remote_=packet;
                const auto& report=packet.at("report");
                if(matches(report,initial_))ready_=true;
                else{
                    const auto captured=capture_();if(!captured)throw std::runtime_error("Original frozen capture unavailable");
                    if(captured->inputFrame!=UINT64_MAX&&report.at("inputFrame")==std::to_string(captured->inputFrame+1)){
                        exchange_.preparePausedReplay(remotePeer_,nonce_,report,packet.at("receipt"),captured->missingOwner,
                            captured->frame,{captured->checkpoint,captured->frand,captured->rand8},now_());
                        target_=report;replaying_=true;
                    }else if(captured->inputFrame>0&&report.at("inputFrame")==std::to_string(captured->inputFrame-1)){
                        auto common=report;for(const auto* key:{"frame","inputFrame","checkpoint","frand","rand8"})common[key]=initial_.at(key);
                        if(!matches(common,initial_)||captured->frame==0||report.at("frame")!=captured->frame-1||
                            receipt_.is_null()||receipt_.at("frame")!=report.at("inputFrame"))
                            throw std::runtime_error("Ahead receipt does not match original suspended poll");
                        for(const auto* key:{"checkpoint","frand","rand8"})if(receipt_.at(key)!=report.at(key))
                            throw std::runtime_error("Ahead receipt checkpoint or RNG differs");
                        ready_=true;
                    }else throw std::runtime_error("Paused polls differ by more than one frame");
                }
            }else if(type=="paused-poll-ack"){
                if(!same(packet.at("accepted"),local_)||packet.at("state").value("reporter","")!=remotePeer_)
                    throw std::runtime_error("Original reconciliation acknowledgment required");
                if(remoteAck_&&!same(*remoteAck_,packet))throw std::runtime_error("Original acknowledgment changed");
                remoteAck_=packet;
            }else throw std::runtime_error("Unexpected reconciliation packet");
            check();
        });
    }
};
}
