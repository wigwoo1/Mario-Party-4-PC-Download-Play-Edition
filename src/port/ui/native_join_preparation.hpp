#pragma once
#include "native_join_lobby.hpp"
#include <port/native_guest_staging_file.hpp>
#include <port/native_guest_game_input.hpp>

namespace partyboard::ui {
// Caller keeps the document and staging alive through stopped cleanup.
// This controller prepares the actual engine; Start never resumes it here.
class NativeJoinPreparation {
    NativeJoinLobby& document_;
    net::NativeGuestStagingFile& staging_;
    std::string build_;
    std::optional<std::string> installedPipelineSeed_;
    bool configured_=false,credential_=false;
    std::unique_ptr<net::NativeGuestPreparedLobby> prepared_;
    std::optional<nlohmann::ordered_json> receipt_;
public:
    NativeJoinPreparation(NativeJoinLobby& document,net::NativeGuestStagingFile& staging,std::string measuredBuild,std::optional<std::string> installedPipelineSeed=std::nullopt)
        :document_(document),staging_(staging),build_(std::move(measuredBuild)),installedPipelineSeed_(std::move(installedPipelineSeed)){}
    NativeJoinPreparation(const NativeJoinPreparation&)=delete;
    NativeJoinPreparation& operator=(const NativeJoinPreparation&)=delete;
    void detachStopped(){document_.detachPrepared();prepared_.reset();}
    net::NativeGuestPreparedLobby* prepared(){return prepared_.get();}
    const nlohmann::ordered_json& receipt()const{if(!receipt_)throw std::runtime_error("Join preparation receipt unavailable");return *receipt_;}
    template<class Pump>
    std::unique_ptr<net::NativeGuestGameInput> installCommittedInput(Pump&& pump){
        auto* session=document_.session();
        if(document_.cancelled()||!session||!prepared_||!prepared_->committed())
            throw std::runtime_error("Original committed Join required for gameplay handoff");
        auto input=std::make_unique<net::NativeGuestGameInput>(*session,*prepared_,std::forward<Pump>(pump));
        input->install();
        document_.detachPrepared();
        return input;
    }
    template<class OpenDisc,class Setup,class Pump>
    bool tick(OpenDisc&& openDisc,Setup&& setup,Pump&& pump){
        if(document_.cancelled())return false;
        auto* session=document_.session();if(!session)return true;
        session->pump();
        if(!session->error().empty())throw std::runtime_error(session->error());
        if(prepared_){prepared_->pump();return !prepared_->committed();}
        if(!session->assetsComplete())return true;
        while(auto message=session->takeControl("partyboard-preparation-v1")){
            const auto* text=std::get_if<std::string>(&message->payload);
            if(!text||text->size()>65536)throw std::invalid_argument("Bounded Join preparation required");
            const auto packet=nlohmann::ordered_json::parse(*text);
            if(packet.at("type")=="session-configuration"){
                session->retainConfiguration(*text,build_);configured_=true;
            }else if(packet.at("type")=="recovery-credential"){
                session->retainOriginalRecoveryCredential(*message);credential_=true;
            }else throw std::invalid_argument("Unexpected Join preparation packet");
        }
        if(!configured_||!credential_)return true;
        const auto channel=session->transport().channel("partyboard-lobby-v1");
        if(!channel||!channel->isOpen())throw std::runtime_error("Original Join Lobby channel required");
        channel->send(nlohmann::ordered_json{{"type","lobby-report"},{"version",1},{"report",{
            {"assetId",session->configuration().at("assetId")},{"sequence",1},{"bytes",session->receivedBytes()},
            {"verified",true},{"shaders",false},{"ready",false},{"saveHash",nullptr}}}}.dump());
        const auto receipt=net::prepareNativeGuestEngine(*session,staging_,build_,
            std::forward<OpenDisc>(openDisc),std::forward<Setup>(setup),[&]{
                if(!pump())return false;
                return !document_.cancelled();
            },installedPipelineSeed_);
        net::publishNativeGuestPreparedReceipt(*session,receipt);
        prepared_=std::make_unique<net::NativeGuestPreparedLobby>(*session,receipt,1);
        document_.bindPrepared(*prepared_);receipt_=receipt;
        return true;
    }
};
}

