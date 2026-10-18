#pragma once
#include "port/native_invitation.hpp"
#include "port/native_connection_route.hpp"
#include "port/native_transport_diagnostics.hpp"
#include <rtc/rtc.hpp>
#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <deque>
#include <sstream>

namespace partyboard::net {
// Initial Join transport only. A negotiated channel is not engine readiness,
// a Start commit, or authorization to replace an original recovery route.
class NativeGuestInvitation {
public:
    struct Message {std::string label;rtc::message_variant payload;};
private:
    struct State {
        std::mutex mutex;
        std::array<std::shared_ptr<rtc::DataChannel>,6> channels{};
        std::array<bool,6> claimed{};
        bool pipelineChannelDeclined=false;
        bool closed=false;
        std::string error;
        std::deque<Message> messages;
        size_t queuedBytes=0;
        NativeTransportDiagnostics diagnostics;
    };
    static constexpr std::array<const char*,6> labels{
        "partyboard-preparation-v1","partyboard-start-v1","partyboard-lobby-v1",
        "partyboard-input-v1","partyboard-recovery-v1","partyboard-assets-v1"};
    std::shared_ptr<State> state_=std::make_shared<State>();
    std::shared_ptr<rtc::PeerConnection> connection_;
    std::string peer_;
public:
    explicit NativeGuestInvitation(const std::string& invitation,rtc::Configuration configuration={},uint8_t acceptedChannels=0x3f) {
        if(!acceptedChannels||(acceptedChannels&~0x3f))throw std::invalid_argument("Bounded Join channel set required");
        const auto packet=receiveNativeInvitation(invitation,"offer");
        peer_=packet.at("playerId").get<std::string>();
        connection_=std::make_shared<rtc::PeerConnection>(std::move(configuration));
        const std::weak_ptr<State> weak=state_;
        connection_->onStateChange([weak](rtc::PeerConnection::State value){
            if(auto state=weak.lock()){std::ostringstream text;text<<value;state->diagnostics.record("connection-state",text.str());}
        });
        connection_->onIceStateChange([weak](rtc::PeerConnection::IceState value){
            if(auto state=weak.lock()){std::ostringstream text;text<<value;state->diagnostics.record("ice-state",text.str());}
        });
        connection_->onDataChannel([weak,acceptedChannels](std::shared_ptr<rtc::DataChannel> channel){
            if(auto state=weak.lock()){
                std::unique_lock lock(state->mutex);
                // Native Join currently uses its locally installed seed. Decline
                // the one recognized browser-only stream without invalidating
                // the original transport; all other unknown/duplicate channels
                // retain their existing rejection behavior.
                if(!state->closed&&channel->label()=="partyboard-pipelines-v1"&&!state->pipelineChannelDeclined){
                    state->pipelineChannelDeclined=true;
                    state->diagnostics.record("channel-declined",channel->label());
                    lock.unlock();channel->close();return;
                }
                size_t index=labels.size();
                for(size_t i=0;i<labels.size();++i)if(channel->label()==labels[i])index=i;
                if(state->closed||index==labels.size()||!(acceptedChannels&(1u<<index))||state->claimed[index]){
                    if(!state->closed)state->error="Unexpected or duplicate Join channel";
                    state->diagnostics.record("channel-rejected",channel->label());
                    lock.unlock();channel->close();return;
                }
                state->claimed[index]=true;
                lock.unlock(); // RTC callback registration can deliver queued data.
                // Install before publication: the host can send a manifest
                // immediately on open, before the game thread polls Join.
                const auto label=channel->label();
                state->diagnostics.record("channel-received",label);
                channel->onClosed([weak,label]{if(auto state=weak.lock())state->diagnostics.record("channel-closed",label);});
                channel->onError([weak,label](std::string error){if(auto state=weak.lock())state->diagnostics.record("channel-error",label+": "+error);});
                channel->onMessage([weak,label](rtc::message_variant message){if(auto state=weak.lock()){
                    std::lock_guard lock(state->mutex);if(state->closed||!state->error.empty())return;
                    const auto bytes=std::visit([](const auto& value){return value.size();},message);
                    if(bytes>262144||state->messages.size()>=256||bytes>4194304-state->queuedBytes){state->error="Join message bounds exceeded";state->diagnostics.record("mailbox-overflow",label);return;}
                    state->queuedBytes+=bytes;state->messages.push_back({label,std::move(message)});
                }});
                lock.lock();
                if(state->closed){lock.unlock();channel->resetCallbacks();channel->close();return;}
                state->channels[index]=std::move(channel);
            }else channel->close();
        });
        try{connection_->setRemoteDescription(rtc::Description(packet.at("description").at("sdp").get<std::string>(),"offer"));}
        catch(...){close();throw;}
    }
    NativeGuestInvitation(const NativeGuestInvitation&)=delete;
    NativeGuestInvitation& operator=(const NativeGuestInvitation&)=delete;
    const std::string& assignedPeer()const{return peer_;}
    NativeConnectionRoute connectionRoute()const{
        {std::lock_guard lock(state_->mutex);if(state_->closed)return NativeConnectionRoute::Closed;}
        return nativeConnectionRoute(*connection_);
    }
    std::optional<std::string> reply()const {
        {std::lock_guard lock(state_->mutex);if(state_->closed||!state_->error.empty())return std::nullopt;}
        if(connection_->gatheringState()!=rtc::PeerConnection::GatheringState::Complete)return std::nullopt;
        const auto description=connection_->localDescription();if(!description)return std::nullopt;
        return encodeNativeInvitation({{"version",1},{"playerId",peer_},{"description",{{"type","answer"},{"sdp",std::string(*description)}}}});
    }
    std::shared_ptr<rtc::DataChannel> channel(const std::string& label)const {
        std::lock_guard lock(state_->mutex);
        if(state_->closed||!state_->error.empty())throw std::runtime_error("Join transport unavailable");
        for(size_t i=0;i<labels.size();++i)if(label==labels[i])return state_->channels[i];
        throw std::invalid_argument("Unknown Join channel");
    }
    std::string error()const{std::lock_guard lock(state_->mutex);return state_->error;}
    nlohmann::ordered_json diagnosticState()const {
        nlohmann::ordered_json result;
        {std::lock_guard lock(state_->mutex);
         result={{"ownerClosed",state_->closed},{"error",state_->error},{"queuedMessages",state_->messages.size()},{"queuedBytes",state_->queuedBytes}};
         result["channels"]=nlohmann::ordered_json::array();
         for(size_t i=0;i<labels.size();++i)result["channels"].push_back({{"label",labels[i]},
             {"present",bool(state_->channels[i])},{"open",state_->channels[i]&&state_->channels[i]->isOpen()}});}
        std::ostringstream connection,ice;connection<<connection_->state();ice<<connection_->iceState();
        result["connectionState"]=connection.str();result["iceState"]=ice.str();
        result["history"]=state_->diagnostics.snapshot();return result;
    }
    std::optional<Message> takeMessage(){
        std::lock_guard lock(state_->mutex);
        if(state_->closed||!state_->error.empty()||state_->messages.empty())return std::nullopt;
        auto result=std::move(state_->messages.front());state_->messages.pop_front();
        state_->queuedBytes-=std::visit([](const auto& value){return value.size();},result.payload);
        return result;
    }
    void close(){
        std::array<std::shared_ptr<rtc::DataChannel>,6> channels;
        {std::lock_guard lock(state_->mutex);if(state_->closed)return;state_->closed=true;state_->diagnostics.record("owner-close");channels.swap(state_->channels);state_->messages.clear();state_->queuedBytes=0;}
        if(connection_)connection_->resetCallbacks();
        for(auto& channel:channels)if(channel){channel->resetCallbacks();channel->close();}
        if(connection_)connection_->close();
    }
    ~NativeGuestInvitation(){close();}
};
}
