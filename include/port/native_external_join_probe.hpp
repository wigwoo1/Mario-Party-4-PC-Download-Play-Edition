#pragma once
#include "port/native_guest_game_input.hpp"
#include "port/native_probe_pipe.hpp"
#include "port/native_runtime_build.hpp"
#include <cstdio>
namespace partyboard::net {
// Diagnostic external-peer adapter. The host owns invitations/configuration/
// readiness/commit. Stdin is local test control, never peer authentication.
class NativeExternalJoinProbe {
    NativeGuestStagingFile staging_;
    std::unique_ptr<NativeGuestJoinSession> session_;
    std::unique_ptr<NativeGuestPreparedLobby> prepared_;
    std::unique_ptr<NativeGuestGameInput> input_;
    NativeProbePipe pipe_;
    std::string build_;
    std::optional<std::string> installedPipelineSeed_;
    bool replied_=false,configured_=false,credential_=false,stopped_=false,runRequested_=false;
    std::optional<uint64_t> emittedRevision_;
    nlohmann::ordered_json emittedRecovery_;
    bool replacementAnswerPending_=false;
    std::unique_ptr<NativeGuestLobbySnapshot> downloadingLobby_;
    std::optional<NativeGuestInvitation::Message> latestLobby_;
    static void emit(const nlohmann::ordered_json& event){fprintf(stdout,"Native external Join event: %s\n",event.dump().c_str());fflush(stdout);}
    void drainDownloadingLobby(){
        while(auto message=session_->takeControl("partyboard-lobby-v1")){
            const auto* text=std::get_if<std::string>(&message->payload);
            if(!text||text->size()>65536)throw std::invalid_argument("Bounded download lobby snapshot required");
            if(!downloadingLobby_){
                const auto snapshot=nlohmann::ordered_json::parse(*text).at("snapshot");
                downloadingLobby_=std::make_unique<NativeGuestLobbySnapshot>(snapshot.at("assetId"),snapshot.at("total"),session_->transport().assignedPeer());
            }
            // This progress state grants no readiness or executable identity.
            downloadingLobby_->receive(*text);latestLobby_=std::move(*message);
        }
    }
public:
    NativeExternalJoinProbe(const std::filesystem::path& base,const std::filesystem::path& descriptorPath,std::optional<std::string> installedPipelineSeed=std::nullopt):staging_(base),installedPipelineSeed_(std::move(installedPipelineSeed)){
        std::ifstream file(descriptorPath);if(!file)throw std::runtime_error("External Join measured package required");
        const auto descriptor=nlohmann::ordered_json::parse(file);
        std::vector<NativeRuntimeArtifact> artifacts;
        for(const auto& artifact:descriptor.at("files"))artifacts.push_back({artifact.at("name"),nativeRuntimeArtifactPath(descriptorPath,artifact.at("path").get<std::string>())});
        build_=fingerprintNativeRuntimeFiles(std::move(artifacts),descriptor.at("configuration").at("buildId").get<std::string>()).at("buildId");
        emit({{"type","join-awaiting-invitation"},{"buildId",build_}});
    }
    template<class OpenDisc,class PlatformSetup,class PumpEvents>
    bool tick(OpenDisc&& openDisc,PlatformSetup&& setup,PumpEvents&& pumpEvents){
        for(const auto& command:pipe_.poll()){
            if(command=="stop"){stopped_=true;emit({{"type","join-stopped"},{"phase",PartyBoard_GameLifecyclePhase()},{"frame",PartyBoard_GameFrameCount()}});return false;}
            if(command=="ready"){
                if(!prepared_)throw std::runtime_error("Prepared external Join required for Ready");
                prepared_->setReady(true);continue;
            }
            if(command=="run"){
                if(!input_||runRequested_)throw std::runtime_error("Installed original guest input required before diagnostic run");
                runRequested_=true;return false;
            }
            const auto packet=nlohmann::ordered_json::parse(command);
            if(packet.at("type")!="join-invitation"||session_)throw std::invalid_argument("Single external Join invitation required");
            session_=std::make_unique<NativeGuestJoinSession>(packet.at("code"),staging_.stream());
        }
        if(!session_)return true;
        session_->pump();if(!session_->error().empty())throw std::runtime_error(session_->error());
        if(!prepared_)drainDownloadingLobby();
        if(!replied_)if(const auto reply=session_->transport().reply()){
            replied_=true;emit({{"type","join-reply"},{"peer",session_->transport().assignedPeer()},{"code",*reply}});
        }
        if(!session_->assetsComplete())return true;
        if(!prepared_){
            while(auto message=session_->takeControl("partyboard-preparation-v1")){
                const auto* text=std::get_if<std::string>(&message->payload);if(!text)throw std::invalid_argument("External preparation text required");
                const auto packet=nlohmann::ordered_json::parse(*text);
                if(packet.at("type")=="session-configuration"){session_->retainConfiguration(*text,build_);configured_=true;}
                else if(packet.at("type")=="recovery-credential"){session_->retainOriginalRecoveryCredential(*message);credential_=true;}
                else throw std::invalid_argument("Unexpected external preparation packet");
            }
            if(!configured_||!credential_)return true;
            const auto lobbyChannel=session_->transport().channel("partyboard-lobby-v1");
            if(!lobbyChannel||!lobbyChannel->isOpen())throw std::runtime_error("Original guest lobby channel required");
            lobbyChannel->send(nlohmann::ordered_json{{"type","lobby-report"},{"version",1},{"report",{
                {"assetId",session_->configuration().at("assetId")},{"sequence",1},{"bytes",session_->receivedBytes()},
                {"verified",true},{"shaders",false},{"ready",false},{"saveHash",nullptr}}}}.dump());
            const auto receipt=prepareNativeGuestEngine(*session_,staging_,build_,std::forward<OpenDisc>(openDisc),std::forward<PlatformSetup>(setup),[&]{drainDownloadingLobby();return pumpEvents();},installedPipelineSeed_);
            publishNativeGuestPreparedReceipt(*session_,receipt);
            prepared_=std::make_unique<NativeGuestPreparedLobby>(*session_,receipt,1);
            if(latestLobby_)prepared_->receiveSnapshot(*latestLobby_);
            latestLobby_.reset();downloadingLobby_.reset();
            emit({{"type","join-engine-prepared"},{"peer",session_->transport().assignedPeer()},{"receipt",receipt}});
        }
        if(!input_){
            prepared_->pump();
            if(prepared_->hasSnapshot()&&emittedRevision_!=prepared_->snapshot().revision){
                emittedRevision_=prepared_->snapshot().revision;
                emit({{"type","join-lobby"},{"revision",*emittedRevision_},{"membership",prepared_->snapshot().membership}});
            }
            if(prepared_->committed()){
                input_=std::make_unique<NativeGuestGameInput>(*session_,*prepared_,[this,pump=pumpEvents]{
                    const auto recovery=input_->recoveryStatus();
                    if(recovery.at("owner")!=0&&recovery!=emittedRecovery_){emittedRecovery_=recovery;emit({{"type","join-recovery"},{"status",recovery}});}
                    if(input_->observations().acceptedCount()>=60){PartyBoard_GameRequestStop();return false;}
                    for(const auto& command:pipe_.poll()){
                        if(command=="stop"){PartyBoard_GameRequestStop();return false;}
                        if(command=="disconnect"){
                            input_->disconnectDiagnostic();
                            emit({{"type","join-disconnected"},{"accepted",input_->observations().acceptedCount()}});continue;
                        }
                        const auto packet=nlohmann::ordered_json::parse(command);
                        if(packet.at("type")!="join-recovery-offer"||replacementAnswerPending_)
                            throw std::invalid_argument("Single pending original Join replacement offer required");
                        input_->beginReplacement(packet.at("offer"));replacementAnswerPending_=true;
                    }
                    if(replacementAnswerPending_)if(const auto answer=input_->replacementAnswer()){
                        replacementAnswerPending_=false;emit({{"type","join-recovery-answer"},{"answer",*answer}});
                    }
                    return pump();
                });input_->install();
                emit({{"type","join-input-installed"},{"commit",prepared_->commit()},{"phase",PartyBoard_GameLifecyclePhase()},{"frame",PartyBoard_GameFrameCount()}});
            }
        }
        return true;
    }
    bool stopped()const{return stopped_;}
    bool runRequested()const{return runRequested_;}
    void run(){
        if(!runRequested_||!input_||stopped_)throw std::runtime_error("Committed diagnostic Join run required");
        const auto result=PartyBoard_GameResume();PartyBoard_GameRequestStop();
        const auto& trace=input_->observations();nlohmann::ordered_json samples=nlohmann::ordered_json::array();
        for(const auto& sample:trace.entries())samples.push_back({{"frame",sample.state.frame},{"checkpoint",sample.state.checkpoint},{"frand",sample.state.frand},{"rand8",sample.state.rand8},{"pads",sample.pads}});
        emit({{"type","join-gameplay"},{"execution",result},{"accepted",trace.acceptedCount()},{"observations",samples},{"recoveryError",input_->recoveryError()},{"phase",PartyBoard_GameLifecyclePhase()},{"frame",PartyBoard_GameFrameCount()}});
        if(!input_->closeStopped())throw std::runtime_error("Running Join stopped input detach rejected");
        emit({{"type","join-game-stopped"},{"detached",true},{"phase",PartyBoard_GameLifecyclePhase()},{"frame",PartyBoard_GameFrameCount()}});
        if(result!=0||trace.acceptedCount()!=60)throw std::runtime_error("Native Join diagnostic did not accept sixty polls");
        stopped_=true;
    }
    void closeStopped(){
        if(input_&&!input_->closeStopped())throw std::runtime_error("External guest input detach rejected");
        input_.reset();prepared_.reset();if(session_)session_->close();session_.reset();
    }
    ~NativeExternalJoinProbe(){try{closeStopped();}catch(...){std::terminate();}}
};
}

