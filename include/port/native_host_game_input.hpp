#pragma once
#include "port/native_host_lobby_session.hpp"
#include "port/native_engine_input_io.hpp"
#include "port/native_input_poll.hpp"
#include "port/native_input_observations.hpp"
#include "port/recovery_barrier.hpp"
#include "port/native_paused_poll_reconciliation.hpp"
#include "port/native_paused_state_agreement.hpp"
#include <SDL3/SDL_timer.h>

namespace partyboard::net {
// Game-thread owner. Construct only after all start acknowledgments. Keep this
// alive across synchronous GameRun, then stop/detach before closing the lobby.
class NativeHostGameInput {
    NativeHostLobbySession& lobby_;
    NativeHostLobbySession::CommittedInputTransport transport_;
    std::unique_ptr<RecoverablePadExchange> exchange_;
    std::unique_ptr<NativeEngineInputIO> io_;
    std::unique_ptr<NativeInputPollBinding<NativeEngineInputIO>> binding_;
    bool installed_=false;
    std::string recoveryError_;
    NativeInputObservations observations_;
    std::map<std::string,std::unique_ptr<NativeHostReplacement>> replacements_;
    std::map<std::string,std::unique_ptr<NativeHostReplacement>> activeReplacements_;
    std::shared_ptr<NativeControlMailbox> recoveryMailbox_=std::make_shared<NativeControlMailbox>();
    std::map<std::string,std::unique_ptr<NativeRtcControlChannel>> recoveryChannels_;
    std::set<std::string> attachedReplacements_;
    RecoverySession originalIdentity_;
    std::unique_ptr<RecoveryBarrier> recoveryBarrier_;
    std::optional<nlohmann::ordered_json> recoveryProposal_;
    uint32_t recoveryOwner_=0;
    uint64_t recoveryEpoch_=0,recoveryRevision_=0;
    std::unique_ptr<NativePausedPollReconciliation> reconciliation_;
    std::unique_ptr<NativePausedStateAgreement> stateAgreement_;
    std::string verifyingPeer_,verifyingNonce_;
    uint32_t verifyingOwner_=0;
    uint64_t verifyingEpoch_=0,verifyingRevision_=0,verifyingDeadline_=0,nextVerificationTick_=0;
    std::set<std::string> verifiedReplacements_;
    std::map<std::string,std::set<std::string>> completedVerificationNonces_;
    bool aligningRoutes_=false;
    std::vector<std::string> alignmentPeers_;
    size_t alignmentIndex_=0;
    bool completedRetry(const std::string& peer,const nlohmann::ordered_json& packet)const{
        const auto found=completedVerificationNonces_.find(peer);const auto type=packet.value("type","");
        return found!=completedVerificationNonces_.end()&&found->second.contains(packet.value("nonce",""))&&
            ((type=="paused-state-report"&&packet.value("reporter","")==peer)||
             ((type=="paused-state-ack"||type=="paused-poll-report"||type=="paused-poll-ack")&&packet.value("peer","")==peer));
    }
    void retainCompletedNonce(){
        auto& nonces=completedVerificationNonces_[verifyingPeer_];
        // Keep a bounded retry history without limiting future disconnects.
        // An evicted packet is rejected by the current nonce check, never used
        // to authorize a newer round. Signed-description replay history remains
        // owned separately by the original credential identity.
        if(nonces.size()>=16)nonces.erase(nonces.begin());
        nonces.insert(verifyingNonce_);
    }
    void startAlignmentRoute(){
        verifyingPeer_=alignmentPeers_.at(alignmentIndex_%alignmentPeers_.size());verifyingNonce_=createRecoveryNonce();
        if(!recoveryChannels_.at(verifyingPeer_)->send(nlohmann::ordered_json{{"type","session-poll-align"},{"version",1},
            {"nonce",verifyingNonce_},{"session",originalIdentity_.session},{"buildId",originalIdentity_.buildId},{"assetId",originalIdentity_.assetId}}.dump()))
            throw std::runtime_error("Assigned route alignment delivery failed");
        reconciliation_=std::make_unique<NativePausedPollReconciliation>(*exchange_,originalIdentity_,"host",verifyingPeer_,verifyingNonce_,
            [this]()->std::optional<PausedCapture>{if(!exchange_->pending())return std::nullopt;return exchange_->captureGame(verifyingOwner_,io_->now());},
            [this]{return verificationCurrent()&&recoveryTransportsReady();},[this]{return io_->now();},verifyingDeadline_,
            [this](const auto& packet){return recoveryChannels_.at(verifyingPeer_)->send(packet.dump());});
        nextVerificationTick_=0;
    }
    void pumpRouteAlignment(){
        while(auto message=recoveryMailbox_->take()){
            const auto packet=nlohmann::ordered_json::parse(message->text);
            if(completedRetry(message->route.peer,packet))continue;
            reconciliation_->receive(message->route.peer,packet);
            // Publish completion/history before draining a queued retry. A
            // completed reconciler rejects further packets, while the owner
            // authenticates and ignores retries for the completed nonce.
            if(reconciliation_->complete())break;
        }
        if(io_->now()>=nextVerificationTick_){reconciliation_->tick();nextVerificationTick_=io_->now()+20;}
        if(!reconciliation_->complete())return;
        retainCompletedNonce();reconciliation_.reset();++alignmentIndex_;
        if(alignmentIndex_<alignmentPeers_.size()*2)startAlignmentRoute();
        else {aligningRoutes_=false;startRecoveryBarrier(verifyingOwner_);}
    }
    bool verificationCurrent()const{
        const auto found=recoveryChannels_.find(verifyingPeer_);
        return installed_&&found!=recoveryChannels_.end()&&found->second->connected()&&
            exchange_->epoch()==verifyingEpoch_&&exchange_->revision()==verifyingRevision_;
    }
    void advanceVerification(){
        if(reconciliation_&&reconciliation_->complete()&&!stateAgreement_){
            stateAgreement_=std::make_unique<NativePausedStateAgreement>(originalIdentity_,"host",verifyingPeer_,verifyingNonce_,
                [this]{return exchange_->captureGame(verifyingOwner_,io_->now());},[this]{return verificationCurrent();},
                [this]{return io_->now();},verifyingDeadline_,[this](const auto& packet){return recoveryChannels_.at(verifyingPeer_)->send(packet.dump());});
            nextVerificationTick_=0;
        }
    }
    void pumpReplacementVerification(){
        advanceVerification();
        while(auto message=recoveryMailbox_->take()){
            if(message->route.peer!=verifyingPeer_)throw std::runtime_error("Unexpected peer during replacement verification");
            const auto packet=nlohmann::ordered_json::parse(message->text);
            if(completedRetry(message->route.peer,packet))continue;
            if(stateAgreement_)stateAgreement_->receive(message->route.peer,packet);
            else reconciliation_->receive(message->route.peer,packet);
            advanceVerification();
        }
        if(io_->now()>=nextVerificationTick_){
            if(stateAgreement_)stateAgreement_->tick();else reconciliation_->tick();
            nextVerificationTick_=io_->now()+(stateAgreement_?100:20);advanceVerification();
        }
        if(stateAgreement_&&stateAgreement_->complete()){
            verifiedReplacements_.insert(verifyingPeer_);retainCompletedNonce();
            stateAgreement_.reset();reconciliation_.reset();
            attachReplacement(verifyingPeer_);
        }
    }
    void announceInputLoss(uint8_t slot){
        std::string missing;
        for(const auto& player:originalIdentity_.players)if(player.slot==slot)missing=player.peer;
        if(missing.empty()||missing=="host")throw std::runtime_error("Assigned guest input loss required");
        const auto notice=nlohmann::ordered_json{{"type","session-input-loss"},{"version",1},
            {"session",originalIdentity_.session},{"peer",missing}}.dump();
        for(const auto& [peer,channel]:recoveryChannels_)
            if(peer!=missing&&channel->connected()&&!channel->send(notice)){
                exchange_->cancel();throw std::runtime_error("Native input loss notice delivery failed");
            }
    }
public:
    NativeHostGameInput(NativeHostLobbySession& lobby,const PreparedStartRuntime& captured,std::function<bool()> pumpEvents)
        :lobby_(lobby),transport_(lobby.prepareCommittedInputTransport(captured)){
        try{
            if(!pumpEvents)throw std::invalid_argument("Native gameplay event pump required");
            const auto& commit=transport_.commit;
            RecoverySession identity{commit.at("session"),commit.at("buildId"),commit.at("assetId"),commit.at("profile"),
                commit.at("seed"),commit.at("revision"),commit.at("membership"),{}};
            SessionId session{};const auto nibble=[](char c){return c<='9'?c-'0':c-'a'+10;};
            for(size_t i=0;i<session.size();++i)session[i]=uint8_t((nibble(identity.session[i*2])<<4)|nibble(identity.session[i*2+1]));
            std::array<uint32_t,4> owners{};
            for(const auto& player:commit.at("players")){
                const auto slot=player.at("slot").get<uint8_t>();owners[slot]=slot+1;
                identity.players.push_back({player.at("peer"),slot});
            }
            exchange_=std::make_unique<RecoverablePadExchange>(session,owners,1,std::array<int8_t,4>{0,-1,-1,-1});
            if(!exchange_->bindOriginalSession(identity,"host","host"))throw std::runtime_error("Native original input identity rejected");
            originalIdentity_=identity;
            io_=std::make_unique<NativeEngineInputIO>([this,pump=std::move(pumpEvents)]{
                try{
                if(!pump()){io_->stopped("host-event-pump-stopped");return false;}
                if(!transport_.input->pump(*exchange_,io_->now(),[this](uint8_t slot){announceInputLoss(slot);})){io_->stopped(transport_.input->failureReason());
                    io_->stopped(std::to_string(static_cast<int>(transport_.input->lastReceive())).c_str());
                    if(const auto& packet=transport_.input->lastInput();packet){
                        const auto current=io_->capture();
                        const auto detail=nlohmann::ordered_json{{"slot",packet->slot},{"packetFrame",packet->frame},{"packetCheckpoint",packet->checkpoint},
                            {"localFrame",current.frame},{"localCheckpoint",current.checkpoint},{"frand",current.frand},{"rand8",current.rand8},
                            {"firstStopSite",exchange_->firstStopSite()?exchange_->firstStopSite():"unknown"},
                            {"firstStopState",static_cast<int>(exchange_->firstStopState())}}.dump();
                        io_->stopped(detail.c_str());
                    }return false;}
                if(aligningRoutes_)pumpRouteAlignment();
                else if(reconciliation_)pumpReplacementVerification();
                else if(recoveryBarrier_)pumpRecoveryAgreement();
                return true;
                }catch(const std::exception& error){recoveryError_=error.what();io_->stopped(error.what());exchange_->cancel();return false;}
            // SDL uses the platform timer without a spin loop. Windows Sleep(1)
            // measured about14ms here, delaying every packet polling round.
            },[this](const Packet& packet){return transport_.input->send(packet);},[]{SDL_Delay(1);},
            [this](const PartyBoardInputState& state,const PADStatus* pads){observations_.record(state,pads);});
            binding_=std::make_unique<NativeInputPollBinding<NativeEngineInputIO>>(*exchange_,*io_);
            for(const auto& [peer,channel]:transport_.recovery)
                recoveryChannels_.emplace(peer,std::make_unique<NativeRtcControlChannel>(recoveryMailbox_,peer,channel,NativeRtcControlChannel::Purpose::Recovery));
        }catch(...){transport_.input->closeStopped();throw;}
    }
    NativeHostGameInput(const NativeHostGameInput&)=delete;
    NativeHostGameInput& operator=(const NativeHostGameInput&)=delete;
    const NativeInputObservations& observations() const noexcept {return observations_;}
    const NativeInputTiming& inputTiming()const noexcept{return io_->timing();}
    void useRollingDiagnosticObservations(){observations_.useRollingDiagnostics();}
    void setDiagnosticLocalInput(std::function<void(const PartyBoardInputState&, PADStatus*)> input){io_->setDiagnosticLocalInput(std::move(input));}
    const std::string& recoveryError()const{return recoveryError_;}
    nlohmann::ordered_json recoveryStatus(){
        const auto now=io_->now();const auto phase=exchange_->tick(now);
        nlohmann::ordered_json result{{"paused",phase==ReconnectPolicy::State::Paused},{"returnToMenu",phase==ReconnectPolicy::State::ReturnToMenu},
            {"remainingMs",exchange_->remainingRecoveryMs(now)},{"verifying",bool(reconciliation_)},{"aligning",aligningRoutes_},{"agreement",bool(recoveryBarrier_)}};
        result["missingPeers"]=nlohmann::ordered_json::array();
        if(aligningRoutes_){
            result["alignmentPeer"]=verifyingPeer_;
            result["alignmentIndex"]=alignmentIndex_;
            result["alignmentCount"]=alignmentPeers_.size()*2;
        }
        for(const auto& player:originalIdentity_.players)if(exchange_->missingSlots()[player.slot])
            result["missingPeers"].push_back({{"peer",player.peer},{"owner",uint32_t(player.slot)+1}});
        if(phase==ReconnectPolicy::State::Paused&&exchange_->pending()){
            uint32_t owner=0;for(const auto& player:originalIdentity_.players)if(exchange_->missingSlots()[player.slot]){owner=player.slot+1;break;}
            const auto captured=exchange_->captureGame(owner,now);
            result["owner"]=owner;result["frame"]=captured.frame;result["inputFrame"]=std::to_string(captured.inputFrame);
            result["checkpoint"]=captured.checkpoint;result["frand"]=captured.frand;result["rand8"]=captured.rand8;
        }
        return result;
    }
    NativeHostReplacement& replacement(const std::string& peer){
        const auto pending=replacements_.find(peer);
        return pending!=replacements_.end()?*pending->second:*activeReplacements_.at(peer);
    }
    bool replacementVerified(const std::string& peer)const{return attachedReplacements_.contains(peer)&&verifiedReplacements_.contains(peer);}
    void requestReturnToMenu(){exchange_->cancel();PartyBoard_GameRequestStop();}
    bool recoveryTransportsReady()const{
        return recoveryChannels_.size()==originalIdentity_.players.size()-1&&
            std::all_of(recoveryChannels_.begin(),recoveryChannels_.end(),[](const auto& entry){return entry.second->connected();});
    }
    void beginRecoveryAgreement(uint32_t missingOwner){
        if(!installed_||reconciliation_||recoveryBarrier_||!recoveryTransportsReady())throw std::runtime_error("Paused all-player authenticated transports required");
        const auto now=io_->now();const auto captured=exchange_->captureGame(missingOwner,now);
        bool verified=false;for(const auto& player:originalIdentity_.players)
            if(uint32_t(player.slot)+1==missingOwner)verified=attachedReplacements_.contains(player.peer)&&verifiedReplacements_.contains(player.peer);
        if(!verified)throw std::runtime_error("Missing original peer must complete replacement state verification");
        alignmentPeers_.clear();for(const auto& player:originalIdentity_.players)if(player.peer!="host")alignmentPeers_.push_back(player.peer);
        alignmentIndex_=0;aligningRoutes_=true;verifyingOwner_=missingOwner;
        verifyingEpoch_=captured.epoch;verifyingRevision_=captured.revision;
        verifyingDeadline_=now+exchange_->remainingRecoveryMs(now);
        startAlignmentRoute();
    }
private:
    void startRecoveryBarrier(uint32_t missingOwner){
        if(aligningRoutes_||reconciliation_||alignmentIndex_!=alignmentPeers_.size()*2||!verificationCurrent()||!recoveryTransportsReady())
            throw std::runtime_error("Every original route must complete two alignment sweeps");
        const auto now=io_->now();const auto captured=exchange_->captureGame(missingOwner,now);
        bool verified=false;for(const auto& player:originalIdentity_.players)
            if(uint32_t(player.slot)+1==missingOwner)verified=attachedReplacements_.contains(player.peer)&&verifiedReplacements_.contains(player.peer);
        if(!verified)throw std::runtime_error("Missing original peer must complete replacement state verification");
        recoveryOwner_=missingOwner;recoveryEpoch_=captured.epoch;recoveryRevision_=captured.revision;
        const auto nonce=createRecoveryNonce();
        recoveryBarrier_=std::make_unique<RecoveryBarrier>(
            [this,nonce]{return recoveryProposal(originalIdentity_,exchange_->captureGame(recoveryOwner_,io_->now()),"host",nonce);},
            [this]{return recoveryTransportsReady()&&exchange_->epoch()==recoveryEpoch_&&exchange_->revision()==recoveryRevision_;},
            [this]{return io_->now();},now+exchange_->remainingRecoveryMs(now));
        recoveryProposal_=recoveryBarrier_->proposal();
        for(const auto& [peer,channel]:recoveryChannels_)if(!channel->send(recoveryProposal_->dump())){
            recoveryBarrier_->cancel();exchange_->cancel();throw std::runtime_error("Native recovery proposal delivery failed");
        }
    }
public:
    bool pumpRecoveryAgreement(){
        if(!recoveryBarrier_)return false;
        bool agreed=false;
        while(auto message=recoveryMailbox_->take()){
            const auto packet=nlohmann::ordered_json::parse(message->text);
            if(completedRetry(message->route.peer,packet))continue;
            agreed=recoveryBarrier_->acknowledge(message->route.peer,packet)||agreed;
        }
        if(!agreed)return false;
        const auto commit=recoveryBarrier_->commit();
        for(const auto& [peer,channel]:recoveryChannels_)if(!channel->send(commit.dump())){exchange_->cancel();throw std::runtime_error("Native recovery commit delivery failed");}
        const auto actual=io_->capture();
        if(!exchange_->resumeCommitted(originalIdentity_,"host","host",*recoveryProposal_,commit,
            recoveryEpoch_,recoveryRevision_,actual.frame,{actual.checkpoint,actual.frand,actual.rand8},recoveryTransportsReady(),io_->now())){
            exchange_->cancel();throw std::runtime_error("Native original recovery resume rejected");
        }
        for(auto it=replacements_.begin();it!=replacements_.end();){
            if(attachedReplacements_.contains(it->first)){
                activeReplacements_[it->first]=std::move(it->second);it=replacements_.erase(it);
            }else ++it;
        }
        recoveryBarrier_.reset();recoveryProposal_.reset();return true;
    }
    NativeHostReplacement& beginReplacement(const std::string& peer){
        if(!installed_||replacements_.contains(peer))throw std::runtime_error("Installed original input without pending replacement required");
        uint32_t owner=0;
        for(const auto& player:transport_.commit.at("players"))
            if(player.at("peer")==peer)owner=player.at("slot").get<uint32_t>()+1;
        const auto now=io_->now();
        exchange_->captureGame(owner,now); // Verifies actual frozen state and missing assigned owner.
        // The previously adopted connection stays owned until its replacement
        // has passed strict state verification and is attached. Retire only
        // its old proof, which cannot authorize this new disconnect epoch.
        attachedReplacements_.erase(peer);verifiedReplacements_.erase(peer);
        const auto remaining=exchange_->remainingRecoveryMs(now);
        if(!remaining)throw std::runtime_error("Original recovery deadline expired");
        auto replacement=lobby_.createOriginalReplacement(peer,now+remaining);
        auto* result=replacement.get();replacements_.emplace(peer,std::move(replacement));return *result;
    }
    // Channel attachment preserves the suspended input and policy. An all-peer
    // recovery barrier must separately authorize resumeCommitted().
    void beginReplacementVerification(const std::string& peer){
        if(!installed_||reconciliation_||recoveryBarrier_||attachedReplacements_.contains(peer))
            throw std::runtime_error("Single owned paused replacement verification required");
        const auto now=io_->now();auto& replacement=*replacements_.at(peer);
        if(!replacement.ready(now))throw std::runtime_error("Authenticated replacement channels required");
        uint32_t owner=0;for(const auto& player:originalIdentity_.players)if(player.peer==peer)owner=player.slot+1;
        const auto captured=exchange_->captureGame(owner,now);
        verifyingPeer_=peer;verifyingNonce_=replacement.nonce();verifyingOwner_=owner;
        verifyingEpoch_=captured.epoch;verifyingRevision_=captured.revision;
        verifyingDeadline_=now+exchange_->remainingRecoveryMs(now);nextVerificationTick_=0;
        recoveryChannels_.erase(peer);
        recoveryChannels_.emplace(peer,std::make_unique<NativeRtcControlChannel>(recoveryMailbox_,peer,replacement.recovery(now),NativeRtcControlChannel::Purpose::Recovery));
        reconciliation_=std::make_unique<NativePausedPollReconciliation>(*exchange_,originalIdentity_,"host",peer,verifyingNonce_,
            [this]()->std::optional<PausedCapture>{if(!exchange_->pending())return std::nullopt;return exchange_->captureGame(verifyingOwner_,io_->now());},
            [this]{return verificationCurrent();},[this]{return io_->now();},verifyingDeadline_,
            [this](const auto& packet){return recoveryChannels_.at(verifyingPeer_)->send(packet.dump());});
    }
    void attachReplacement(const std::string& peer){
        if(!installed_||!verifiedReplacements_.contains(peer)||attachedReplacements_.contains(peer))throw std::runtime_error("Verified unattached installed original replacement required");
        const auto now=io_->now();auto& replacement=*replacements_.at(peer);
        if(!replacement.ready(now))throw std::runtime_error("Authenticated replacement channels not open");
        uint8_t slot=4;
        for(const auto& player:transport_.commit.at("players"))if(player.at("peer")==peer)slot=player.at("slot").get<uint8_t>();
        exchange_->captureGame(uint32_t(slot)+1,now);
        transport_.input->attachAuthenticated(slot,replacement.input(now));
        activeReplacements_.erase(peer); // New route generation rejects late old callbacks.
        lobby_.retireOriginalRouterMapping(peer);
        attachedReplacements_.insert(peer);
    }
    bool installAndDeliver(const PreparedStartRuntime& captured){
        if(installed_||!binding_)return false;
        installed_=true; // Preserve owner if delivery throws on failed rollback.
        if(!lobby_.deliverInstalledInputCommit(captured,*binding_)){installed_=false;return false;}
        return true;
    }
    bool closeStopped(){
        if(installed_){if(!binding_->detachStopped())return false;installed_=false;}
        if(recoveryBarrier_)recoveryBarrier_->cancel();
        recoveryBarrier_.reset();recoveryProposal_.reset();
        stateAgreement_.reset();reconciliation_.reset();
        aligningRoutes_=false;alignmentPeers_.clear();
        recoveryChannels_.clear();recoveryMailbox_->close();
        replacements_.clear();
        activeReplacements_.clear();
        if(transport_.input)transport_.input->closeStopped();
        return true;
    }
    ~NativeHostGameInput(){if(!closeStopped())std::terminate();}
};
}
