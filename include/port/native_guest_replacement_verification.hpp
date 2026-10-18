#pragma once
#include "port/native_paused_poll_reconciliation.hpp"
#include "port/native_paused_state_agreement.hpp"
#include <memory>

namespace partyboard::net {
// Caller supplies the already authenticated candidate route and original
// first-loss deadline. Verification never adopts channels or resumes input.
class NativeGuestReplacementVerification {
    RecoverySession identity_;
    std::string peer_,host_,nonce_;
    std::function<PausedCapture()> capture_;
    std::function<bool()> current_;
    std::function<uint64_t()> now_;
    std::function<bool(const nlohmann::ordered_json&)> send_;
    uint64_t deadline_;
    std::unique_ptr<NativePausedPollReconciliation> poll_;
    std::unique_ptr<NativePausedStateAgreement> state_;
    void advance(){
        if(poll_->complete()&&!state_)
            state_=std::make_unique<NativePausedStateAgreement>(identity_,peer_,host_,nonce_,capture_,current_,now_,deadline_,send_);
    }
public:
    NativeGuestReplacementVerification(RecoverablePadExchange& exchange,RecoverySession identity,
            std::string peer,std::string host,std::string nonce,
            std::function<PausedCapture()> capture,std::function<bool()> current,
            std::function<uint64_t()> now,uint64_t originalDeadline,
            std::function<bool(const nlohmann::ordered_json&)> send)
        :identity_(std::move(identity)),peer_(std::move(peer)),host_(std::move(host)),nonce_(std::move(nonce)),
         capture_(std::move(capture)),current_(std::move(current)),now_(std::move(now)),send_(std::move(send)),deadline_(originalDeadline){
        if(!capture_||!current_||!now_||!send_||!recoveryHex(nonce_,32)||deadline_>9007199254740991ULL)
            throw std::invalid_argument("Original replacement verification required");
        poll_=std::make_unique<NativePausedPollReconciliation>(exchange,identity_,peer_,host_,nonce_,
            [this,&exchange]()->std::optional<PausedCapture>{if(!exchange.pending())return std::nullopt;return capture_();},
            current_,now_,deadline_,send_);
    }
    void tick(){advance();if(state_)state_->tick();else poll_->tick();advance();}
    void receive(const std::string& boundHost,const nlohmann::ordered_json& packet){
        advance();if(state_)state_->receive(boundHost,packet);else poll_->receive(boundHost,packet);advance();
    }
    bool verified()const{return state_&&state_->complete();}
    NativeGuestReplacementVerification(const NativeGuestReplacementVerification&)=delete;
    NativeGuestReplacementVerification& operator=(const NativeGuestReplacementVerification&)=delete;
};
}
