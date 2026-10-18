#pragma once
#include "native_join_preparation.hpp"
#include "native_guest_recovery_window.hpp"
#include <port/native_probe_pipe.hpp>
namespace partyboard::ui {
// Local diagnostic commands only inject ordinary menu events. Peer authority
// remains in the original invitation/session/receipt/Start coordinators.
class NativeJoinMenuProbe {
    NativeJoinLobby& document_;
    NativeJoinPreparation& preparation_;
    net::NativeProbePipe pipe_;
    uint64_t updates_=0,stageUpdate_=0,readyUpdate_=0;
    unsigned stage_=0;
    bool preparedEmitted_=false,readyRequested_=false,readySent_=false,runRequested_=false,stopped_=false;
    std::optional<uint64_t> revision_;
    unsigned recoveryStage_=0;uint64_t recoveryUpdate_=0;
    nlohmann::ordered_json emittedRecovery_;
    static void key(SDL_Keycode code,SDL_Scancode scan){SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=code;event.key.scancode=scan;SDL_PushEvent(&event);event.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&event);}
public:
    static void emit(const nlohmann::ordered_json& event){fprintf(stdout,"Native external Join event: %s\n",event.dump().c_str());fflush(stdout);}
    NativeJoinMenuProbe(NativeJoinLobby& document,NativeJoinPreparation& preparation,const std::string& build)
        :document_(document),preparation_(preparation){emit({{"type","join-awaiting-invitation"},{"buildId",build}});}
    bool runRequested()const{return runRequested_;}
    bool stopped()const{return stopped_;}
    bool pumpGameplay(net::NativeGuestGameInput& input,NativeGuestRecoveryWindow& recovery,
            std::function<bool(const nlohmann::ordered_json&)> diagnosticCommand={}){
        ++updates_;
        // A completed reply belongs to one recovery round. Only reopen the
        // diagnostic invitation slot after the original session has resumed.
        if(recoveryStage_==5&&!input.recoveryStatus().at("paused").get<bool>())
            recoveryStage_=0;
        for(const auto& command:pipe_.poll()){
            if(command=="stop"){stopped_=true;PartyBoard_GameRequestStop();return false;}
            if(command=="disconnect"){input.disconnectDiagnostic();emit({{"type","join-disconnected"},{"accepted",input.observations().acceptedCount()}});continue;}
            const auto packet=nlohmann::ordered_json::parse(command);
            if(diagnosticCommand&&diagnosticCommand(packet))continue;
            if(packet.at("type")!="join-recovery-offer"||recoveryStage_)throw std::invalid_argument("Single diagnostic recovery invitation required");
            const auto code=packet.at("offer").dump();
            if(code.size()>110000||!document_.writeClipboard(code))throw std::runtime_error("Reconnect invitation clipboard failed");
            recoveryStage_=1;
        }
        auto state=input.recoveryStatus();state.erase("remainingMs");
        if(state.at("owner")!=0&&state!=emittedRecovery_){emittedRecovery_=state;emit({{"type","join-recovery"},{"status",state}});}
        if(recoveryStage_==1&&recovery.visible()){key(SDLK_RETURN,SDL_SCANCODE_RETURN);recoveryStage_=2;}
        else if(recoveryStage_==2&&recovery.replyAvailable()){key(SDLK_DOWN,SDL_SCANCODE_DOWN);recoveryStage_=3;recoveryUpdate_=net::NativeEngineInputIO::monotonicNow();}
        else if(recoveryStage_==3&&net::NativeEngineInputIO::monotonicNow()>=recoveryUpdate_+200){key(SDLK_RETURN,SDL_SCANCODE_RETURN);recoveryStage_=4;recoveryUpdate_=net::NativeEngineInputIO::monotonicNow();}
        else if(recoveryStage_==4&&net::NativeEngineInputIO::monotonicNow()>=recoveryUpdate_+200){
            std::string code=document_.readClipboard();
            const auto answer=nlohmann::ordered_json::parse(code);
            emit({{"type","join-recovery-answer"},{"answer",answer}});recoveryStage_=5;
        }
        return true;
    }
    void pump(){
        ++updates_;
        for(const auto& command:pipe_.poll()){
            if(command=="stop"){stopped_=true;key(SDLK_ESCAPE,SDL_SCANCODE_ESCAPE);continue;}
            if(command=="ready"){
                if(!preparation_.prepared()||readyRequested_)throw std::runtime_error("Prepared menu required for diagnostic Ready");
                readyRequested_=true;continue;
            }
            if(command=="run"){
                if(!preparation_.prepared()||!preparation_.prepared()->committed()||runRequested_)throw std::runtime_error("Committed menu required for diagnostic run");
                runRequested_=true;continue;
            }
            const auto packet=nlohmann::ordered_json::parse(command);
            if(packet.at("type")!="join-invitation"||stage_||document_.session())throw std::invalid_argument("Single menu invitation required");
            const auto code=packet.at("code").get<std::string>();
            if(code.empty()||code.size()>110000||!document_.writeClipboard(code))throw std::runtime_error("Diagnostic invitation clipboard failed");
            key(SDLK_RETURN,SDL_SCANCODE_RETURN);stage_=1;
        }
        auto* session=document_.session();
        if(stage_==1&&session&&session->transport().reply()){key(SDLK_DOWN,SDL_SCANCODE_DOWN);stage_=2;stageUpdate_=updates_;}
        else if(stage_==2&&updates_>=stageUpdate_+2){key(SDLK_RETURN,SDL_SCANCODE_RETURN);stage_=3;stageUpdate_=updates_;}
        else if(stage_==3&&updates_>=stageUpdate_+2){
            std::string code=document_.readClipboard();
            (void)net::receiveNativeInvitation(code,"answer",session->transport().assignedPeer());
            emit({{"type","join-reply"},{"peer",session->transport().assignedPeer()},{"code",code}});stage_=4;
        }
        auto* prepared=preparation_.prepared();
        if(prepared&&!preparedEmitted_){preparedEmitted_=true;emit({{"type","join-engine-prepared"},{"peer",session->transport().assignedPeer()},{"receipt",preparation_.receipt()},{"music",document_.musicState()}});}
        if(prepared&&prepared->hasSnapshot()&&revision_!=prepared->snapshot().revision){revision_=prepared->snapshot().revision;emit({{"type","join-lobby"},{"revision",*revision_},{"membership",prepared->snapshot().membership}});}
        if(readyRequested_&&prepared&&prepared->hasSnapshot()&&!readySent_){readySent_=true;readyUpdate_=updates_;}
        if(readySent_&&updates_==readyUpdate_+2)key(SDLK_DOWN,SDL_SCANCODE_DOWN);
        if(readySent_&&updates_==readyUpdate_+4)key(SDLK_RETURN,SDL_SCANCODE_RETURN);
    }
};
}
