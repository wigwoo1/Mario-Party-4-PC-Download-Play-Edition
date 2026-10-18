#pragma once
#include "port/native_guest_invitation.hpp"
#include "port/native_guest_asset_transfer.hpp"
#include "port/native_session_configuration.hpp"
#include "port/recovery_identity.hpp"

namespace partyboard::net {
// Game-thread Join owner. The caller's private staging stream outlives this
// owner. Retained control packets are handed to readiness/Start coordinators;
// asset completion alone never starts the engine.
class NativeGuestJoinSession {
    NativeGuestInvitation invitation_;
    std::ostream& staging_;
    std::unique_ptr<NativeGuestAssetTransfer> assets_;
    std::deque<NativeGuestInvitation::Message> controls_;
    size_t controlBytes_=0;
    bool closed_=false;
    std::string error_;
    std::optional<nlohmann::ordered_json> configuration_;
    std::unique_ptr<RecoveryIdentity> recoveryIdentity_;
public:
    NativeGuestJoinSession(const std::string& code,std::ostream& staging,rtc::Configuration configuration={})
        :invitation_(code,std::move(configuration)),staging_(staging){}
    NativeGuestJoinSession(const NativeGuestJoinSession&)=delete;
    NativeGuestJoinSession& operator=(const NativeGuestJoinSession&)=delete;
    NativeGuestInvitation& transport(){return invitation_;}
    bool usesStaging(const std::ostream& stream)const{return &staging_==&stream;}
    void pump(){
        if(closed_||!error_.empty())return;
        try{
            if(!invitation_.error().empty())throw std::runtime_error(invitation_.error());
            for(unsigned i=0;i<64;++i){
                auto message=invitation_.takeMessage();if(!message)break;
                if(message->label=="partyboard-assets-v1"){
                    if(!assets_)assets_=std::make_unique<NativeGuestAssetTransfer>(staging_,invitation_.channel(message->label));
                    assets_->submit(std::move(message->payload));
                }else{
                    const auto bytes=std::visit([](const auto& value){return value.size();},message->payload);
                    if(controls_.size()>=128||bytes>4194304-controlBytes_)throw std::runtime_error("Join retained control bounds exceeded");
                    controlBytes_+=bytes;controls_.push_back(std::move(*message));
                }
            }
            if(assets_&&!assets_->error().empty())throw std::runtime_error(assets_->error());
        }catch(const std::exception& error){
            error_=error.what();if(assets_)assets_->close();controls_.clear();controlBytes_=0;
        }
    }
    std::optional<NativeGuestInvitation::Message> takeControl(){
        if(closed_||!error_.empty()||controls_.empty())return std::nullopt;
        auto result=std::move(controls_.front());controls_.pop_front();
        controlBytes_-=std::visit([](const auto& value){return value.size();},result.payload);return result;
    }
    // Preparation may finish after Lobby snapshots arrive on another channel.
    // Consume only its packets without losing the retained host snapshot.
    std::optional<NativeGuestInvitation::Message> takeControl(const std::string& label){
        if(closed_||!error_.empty())return std::nullopt;
        const auto found=std::find_if(controls_.begin(),controls_.end(),[&](const auto& message){return message.label==label;});
        if(found==controls_.end())return std::nullopt;
        auto result=std::move(*found);controls_.erase(found);
        controlBytes_-=std::visit([](const auto& value){return value.size();},result.payload);return result;
    }
    bool assetsComplete()const{return !closed_&&error_.empty()&&assets_&&assets_->complete();}
    void retainConfiguration(const std::string& text,const std::string& measuredBuildId){
        if(!assetsComplete()||configuration_)throw std::runtime_error("Single verified Join configuration required");
        configuration_=receiveNativeSessionConfiguration(text,assets_->verifiedAssetId(),measuredBuildId);
    }
    const nlohmann::ordered_json& configuration()const{
        if(closed_||!error_.empty()||!configuration_)throw std::runtime_error("Retained Join configuration unavailable");return *configuration_;
    }
    void retainOriginalRecoveryCredential(const NativeGuestInvitation::Message& message){
        const auto& configured=configuration();
        if(recoveryIdentity_||message.label!="partyboard-preparation-v1")throw std::runtime_error("Original assigned Preparation credential required");
        const auto* text=std::get_if<std::string>(&message.payload);
        if(!text||text->empty()||text->size()>2048)throw std::invalid_argument("Bounded original recovery credential required");
        const auto packet=nlohmann::ordered_json::parse(*text);
        if(packet.at("session")!=configured.at("session")||packet.at("assetId")!=configured.at("assetId")||packet.at("peer")!=invitation_.assignedPeer())
            throw std::invalid_argument("Recovery credential differs from original Join identity");
        recoveryIdentity_=std::make_unique<RecoveryIdentity>(packet);
    }
    RecoveryIdentity& recoveryIdentity(){
        (void)configuration();if(!recoveryIdentity_)throw std::runtime_error("Original Join recovery credential unavailable");return *recoveryIdentity_;
    }
    uint64_t receivedBytes()const{return assets_?assets_->receivedBytes():0;}
    const std::string& error()const{return error_;}
    void close(){
        if(closed_)return;closed_=true;
        if(assets_){assets_->close();assets_.reset();}
        controls_.clear();controlBytes_=0;recoveryIdentity_.reset();configuration_.reset();invitation_.close();
    }
    ~NativeGuestJoinSession(){close();}
};
}
