#pragma once
#include <rtc/rtc.hpp>
#include <nlohmann/json.hpp>
#include <functional>
#include <array>
#include <optional>
#include "port/native_router_connection.hpp"

namespace partyboard::net {
// Game-thread owner. Signed descriptions use the retained original identity;
// open channels alone do not authorize recovery or resume simulation.
class NativeHostReplacement {
    std::shared_ptr<rtc::PeerConnection> connection_;
    NativeRouterConnectionMapping router_;
    std::array<std::shared_ptr<rtc::DataChannel>,2> channels_;
    std::function<nlohmann::ordered_json(const nlohmann::ordered_json&)> sign_;
    std::function<bool(const nlohmann::ordered_json&)> verify_;
    uint64_t deadline_;
    bool offered_=false,answered_=false,closed_=false;
    std::string nonce_;
public:
    NativeHostReplacement(uint64_t deadline,
        std::function<nlohmann::ordered_json(const nlohmann::ordered_json&)> sign,
        std::function<bool(const nlohmann::ordered_json&)> verify,rtc::Configuration configuration={},std::shared_ptr<NativeRouterWorker> router={})
        :router_(std::move(router)),sign_(std::move(sign)),verify_(std::move(verify)),deadline_(deadline){
        if(!sign_||!verify_)throw std::invalid_argument("Original replacement identity callbacks required");
        connection_=std::make_shared<rtc::PeerConnection>(std::move(configuration));
        try{
            channels_[0]=connection_->createDataChannel("partyboard-input-v1");
            channels_[1]=connection_->createDataChannel("partyboard-recovery-v1");
        }catch(...){close();throw;}
    }
    NativeHostReplacement(const NativeHostReplacement&)=delete;
    NativeHostReplacement& operator=(const NativeHostReplacement&)=delete;
    void check(uint64_t now){if(closed_||now>=deadline_){close();throw std::runtime_error("Native replacement expired or closed");}}
    std::optional<nlohmann::ordered_json> offer(uint64_t now){
        check(now);if(offered_)throw std::runtime_error("Native replacement offer already published");
        if(connection_->gatheringState()!=rtc::PeerConnection::GatheringState::Complete)return std::nullopt;
        const auto description=connection_->localDescription();if(!description)return std::nullopt;
        const auto mapped=router_.description(*description,NativeRouterTicket::NativeRouterWorkerClock());if(!mapped)return {};
        auto signedOffer=sign_({{"type","offer"},{"sdp",*mapped}});nonce_=signedOffer.at("nonce").get<std::string>();offered_=true;return signedOffer;
    }
    void acceptAnswer(const nlohmann::ordered_json& answer,uint64_t now){
        check(now);if(!offered_||answered_||!verify_(answer))throw std::runtime_error("Original signed replacement answer required");
        try{connection_->setRemoteDescription(rtc::Description(answer.at("description").at("sdp").get<std::string>(),"answer"));answered_=true;}
        catch(...){close();throw;}
    }
    bool ready(uint64_t now){check(now);return answered_&&channels_[0]->isOpen()&&channels_[1]->isOpen();}
    const std::string& nonce()const{if(!offered_)throw std::runtime_error("Published original offer required");return nonce_;}
    std::shared_ptr<rtc::DataChannel> input(uint64_t now){if(!ready(now))throw std::runtime_error("Open authenticated replacement required");return channels_[0];}
    std::shared_ptr<rtc::DataChannel> recovery(uint64_t now){if(!ready(now))throw std::runtime_error("Open authenticated replacement required");return channels_[1];}
    void close(){if(closed_)return;closed_=true;router_.close();for(const auto& channel:channels_)if(channel)channel->close();if(connection_){connection_->resetCallbacks();connection_->close();}}
    ~NativeHostReplacement(){close();}
};
}
