#pragma once
#include "port/native_asset_preparation.hpp"
#include "port/native_host_guest.hpp"
#include "port/native_host_lobby_control.hpp"
#include "port/session_start_barrier.hpp"
#include "port/native_session_configuration.hpp"
#include "port/native_prepared_report.hpp"
#include "port/native_host_start_control.hpp"
#include "port/native_rtc_host_input.hpp"
#include "port/recovery_credentials.hpp"
#include "port/native_host_replacement.hpp"

namespace partyboard::net {
// UI-thread session lifetime. tick() never waits for disc hashing or ICE;
// callbacks remain in the existing transport mailboxes. No engine readiness
// or start is implied by asset preparation or invitation creation.
class NativeHostLobbySession {
    NativeRtcSessionControl controls_;
    std::unique_ptr<NativeAssetPreparation> preparation_;
    std::shared_ptr<NativeSharedAsset> asset_;
    std::unique_ptr<NativeLobbyState> state_;
    std::unique_ptr<NativeHostLobbyControl> lobby_;
    std::map<std::string,std::unique_ptr<NativeHostGuest>> guests_;
    std::shared_ptr<NativeRouterWorker> router_;
    std::set<std::string> announced_;
    std::set<std::string> configured_;
    std::optional<nlohmann::ordered_json> configuration_;
    std::optional<nlohmann::ordered_json> hostReceipt_;
    std::shared_ptr<const NativePipelineSeed> pipelineSeed_;
    std::map<std::string,nlohmann::ordered_json> guestReceipts_;
    struct RecoveryGuest {
        nlohmann::ordered_json credential;
        std::unique_ptr<RecoveryIdentity> identity;
    };
    std::map<std::string,RecoveryGuest> recoveryGuests_;
    std::map<std::string,std::string> acceptedReplacementNonces_;
    std::map<std::string,std::string> offeredReplacementNonces_;
    std::unique_ptr<NativeStartCoordinator> startCoordinator_;
    std::unique_ptr<NativeHostStartControl> startControl_;
    std::string error_;
    bool closed_=false;
    bool frozen_=false;
    bool committedInputPrepared_=false;
    unsigned guestSequence_=0;
    bool allowDepartures_=false;
    std::deque<std::string> departed_;
public:
    void setPipelineSeed(std::shared_ptr<const NativePipelineSeed> seed){
        if(closed_||hostReceipt_||!guests_.empty())throw std::runtime_error("Pipeline seed must precede host preparation and invitations");
        pipelineSeed_=std::move(seed);
    }
    struct CommittedInputTransport {
        nlohmann::ordered_json commit;
        std::unique_ptr<NativeRtcHostInput> input;
        std::map<std::string,std::shared_ptr<rtc::DataChannel>> recovery;
    };
    explicit NativeHostLobbySession(const std::filesystem::path& path)
        :preparation_(std::make_unique<NativeAssetPreparation>(path,"Mario Party 4.ciso")){
#if defined(PARTYBOARD_NATIVE_UPNP)
        router_=std::make_shared<NativeRouterWorker>();
#endif
    }
    NativeHostLobbySession(const NativeHostLobbySession&)=delete;
    NativeHostLobbySession& operator=(const NativeHostLobbySession&)=delete;
    bool tick(){
        if(closed_||!error_.empty())return false;
        try{
            if(preparation_){
                if(!preparation_->done())return true;
                asset_=preparation_->result();
                if(!asset_)throw std::runtime_error(preparation_->error());
                preparation_.reset();
                const auto& manifest=asset_->asset.manifest();
                state_=std::make_unique<NativeLobbyState>(manifest.at("id"),manifest.at("size"));
                state_->join("host","Host");
                lobby_=std::make_unique<NativeHostLobbyControl>(*state_,controls_);
                lobby_->setPreparationGate([this](const std::string& peer){
                    const auto found=guestReceipts_.find(peer);
                    return hostReceipt_&&found!=guestReceipts_.end()&&nativePreparedRuntimeReportsMatch(*hostReceipt_,found->second);
                });
                if(!lobby_->reportLocal("host",{{"assetId",manifest.at("id")},{"sequence",1},{"bytes",manifest.at("size")},{"verified",true},{"shaders",false},{"ready",false}}))
                    throw std::runtime_error("Native host status publication failed");
            }
            if(allowDepartures_&&!frozen_){
                std::vector<std::string> gone;
                for(const auto& [peer,guest]:guests_)if((announced_.contains(peer)&&!controls_.connected(peer))||!guest->assetError().empty())gone.push_back(peer);
                for(const auto& peer:gone){removeGuest(peer);departed_.push_back(peer);}
            }
            for(const auto& [peer,guest]:guests_){
                if(const auto error=guest->assetError();!error.empty())throw std::runtime_error(error);
                guest->pumpPipelineSeed();
            }
            while(auto message=controls_.take(NativeRtcControlChannel::Purpose::Preparation)){
                const auto& peer=message->route.peer;
                if(!configuration_||!guests_.contains(peer)||!configured_.contains(peer))throw std::runtime_error("Unconfigured native guest prepared receipt");
                const auto receipt=receiveNativePreparedRuntimeReport(message->text,*configuration_);
                if(auto found=guestReceipts_.find(peer);found!=guestReceipts_.end()&&found->second!=receipt)
                    throw std::runtime_error("Native guest prepared receipt cannot change");
                if(hostReceipt_&&!nativePreparedRuntimeReportsMatch(*hostReceipt_,receipt))
                    throw std::runtime_error("Native guest preparation differs from host");
                guestReceipts_[peer]=receipt;
            }
            if(lobby_&&!lobby_->pump())throw std::runtime_error(lobby_->error().empty()?"Native lobby connection or report failed":lobby_->error());
            if(startControl_&&!startControl_->pumpAcknowledgments())throw std::runtime_error("Native start acknowledgment rejected");
            for(const auto& [peer,guest]:guests_)if(controls_.connected(peer)&&announced_.insert(peer).second)
                if(!lobby_->broadcast())throw std::runtime_error("Native lobby initial snapshot failed");
            if(configuration_)for(const auto& [peer,guest]:guests_)
                if(controls_.connected(peer)&&!configured_.contains(peer)){
                    // Retain the original secret and replay history for this
                    // assigned peer. Replacement connections must reuse these.
                    if(!recoveryGuests_.contains(peer)){
                        auto credential=createRecoveryCredential(configuration_->at("session"),manifest().at("id"),peer);
                        auto identity=std::make_unique<RecoveryIdentity>(credential);
                        recoveryGuests_.emplace(peer,RecoveryGuest{std::move(credential),std::move(identity)});
                    }
                    if(!controls_.send(peer,NativeRtcControlChannel::Purpose::Preparation,configuration_->dump()))
                        throw std::runtime_error("Native retained configuration delivery failed");
                    if(!controls_.send(peer,NativeRtcControlChannel::Purpose::Preparation,recoveryGuests_.at(peer).credential.dump()))
                        throw std::runtime_error("Native original recovery credential delivery failed");
                    configured_.insert(peer);
                }
            return true;
        }catch(const std::exception& error){error_=error.what();close();return false;}
    }
    bool prepared()const{return !closed_&&bool(lobby_);}
    // Caller owns this beneath the session and supplies the original recovery
    // deadline after capturing a genuine suspended input attempt.
    std::unique_ptr<NativeHostReplacement> createOriginalReplacement(const std::string& peer,uint64_t deadline){
        if(closed_||!startCoordinator_||!startCoordinator_->commitDelivered()||
           !configured_.contains(peer)||!recoveryGuests_.contains(peer))
            throw std::runtime_error("Committed assigned original peer required");
        return std::make_unique<NativeHostReplacement>(deadline,
            [this,peer](const nlohmann::ordered_json& description){return signOriginalReplacementOffer(peer,description);},
            [this,peer](const nlohmann::ordered_json& answer){return verifyOriginalReplacementAnswer(peer,answer);},rtc::Configuration{},router_);
    }
    nlohmann::ordered_json signOriginalReplacementOffer(const std::string& peer,
            const nlohmann::ordered_json& description){
        if(closed_||!startCoordinator_||!startCoordinator_->commitDelivered()||
           !configured_.contains(peer)||description.at("type")!="offer"||
           offeredReplacementNonces_.contains(peer)||acceptedReplacementNonces_.contains(peer))
            throw std::runtime_error("Committed original peer without pending replacement required");
        const auto nonce=createRecoveryNonce();
        auto offer=recoveryGuests_.at(peer).identity->sign(description,nonce);
        offeredReplacementNonces_.emplace(peer,nonce);
        return offer;
    }
    bool verifyOriginalReplacementAnswer(const std::string& peer,const nlohmann::ordered_json& answer){
        if(closed_)return false;
        const auto nonce=offeredReplacementNonces_.find(peer);
        if(nonce==offeredReplacementNonces_.end())return false;
        try{
            if(!recoveryGuests_.at(peer).identity->verify(answer,"answer",nonce->second))return false;
            offeredReplacementNonces_.erase(nonce);return true;
        }catch(const std::exception&){return false;}
    }
    // Authenticating a description alone never replaces a channel or resumes
    // gameplay. The recovery coordinator must also authorize paused state.
    bool verifyOriginalReplacementOffer(const std::string& peer,const nlohmann::ordered_json& offer){
        if(closed_||!startCoordinator_||!startCoordinator_->commitDelivered()||
           !configured_.contains(peer))return false;
        const auto found=recoveryGuests_.find(peer);
        if(found==recoveryGuests_.end())return false;
        if(acceptedReplacementNonces_.contains(peer)||offeredReplacementNonces_.contains(peer))return false;
        try{
            if(!found->second.identity->verify(offer,"offer"))return false;
            acceptedReplacementNonces_.emplace(peer,offer.at("nonce").get<std::string>());
            return true;
        }
        catch(const std::exception&){return false;}
    }
    nlohmann::ordered_json signOriginalReplacementAnswer(const std::string& peer,
            const nlohmann::ordered_json& description){
        const auto nonce=acceptedReplacementNonces_.find(peer);
        if(closed_||nonce==acceptedReplacementNonces_.end()||description.at("type")!="answer")
            throw std::runtime_error("Accepted original replacement offer required");
        auto answer=recoveryGuests_.at(peer).identity->sign(description,nonce->second);
        acceptedReplacementNonces_.erase(nonce);
        return answer;
    }
    const std::string& error()const{return error_;}
    // Caller supplies the locally measured package identity, never a guest's
    // advertised identity. This retains/publishes configuration only; actual
    // renderer receipts and start agreement are separate requirements.
    void retainConfiguration(const nlohmann::ordered_json& configuration,const std::string& measuredBuildId){
        if(!prepared()||frozen_)throw std::runtime_error("Native configuration unavailable");
        const auto& seed=configuration.at("seed");
        if(!seed.is_number_integer()||seed.get<int64_t>()<0||seed.get<uint64_t>()>UINT32_MAX)
            throw std::invalid_argument("Invalid native configuration seed");
        std::vector<std::string> extras;
        if(configuration.contains("runtimeArtifacts"))extras=configuration.at("runtimeArtifacts").get<std::vector<std::string>>();
        const auto expected=nativeSessionConfiguration(configuration.at("session"),seed.get<uint32_t>(),
            manifest().at("id"),measuredBuildId,extras);
        if(nlohmann::json(configuration)!=nlohmann::json(expected))throw std::invalid_argument("Native configuration differs from local measurements");
        if(configuration_&&nlohmann::json(*configuration_)!=nlohmann::json(configuration))throw std::runtime_error("Native retained configuration cannot change");
        configuration_=expected;
    }
    nlohmann::ordered_json manifest()const{
        if(!prepared())throw std::runtime_error("Native lobby asset unavailable");
        return asset_->asset.manifest();
    }
    void publishHostPrepared(const PreparedStartRuntime& captured,const nlohmann::ordered_json& actualReceipt,
            std::optional<std::string> installedPipelineSeed=std::nullopt){
        if(!prepared()||!configuration_||frozen_)throw std::runtime_error("Native host preparation unavailable");
        const auto receipt=receiveNativePreparedRuntimeReport(actualReceipt.dump(),*configuration_);
        // Only the local renderer installer may supply this provenance. Never
        // adopt a cache identity from an incoming peer receipt.
        const auto expected=nativePreparedRuntimeReport(captured,*configuration_,true,installedPipelineSeed);
        if(pipelineSeed_&&(!installedPipelineSeed||pipelineSeed_->sha256!=*installedPipelineSeed))
            throw std::invalid_argument("Shared pipeline seed differs from local installation");
        if(!nativePreparedRuntimeReportsMatch(receipt,expected))throw std::invalid_argument("Native host receipt differs from captured engine");
        if(hostReceipt_){
            if(*hostReceipt_!=receipt)throw std::runtime_error("Native host preparation cannot change");
            return;
        }
        for(const auto& [peer,guestReceipt]:guestReceipts_)
            if(!nativePreparedRuntimeReportsMatch(receipt,guestReceipt))throw std::invalid_argument("Native guest preparation differs from captured host");
        if(!lobby_->reportLocal("host",{{"assetId",manifest().at("id")},{"sequence",2},{"bytes",manifest().at("size")},{"verified",true},{"shaders",true},{"ready",false}}))
            throw std::runtime_error("Native host shader publication failed");
        hostReceipt_=receipt;
    }
    std::string addGuest(const std::string& name){
        if(!prepared()||frozen_||guests_.size()>=3)throw std::runtime_error("Native lobby guest slot unavailable");
        if(guestSequence_>=256)throw std::runtime_error("Create a fresh lobby to invite more guests");
        const auto peer="guest"+std::to_string(++guestSequence_);
        auto guest=std::make_unique<NativeHostGuest>(peer,controls_,asset_,rtc::Configuration{},pipelineSeed_,router_);
        lobby_->attachAssigned(peer,name);
        guests_.emplace(peer,std::move(guest));return peer;
    }
    void allowGuestDepartures(){if(!guests_.empty()||frozen_)throw std::runtime_error("Configure departures before invitations");allowDepartures_=true;}
    void removeGuest(const std::string& peer){
        if(!prepared()||frozen_)throw std::runtime_error("Open prepared native lobby required");
        if(!guests_.contains(peer))return;
        lobby_->detachAssigned(peer);guests_.erase(peer);
        announced_.erase(peer);configured_.erase(peer);guestReceipts_.erase(peer);recoveryGuests_.erase(peer);
        offeredReplacementNonces_.erase(peer);acceptedReplacementNonces_.erase(peer);
    }
    std::optional<std::string> takeDeparture(){if(departed_.empty())return std::nullopt;auto peer=std::move(departed_.front());departed_.pop_front();return peer;}
    bool hostReady()const{
        const auto current=snapshot();
        for(const auto& player:current.at("players"))if(player.at("peer")=="host")return player.at("ready");
        throw std::runtime_error("Native host slot unavailable");
    }
    bool preparedReceiptsComplete()const{
        return !closed_&&hostReceipt_&&!guests_.empty()&&guestReceipts_.size()==guests_.size();
    }
    nlohmann::ordered_json preparedReceipts()const{
        if(!preparedReceiptsComplete())throw std::runtime_error("Native assigned preparation receipts incomplete");
        nlohmann::ordered_json receipts=nlohmann::ordered_json::object();receipts["host"]=*hostReceipt_;
        for(const auto& [peer,receipt]:guestReceipts_)receipts[peer]=receipt;
        return receipts;
    }
    void setHostReady(const PreparedStartRuntime& captured,bool ready){
        if(!prepared()||!configuration_||!hostReceipt_||frozen_)throw std::runtime_error("Prepare host shaders before ready");
        if(!nativePreparedRuntimeReportsMatch(*hostReceipt_,nativePreparedRuntimeReport(captured,*configuration_,true,
            hostReceipt_->at("pipelineSeed").is_null()?std::nullopt:std::optional<std::string>(hostReceipt_->at("pipelineSeed").get<std::string>()))))
            throw std::runtime_error("Native host engine changed after preparation");
        const auto current=snapshot();
        for(const auto& player:current.at("players"))if(player.at("peer")=="host"){
            if(!lobby_->reportLocal("host",{{"assetId",manifest().at("id")},{"sequence",player.at("sequence").get<uint64_t>()+1},
                {"bytes",manifest().at("size")},{"verified",true},{"shaders",true},{"ready",ready},{"membership",current.at("membership")}}))
                throw std::runtime_error("Native host ready publication failed");
            return;
        }
        throw std::runtime_error("Native host slot unavailable");
    }
    std::optional<std::string> invitation(const std::string& peer)const{return guests_.at(peer)->invitation();}
    void acceptReply(const std::string& peer,const std::string& code){guests_.at(peer)->acceptReply(code);}
    bool connected(const std::string& peer)const{return !closed_&&controls_.connected(peer);}
    bool assetsComplete(const std::string& peer)const{return !closed_&&guests_.at(peer)->assetsComplete();}
    uint64_t assetSentBytes(const std::string& peer)const{return guests_.at(peer)->assetSentBytes();}
    NativeConnectionRoute connectionRoute(const std::string& peer)const{return guests_.at(peer)->connectionRoute();}
    void retireOriginalRouterMapping(const std::string& peer){guests_.at(peer)->retireRouterMapping();}
    bool routerEnabled()const{return bool(router_);}
    const char* routerPreparation(const std::string& peer)const{return guests_.at(peer)->routerPreparation();}
    nlohmann::ordered_json snapshot()const{
        if(!prepared())throw std::runtime_error(error_.empty()?"Native lobby unavailable":error_);
        return state_->snapshot();
    }
    // Retain exactly the owned membership/revision for the start coordinator.
    // A transfer-complete lobby alone cannot cross this boundary.
    PreparedStartLobby freezeReadyLobby(){
        if(!prepared()||frozen_)throw std::runtime_error("Native start lobby unavailable");
        if(!hostReceipt_||guestReceipts_.size()!=guests_.size())throw std::runtime_error("Native start requires every assigned prepared receipt");
        const auto current=state_->snapshot();
        if(current.at("canStart")!=true)throw std::runtime_error("Native lobby is not ready for start");
        const auto snapshot=lobby_->freezeMembership();
        PreparedStartLobby result{snapshot.at("assetId"),snapshot.at("total"),snapshot.at("revision"),snapshot.at("membership"),{}};
        for(const auto& player:snapshot.at("players"))
            result.players.push_back({player.at("peer").get<std::string>(),player.at("slot").get<uint8_t>(),player.at("ready").get<bool>(),player.at("verified").get<bool>(),player.at("shaders").get<bool>(),player.at("bytes").get<uint64_t>()});
        frozen_=true;return result;
    }
    // Proposal only. Runtime must install committed input and deliver every
    // commit before authorizing engine execution; tick retains these owners.
    void proposePreparedStart(const PreparedStartRuntime& captured){
        if(startCoordinator_||!hostReceipt_||!configuration_)throw std::runtime_error("Native prepared start unavailable");
        if(!nativePreparedRuntimeReportsMatch(*hostReceipt_,nativePreparedRuntimeReport(captured,*configuration_,true,
            hostReceipt_->at("pipelineSeed").is_null()?std::nullopt:std::optional<std::string>(hostReceipt_->at("pipelineSeed").get<std::string>()))))
            throw std::runtime_error("Native host engine changed before start");
        const auto lobby=freezeReadyLobby();
        try{
        startCoordinator_=std::make_unique<NativeStartCoordinator>(lobby,*configuration_);
        if(!startCoordinator_->receivePrepared("host",hostReceipt_->dump()))throw std::runtime_error("Native host start receipt rejected");
        for(const auto& [peer,receipt]:guestReceipts_)
            if(!startCoordinator_->receivePrepared(peer,receipt.dump()))throw std::runtime_error("Native assigned start receipt rejected");
        if(!startCoordinator_->preparedReceiptsComplete())throw std::runtime_error("Native assigned start receipts incomplete");
        startControl_=std::make_unique<NativeHostStartControl>(controls_,*startCoordinator_);
        if(!startControl_->propose(captured,*hostReceipt_,"host")){
            throw std::runtime_error("Native prepared start proposal failed");
        }
        }catch(const std::exception& error){error_=error.what();close();throw;}
    }
    bool acknowledgmentsComplete()const{return !closed_&&startCoordinator_&&startCoordinator_->acknowledgmentsComplete();}
    // Only an acknowledged agreement can attach PAD channels. Returned owner
    // must outlive its stopped engine input binding; this never sends a commit.
    CommittedInputTransport prepareCommittedInputTransport(const PreparedStartRuntime& captured){
        if(closed_||committedInputPrepared_||!startCoordinator_||!startControl_)throw std::runtime_error("Native start agreement unavailable");
        auto commit=startCoordinator_->prepareCommit(captured);
        const auto identity=commit.at("session").get<std::string>();
        SessionId session{};
        const auto nibble=[](char value)->uint8_t{return uint8_t(value<='9'?value-'0':value-'a'+10);};
        for(size_t index=0;index<session.size();++index)session[index]=uint8_t((nibble(identity[index*2])<<4)|nibble(identity[index*2+1]));
        uint8_t active=0;
        for(const auto& player:commit.at("players"))active|=uint8_t(1u<<player.at("slot").get<uint8_t>());
        auto input=std::make_unique<NativeRtcHostInput>(session,0,active);
        std::map<std::string,std::shared_ptr<rtc::DataChannel>> recovery;
        try{
            for(const auto& player:commit.at("players")){
                const auto peer=player.at("peer").get<std::string>();
                if(peer=="host")continue;
                const auto channel=guests_.at(peer)->inputChannel();
                if(!controls_.connected(peer)||!channel->isOpen())throw std::runtime_error("Native committed guest input unavailable");
                input->attachAuthenticated(player.at("slot").get<uint8_t>(),channel);
                const auto control=guests_.at(peer)->recoveryChannel();
                if(!control->isOpen())throw std::runtime_error("Native committed recovery channel unavailable");
                recovery.emplace(peer,control);
            }
        }catch(...){input->closeStopped();startCoordinator_->cancel();throw;}
        committedInputPrepared_=true;
        return {std::move(commit),std::move(input),std::move(recovery)};
    }
    // Caller already bound the exchange to this original committed identity
    // and retains transport/binding until stopped detach. No resume here.
    template<class Binding> bool deliverInstalledInputCommit(const PreparedStartRuntime& captured,Binding& binding){
        if(closed_||!committedInputPrepared_||!startCoordinator_||!startControl_||startCoordinator_->commitDelivered())return false;
        bool installed=false;
        try{
            const auto commit=startCoordinator_->prepareCommit(captured);
            RecoverySession identity{commit.at("session").get<std::string>(),commit.at("buildId").get<std::string>(),
                commit.at("assetId").get<std::string>(),commit.at("profile").get<std::string>(),commit.at("seed").get<uint32_t>(),
                commit.at("revision").get<uint64_t>(),commit.at("membership").get<uint64_t>(),{}};
            for(const auto& player:commit.at("players"))identity.players.push_back({player.at("peer").get<std::string>(),player.at("slot").get<uint8_t>()});
            if(!binding.installOriginalSession(identity))throw std::runtime_error("Native stopped input installation rejected");
            installed=true;
            if(startCoordinator_->deliverCommit(captured,"host",[this](const std::string& peer,const std::string& packet){
                return startControl_->sendCommit(peer,packet);
            }))return true;
        }catch(...){ }
        startCoordinator_->cancel();
        if(installed&&!binding.detachStopped())throw std::runtime_error("Native failed commit input rollback rejected; retain binding");
        return false;
    }
    // No input binding is installed by this pre-game owner. The eventual start
    // coordinator must detach any installed binding before closing this owner.
    void close(){
        if(closed_)return;
        closed_=true;
        preparation_.reset();
        startControl_.reset();startCoordinator_.reset();
        guests_.clear();
        router_.reset();
        recoveryGuests_.clear();
        acceptedReplacementNonces_.clear();
        offeredReplacementNonces_.clear();
        lobby_.reset();state_.reset();asset_.reset();configuration_.reset();hostReceipt_.reset();guestReceipts_.clear();controls_.close();
    }
    ~NativeHostLobbySession(){close();}
};
}
