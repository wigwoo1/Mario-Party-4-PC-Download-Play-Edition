#pragma once
#include "port/native_guest_input_loss.hpp"
#include "port/native_paused_poll_reconciliation.hpp"
#include <memory>
#include <set>

namespace partyboard::net {
// Healthy original host route only. Replacement authentication is owned by
// the invitation/session coordinator. No receipt alone authorizes resume.
class NativeGuestRecoveryControl {
    using Json=nlohmann::ordered_json;
    RecoverablePadExchange& exchange_;
    RecoverySession identity_;
    std::string peer_,host_,nonce_;
    std::function<uint64_t()> now_;
    std::function<PausedCapture(uint32_t)> capture_;
    std::function<bool()> ready_;
    std::function<bool(const Json&)> send_;
    std::unique_ptr<NativePausedPollReconciliation> alignment_;
    std::optional<Json> proposal_;
    std::set<std::string> completed_;
    uint64_t epoch_=0,revision_=0,nextTick_=0;
    uint32_t owner_=0;
    std::string lastType_;
    void remember(){if(completed_.size()>=16)completed_.erase(completed_.begin());completed_.insert(nonce_);}
    void finishAlignment(){if(alignment_&&alignment_->complete()){remember();alignment_.reset();}}
public:
    NativeGuestRecoveryControl(RecoverablePadExchange& exchange,RecoverySession identity,std::string peer,std::string host,
            std::function<uint64_t()> now,std::function<PausedCapture(uint32_t)> capture,
            std::function<bool()> ready,std::function<bool(const Json&)> send)
        :exchange_(exchange),identity_(std::move(identity)),peer_(std::move(peer)),host_(std::move(host)),
         now_(std::move(now)),capture_(std::move(capture)),ready_(std::move(ready)),send_(std::move(send)){
        if(!now_||!capture_||!ready_||!send_||!exchange_.originalSessionMatches(identity_))
            throw std::invalid_argument("Original guest recovery owner required");
    }
    void receive(const std::string& boundHost,const Json& packet){
        if(boundHost!=host_||!ready_())throw std::runtime_error("Original open host recovery route required");
        const auto type=packet.value("type","");
        lastType_=type;
        if(type=="session-input-loss"){
            const auto owner=receiveNativeGuestInputLoss(packet,identity_,peer_,host_);
            if(!exchange_.missingSlots()[owner-1]&&!exchange_.disconnected(owner,now_()))
                throw std::runtime_error("Original guest pause rejected");
            owner_=owner;return;
        }
        if(type=="paused-poll-report"||type=="paused-poll-ack"||type=="paused-state-report"||type=="paused-state-ack"){
            if(packet.value("peer",packet.value("reporter",""))!=host_)throw std::runtime_error("Assigned host report required");
            if(completed_.contains(packet.value("nonce","")))return;
            if(!alignment_)throw std::runtime_error("Current guest alignment required");
            alignment_->receive(host_,packet);finishAlignment();return;
        }
        if(type=="session-poll-align"){
            if(alignment_||proposal_||packet.size()!=6||packet.value("version",0)!=1||
               packet.value("session","")!=identity_.session||packet.value("buildId","")!=identity_.buildId||
               packet.value("assetId","")!=identity_.assetId||!recoveryHex(packet.value("nonce",""),32)||
               completed_.contains(packet.value("nonce","")))throw std::runtime_error("Fresh original guest alignment required");
            const auto captured=capture_(owner_);epoch_=captured.epoch;revision_=captured.revision;nonce_=packet.at("nonce");
            alignment_=std::make_unique<NativePausedPollReconciliation>(exchange_,identity_,peer_,host_,nonce_,
                [this]()->std::optional<PausedCapture>{if(!exchange_.pending())return std::nullopt;return capture_(owner_);},
                [this]{return ready_()&&exchange_.epoch()==epoch_&&exchange_.revision()==revision_;},now_,
                now_()+exchange_.remainingRecoveryMs(now_()),send_);nextTick_=0;return;
        }
        if(type=="session-recovery-proposal"){
            if(alignment_||proposal_)throw std::runtime_error("Prior guest recovery phase pending");
            const auto captured=capture_(owner_);
            const auto expected=recoveryProposal(identity_,captured,host_,packet.value("nonce",""));
            if(nlohmann::json(packet)!=nlohmann::json(expected))throw std::runtime_error("Actual guest paused proposal differs");
            Json ack={{"type","session-recovery-ack"},{"version",1},{"peer",peer_},{"accepted",expected}};
            if(!send_(ack))throw std::runtime_error("Guest recovery acknowledgment delivery failed");
            proposal_=expected;epoch_=captured.epoch;revision_=captured.revision;return;
        }
        if(type=="session-recovery-commit"){
            if(!proposal_)throw std::runtime_error("Original acknowledged guest proposal required");
            const auto captured=capture_(owner_);
            if(!exchange_.resumeCommitted(identity_,peer_,host_,*proposal_,packet,epoch_,revision_,captured.frame,
                    {captured.checkpoint,captured.frand,captured.rand8},ready_(),now_()))
                throw std::runtime_error("Original all-player guest recovery commit rejected");
            proposal_.reset();owner_=0;return;
        }
        throw std::runtime_error("Unexpected guest recovery control");
    }
    void tick(){if(alignment_&&now_()>=nextTick_){alignment_->tick();nextTick_=now_()+20;finishAlignment();}}
    // Called by the actual verified replacement owner; still no resume authority.
    void verifiedLocalReplacement(const std::string& nonce){
        if(alignment_||proposal_||!recoveryHex(nonce,32)||exchange_.tick(now_())!=ReconnectPolicy::State::Paused||
           !exchange_.missingSlots()[0])throw std::runtime_error("Original local paused replacement required");
        owner_=1;nonce_=nonce;remember();
    }
    Json status()const{return {{"owner",owner_},{"alignment",bool(alignment_)},{"proposal",bool(proposal_)},
        {"nonce",nonce_},{"lastType",lastType_},{"epoch",exchange_.epoch()},{"revision",exchange_.revision()}};}
};
}
