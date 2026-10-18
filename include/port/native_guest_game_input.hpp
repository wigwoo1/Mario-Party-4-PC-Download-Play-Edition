#pragma once
#include "port/native_guest_prepared_lobby.hpp"
#include "port/guest_input_bridge.hpp"
#include "port/native_engine_input_io.hpp"
#include "port/native_input_observations.hpp"
#include "port/native_guest_recovery_control.hpp"
#include "port/native_guest_replacement.hpp"
#include "port/native_guest_replacement_verification.hpp"
#include <SDL3/SDL_timer.h>
namespace partyboard::net {
// Nonmoving game-thread owner. The original Join session outlives this owner.
// Installation requires its accepted commit; resume remains caller-authorized.
class NativeGuestGameInput {
    NativeGuestJoinSession& session_;
    nlohmann::ordered_json commit_;
    RecoverySession identity_;
    std::string host_;
    std::string recoveryError_;
    std::unique_ptr<GuestInputBridge> transport_;
    std::unique_ptr<RecoverablePadExchange> exchange_;
    std::unique_ptr<NativeGuestRecoveryControl> recovery_;
    std::unique_ptr<NativeEngineInputIO> io_;
    std::unique_ptr<NativeInputPollBinding<NativeEngineInputIO>> binding_;
    NativeInputObservations observations_;
    bool installed_=false,lossAnnounced_=false,closed_=false;
    std::unique_ptr<NativeGuestReplacement> replacement_;
    std::unique_ptr<NativeGuestReplacementVerification> replacementVerification_;
    uint64_t replacementEpoch_=0,replacementRevision_=0,replacementDeadline_=0;
    std::unique_ptr<NativeGuestInvitation> activeReplacement_;
    SessionId padSession_{};
    uint8_t localSlot_=0,activeMask_=0;
    std::shared_ptr<rtc::DataChannel> channel(const std::string& label){
        return activeReplacement_?activeReplacement_->channel(label):session_.transport().channel(label);
    }
public:
    NativeGuestGameInput(NativeGuestJoinSession& session,const NativeGuestPreparedLobby& prepared,
            std::function<bool()> pumpEvents):session_(session){
        if(!prepared.ownsSession(session)||!prepared.committed()||!pumpEvents)
            throw std::runtime_error("Original committed Join and event pump required");
        commit_=prepared.commit();
        auto expected=SessionStartBarrier::fromConfiguration(prepared.snapshot(),session.configuration(),
            captureNativePreparedStart(session.configuration().at("assetId"),session.configuration().at("buildId"))).proposal();
        expected["type"]="session-start-commit";
        if(nlohmann::json(expected)!=nlohmann::json(commit_))throw std::runtime_error("Join engine differs from committed preparation");
        identity_={commit_.at("session"),commit_.at("buildId"),commit_.at("assetId"),commit_.at("profile"),
            commit_.at("seed"),commit_.at("revision"),commit_.at("membership"),{}};
        SessionId sessionId{};const auto nibble=[](char c){return c<='9'?c-'0':c-'a'+10;};
        for(size_t i=0;i<16;++i)sessionId[i]=uint8_t((nibble(identity_.session[i*2])<<4)|nibble(identity_.session[i*2+1]));
        std::array<uint32_t,4> owners{};std::array<int8_t,4> lanes{-1,-1,-1,-1};
        uint8_t local=4,active=0;std::string host;
        for(const auto& player:commit_.at("players")){
            const auto slot=player.at("slot").get<uint8_t>();const auto peer=player.at("peer").get<std::string>();
            owners[slot]=slot+1;active|=uint8_t(1u<<slot);identity_.players.push_back({peer,slot});
            if(slot==0)host=peer;if(peer==session.transport().assignedPeer())local=slot;
        }
        if(local==0||local>=4||host.empty())throw std::runtime_error("Committed guest slot required");
        host_=host;
        padSession_=sessionId;localSlot_=local;activeMask_=active;
        lanes[local]=0;
        exchange_=std::make_unique<RecoverablePadExchange>(sessionId,owners,local+1,lanes);
        if(!exchange_->bindOriginalSession(identity_,session.transport().assignedPeer(),host))throw std::runtime_error("Guest original input identity rejected");
        transport_=std::make_unique<GuestInputBridge>(sessionId,local,active,[this](std::span<const uint8_t> bytes){
            const auto channel=this->channel("partyboard-input-v1");
            if(!channel||!channel->isOpen())return false;
            channel->send(reinterpret_cast<const rtc::byte*>(bytes.data()),bytes.size());return true;
        });
        recovery_=std::make_unique<NativeGuestRecoveryControl>(*exchange_,identity_,session.transport().assignedPeer(),host_,
            [this]{return io_->now();},[this](uint32_t owner){return exchange_->captureGame(owner,io_->now());},
            [this]{const auto input=channel("partyboard-input-v1"),control=channel("partyboard-recovery-v1");
                return !closed_&&input&&input->isOpen()&&control&&control->isOpen();},
            [this](const auto& packet){const auto control=channel("partyboard-recovery-v1");
                if(!control||!control->isOpen())return false;control->send(packet.dump());return true;});
        io_=std::make_unique<NativeEngineInputIO>([this,pump=std::move(pumpEvents)]{
            const auto stop=[this](const std::string& reason){recoveryError_=reason;return false;};
            if(closed_)return stop("Guest input owner closed");
            if(!pump())return stop("Guest event pump stopped");
            if(!activeReplacement_){session_.pump();if(!session_.error().empty())return stop("Original Join session: "+session_.error());}
            else if(!activeReplacement_->error().empty())return stop("Replacement Join transport: "+activeReplacement_->error());
            const auto channel=this->channel("partyboard-input-v1");
            if(!channel||!channel->isOpen()){
                if(!lossAnnounced_){lossAnnounced_=true;if(!exchange_->disconnected(1,io_->now()))return stop("Guest local loss pause rejected");}
            }
            if(activeReplacement_)while(session_.takeControl()){} // Retired original route grants no authority.
            while(auto message=activeReplacement_?activeReplacement_->takeMessage():session_.takeControl()){
                if(message->label=="partyboard-recovery-v1"){
                    const auto* text=std::get_if<std::string>(&message->payload);
                    if(!text||text->size()>4096)return stop("Invalid guest recovery control payload");
                    try{recovery_->receive(host_,nlohmann::ordered_json::parse(*text));}
                    catch(const std::exception& error){recoveryError_=error.what();return false;}
                    continue;
                }
                if(message->label!="partyboard-input-v1")return stop("Unexpected guest gameplay channel: "+message->label);
                const auto* bytes=std::get_if<rtc::binary>(&message->payload);
                if(!bytes)return stop("Guest input payload is not binary");
                const auto result=transport_->enqueueHost(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes->data()),bytes->size()));
                if(result!=InputMailbox::Push::Queued)return stop("Guest input mailbox rejected host packet");
            }
            try{
                recovery_->tick();
                if(replacement_&&replacement_->ready(io_->now())){
                    if(!replacementVerification_){
                        replacementVerification_=std::make_unique<NativeGuestReplacementVerification>(*exchange_,identity_,
                            session_.transport().assignedPeer(),host_,replacement_->nonce(),
                            [this]{return exchange_->captureGame(1,io_->now());},
                            [this]{return replacement_->ready(io_->now())&&exchange_->epoch()==replacementEpoch_&&exchange_->revision()==replacementRevision_;},
                            [this]{return io_->now();},replacementDeadline_,
                            [this](const auto& packet){replacement_->recovery(io_->now())->send(packet.dump());return true;});
                    }
                    // Verification is a candidate-only stage. Leave subsequent
                    // packets queued for the later authenticated adoption owner.
                    for(unsigned i=0;i<64&&!replacementVerification_->verified();++i){
                        auto message=replacement_->takeMessage(io_->now());if(!message)break;
                        const auto* text=std::get_if<std::string>(&message->payload);
                        if(message->label!="partyboard-recovery-v1"||!text||text->size()>4096)
                            throw std::runtime_error("Bounded replacement verification control required");
                        replacementVerification_->receive(host_,nlohmann::ordered_json::parse(*text));
                    }
                    if(!replacementVerification_->verified())replacementVerification_->tick();
                    if(replacementVerification_->verified()){
                        recovery_->verifiedLocalReplacement(replacement_->nonce());
                        transport_->closeStopped();
                        if(activeReplacement_)activeReplacement_->close();
                        activeReplacement_=replacement_->releaseCandidate(io_->now());
                        // Retire original PAD authority here, but preserve its
                        // lobby/preparation connection until stopped cleanup.
                        // The host still owns that original lobby membership.
                        transport_=std::make_unique<GuestInputBridge>(padSession_,localSlot_,activeMask_,[this](std::span<const uint8_t> bytes){
                            const auto input=this->channel("partyboard-input-v1");
                            if(!input||!input->isOpen())return false;
                            input->send(reinterpret_cast<const rtc::byte*>(bytes.data()),bytes.size());return true;
                        });
                        replacementVerification_.reset();replacement_.reset();lossAnnounced_=false;
                    }
                }
            }catch(const std::exception& error){recoveryError_=error.what();return false;}
            const auto phase=exchange_->tick(io_->now());
            if(phase==ReconnectPolicy::State::ReturnToMenu)return stop("Guest recovery policy returned to menu");
            if(phase==ReconnectPolicy::State::Paused)return true;
            return transport_->pump(*exchange_)||stop("Guest host input queue could not drain");
        },[this](const Packet& packet){return transport_->sendLocal(packet);},
        // Preserve the requested1ms idle; avoid Windows' coarse Sleep(1) wakeup.
        []{SDL_Delay(1);},
        [this](const PartyBoardInputState& state,const PADStatus* pads){observations_.record(state,pads);});
        binding_=std::make_unique<NativeInputPollBinding<NativeEngineInputIO>>(*exchange_,*io_);
    }
    NativeGuestGameInput(const NativeGuestGameInput&)=delete;
    const NativeInputTiming& inputTiming()const noexcept{return io_->timing();}
    NativeGuestGameInput& operator=(const NativeGuestGameInput&)=delete;
    void install(){
        if(installed_||closed_)throw std::runtime_error("Fresh guest input installation required");
        const auto capture=captureNativePreparedStart(identity_.assetId,identity_.buildId);
        if(capture.checkpoint!=commit_.at("checkpoint").get<uint32_t>()||capture.seed!=identity_.seed)
            throw std::runtime_error("Guest engine changed before input installation");
        installNativeCommitInput(commit_,*binding_);installed_=true;
    }
    const NativeInputObservations& observations()const{return observations_;}
    void useRollingDiagnosticObservations(){observations_.useRollingDiagnostics();}
    void setDiagnosticLocalInput(std::function<void(const PartyBoardInputState&, PADStatus*)> input){io_->setDiagnosticLocalInput(std::move(input));}
    const std::string& recoveryError()const{return recoveryError_;}
    void disconnectDiagnostic(){
        if(!installed_||closed_||exchange_->tick(io_->now())!=ReconnectPolicy::State::Running)
            throw std::runtime_error("Running original guest required for diagnostic loss");
        const auto input=channel("partyboard-input-v1");
        if(!input||!input->isOpen())throw std::runtime_error("Open original guest input required");
        input->close();
    }
    void beginReplacement(const nlohmann::ordered_json& offer){
        const auto now=io_->now();
        if(!installed_||closed_||replacement_||!lossAnnounced_||exchange_->tick(now)!=ReconnectPolicy::State::Paused||!exchange_->pending())
            throw std::runtime_error("Original locally disconnected suspended guest required");
        replacementDeadline_=now+exchange_->remainingRecoveryMs(now);
        replacement_=std::make_unique<NativeGuestReplacement>(session_.recoveryIdentity(),offer,now,replacementDeadline_);
        replacementEpoch_=exchange_->epoch();replacementRevision_=exchange_->revision();
    }
    bool canBeginReplacement(){return installed_&&!closed_&&!replacement_&&lossAnnounced_&&exchange_->tick(io_->now())==ReconnectPolicy::State::Paused&&exchange_->pending();}
    std::optional<nlohmann::ordered_json> replacementAnswer(){
        if(!replacement_)throw std::runtime_error("Original candidate replacement required");
        return replacement_->answer(io_->now());
    }
    bool replacementVerified()const{return replacementVerification_&&replacementVerification_->verified();}
    nlohmann::ordered_json recoveryStatus()const{
        auto result=recovery_->status();const auto now=io_->now();const auto phase=exchange_->tick(now);
        result["paused"]=phase==ReconnectPolicy::State::Paused;result["returnToMenu"]=phase==ReconnectPolicy::State::ReturnToMenu;
        result["remainingMs"]=exchange_->remainingRecoveryMs(now);result["replacementCandidate"]=bool(replacement_);
        result["replacementVerified"]=replacementVerified();result["replacementAdopted"]=bool(activeReplacement_);return result;
    }
    nlohmann::ordered_json diagnosticState()const{
        auto result=recoveryStatus();
        result["transport"]=activeReplacement_?activeReplacement_->diagnosticState():session_.transport().diagnosticState();
        result["pending"]=exchange_->pending();result["missingSlots"]=exchange_->missingSlots();
        const auto packets=transport_->diagnostics();
        auto received=nlohmann::ordered_json::array();
        for(unsigned slot=0;slot<4;++slot)received.push_back({{"slot",slot},{"count",packets.received[slot]},
            {"lastFrame",packets.lastReceivedFrame[slot]?nlohmann::ordered_json(*packets.lastReceivedFrame[slot]):nlohmann::ordered_json(nullptr)}});
        result["packets"]={{"received",received},{"rejected",packets.rejected},
            {"sendAttempts",packets.sendAttempts},{"sendFailures",packets.sendFailures},
            {"lastSentFrame",packets.lastSentFrame?nlohmann::ordered_json(*packets.lastSentFrame):nlohmann::ordered_json(nullptr)}};
        return result;
    }
    bool closeStopped(){
        if(installed_){if(!binding_->detachStopped())return false;installed_=false;}
        closed_=true;replacementVerification_.reset();if(replacement_)replacement_->close();replacement_.reset();
        if(activeReplacement_)activeReplacement_->close();activeReplacement_.reset();
        if(transport_)transport_->closeStopped();return true;
    }
    ~NativeGuestGameInput(){if(!closeStopped())std::terminate();}
};
}
