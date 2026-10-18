#pragma once
#include "port/native_guest_invitation.hpp"
#include "port/recovery_identity.hpp"

namespace partyboard::net {
// Retain the original credential and original first-loss deadline. These two
// channels are candidates until actual paused-state verification/adoption;
// this owner never installs input or resumes the engine.
class NativeGuestReplacement {
    RecoveryIdentity& identity_;
    std::unique_ptr<NativeGuestInvitation> transport_;
    uint64_t deadline_;
    uint64_t lastNow_;
    std::string nonce_;
    bool answered_=false,closed_=false;
public:
    NativeGuestReplacement(RecoveryIdentity& original,const nlohmann::ordered_json& offer,
            uint64_t now,uint64_t originalDeadline,rtc::Configuration configuration={})
        :identity_(original),deadline_(originalDeadline),lastNow_(now){
        if(deadline_>9007199254740991ULL||now>=deadline_||!identity_.verify(offer,"offer"))throw std::runtime_error("Live original signed replacement offer required");
        nonce_=offer.at("nonce");
        const auto invitation=encodeNativeInvitation({{"version",1},{"playerId",offer.at("peer")},{"description",offer.at("description")}});
        transport_=std::make_unique<NativeGuestInvitation>(invitation,std::move(configuration),0x18);
    }
    void check(uint64_t now){
        if(closed_||now<lastNow_||now>=deadline_){close();throw std::runtime_error("Original guest replacement expired, clock reversed or closed");}
        lastNow_=now;
        if(!transport_->error().empty()){const auto error=transport_->error();close();throw std::runtime_error(error);}
    }
    std::optional<nlohmann::ordered_json> answer(uint64_t now){
        check(now);if(answered_)throw std::runtime_error("Guest replacement answer already published");
        const auto reply=transport_->reply();if(!reply)return std::nullopt;
        const auto packet=receiveNativeInvitation(*reply,"answer");
        auto signedAnswer=identity_.sign(packet.at("description"),nonce_);answered_=true;return signedAnswer;
    }
    bool ready(uint64_t now){check(now);const auto input=transport_->channel("partyboard-input-v1"),recovery=transport_->channel("partyboard-recovery-v1");
        return answered_&&input&&input->isOpen()&&recovery&&recovery->isOpen();}
    const std::string& nonce()const{return nonce_;}
    std::shared_ptr<rtc::DataChannel> input(uint64_t now){if(!ready(now))throw std::runtime_error("Authenticated guest replacement not open");return transport_->channel("partyboard-input-v1");}
    std::shared_ptr<rtc::DataChannel> recovery(uint64_t now){if(!ready(now))throw std::runtime_error("Authenticated guest replacement not open");return transport_->channel("partyboard-recovery-v1");}
    std::optional<NativeGuestInvitation::Message> takeMessage(uint64_t now){check(now);return transport_->takeMessage();}
    // Caller must prove paused-state verification before installing this route.
    std::unique_ptr<NativeGuestInvitation> releaseCandidate(uint64_t now){
        if(!ready(now))throw std::runtime_error("Open authenticated candidate required");
        closed_=true;return std::move(transport_);
    }
    void close(){if(closed_)return;closed_=true;if(transport_)transport_->close();}
    ~NativeGuestReplacement(){close();}
};
}
