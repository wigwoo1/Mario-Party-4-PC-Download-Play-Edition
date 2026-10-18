#pragma once
#include "window.hpp"
#include <port/native_host_game_input.hpp>
#include <port/native_room_host_lobby.hpp>
#include <SDL3/SDL_clipboard.h>
#include <fmt/format.h>

namespace partyboard::ui {
// Non-owning document; detach before the stopped gameplay owner is destroyed.
class NativeRecoveryWindow : public WindowSmall {
    net::NativeHostGameInput* input_;
    net::NativeRoomHostLobby* room_=nullptr;
    std::set<std::string> automaticVerified_;
    bool automaticFailed_=false,resumeRequested_=false;
    Rml::Element* status_;
    std::vector<std::unique_ptr<Button>> buttons_;
    std::string peer_,message_;
    uint32_t owner_=0;
    bool pending_=false,offered_=false,answered_=false,verifying_=false;
    uint64_t nextUpdate_=0;
    template<class Action>void act(Action action){try{if(!input_)throw std::runtime_error("Online session ended");action();}catch(const std::exception& error){message_=error.what();}}
public:
    explicit NativeRecoveryWindow(net::NativeHostGameInput& input,net::NativeRoomHostLobby* room=nullptr):WindowSmall("modal","modal-dialog"),input_(&input),room_(room){
        append(mDialog,"div")->SetInnerRML("Online Multiplayer — Reconnect");status_=append(mDialog,"div");
        auto* actions=append(mDialog,"div");actions->SetClass("modal-actions",true);
        auto button=[&](const std::string& text,ButtonCallback callback){auto item=std::make_unique<Button>(actions,text);item->root()->SetClass("modal-btn",true);item->on_pressed(std::move(callback));buttons_.push_back(std::move(item));};
        if(!room_){button("Select disconnected player",[this]{act([this]{
            if(pending_)throw std::runtime_error("Finish this player's reconnect first.");
            const auto missing=input_->recoveryStatus().at("missingPeers");if(missing.empty())throw std::runtime_error("No disconnected player.");
            size_t index=0;for(size_t i=0;i<missing.size();++i)if(missing[i].at("peer")==peer_)index=(i+1)%missing.size();
            peer_=missing[index].at("peer");owner_=missing[index].at("owner");message_="Selected "+peer_+".";
            offered_=answered_=verifying_=false;invitation_.clear();
        });});
        button("Copy reconnect invitation",[this]{act([this]{
            if(peer_.empty())throw std::runtime_error("Select a disconnected player first.");
            if(!pending_){input_->beginReplacement(peer_);pending_=true;message_="Preparing reconnect invitation. Select Copy again shortly.";return;}
            if(offered_)throw std::runtime_error("Invitation already copied. Paste this player's reply.");
            const auto offer=input_->replacement(peer_).offer(net::NativeEngineInputIO::monotonicNow());
            if(!offer){message_="Preparing reconnect invitation. Select Copy again shortly.";return;}
            // Preserve publication even if the clipboard write fails.
            invitation_=offer->dump();offered_=true;
            if(!SDL_SetClipboardText(invitation_.c_str()))throw std::runtime_error("Could not copy invitation.");
            message_="Reconnect invitation copied. Share it with "+peer_+", then paste their reply.";
        });});
        button("Copy invitation again",[this]{act([this]{if(invitation_.empty())throw std::runtime_error("Create a reconnect invitation first.");if(!SDL_SetClipboardText(invitation_.c_str()))throw std::runtime_error("Could not copy invitation.");message_="Reconnect invitation copied.";});});
        button("Paste reconnect reply",[this]{act([this]{
            if(!offered_||answered_)throw std::runtime_error("A pending reconnect invitation is required.");
            char* text=SDL_GetClipboardText();if(!text)throw std::runtime_error("Could not read clipboard.");
            std::string reply(text);SDL_free(text);if(reply.empty()||reply.size()>110000)throw std::runtime_error("Paste a valid reconnect reply.");
            input_->replacement(peer_).acceptAnswer(nlohmann::ordered_json::parse(reply),net::NativeEngineInputIO::monotonicNow());
            answered_=true;message_="Reply authenticated. Waiting for connection and paused-state verification.";
        });});
        button("Ask all players to resume",[this]{act([this]{
            if(!input_->replacementVerified(peer_))throw std::runtime_error("Wait for the connection and paused state to be verified.");
            input_->beginRecoveryAgreement(owner_);message_="Aligning all players and waiting for resume acknowledgments.";
        });});
        }
        button("Return to lobby",[this]{act([this]{input_->requestReturnToMenu();});});
    }
    void detach(){input_=nullptr;hide(false);}
    bool focus()override{return !buttons_.empty()&&buttons_.front()->focus();}
    // Called from the retained input callback before the engine render frame.
    // Returning true asks the caller to render the paused menu at bounded rate.
    bool pump(){
        if(!input_)return false;const auto time=net::NativeEngineInputIO::monotonicNow();if(time<nextUpdate_)return false;nextUpdate_=time+100;
        const auto state=input_->recoveryStatus();
        if(room_)room_->tick();
        if(!state.at("paused").get<bool>()){
            if(visible())hide(false);
            pending_=offered_=answered_=verifying_=false;peer_.clear();invitation_.clear();message_.clear();automaticVerified_.clear();automaticFailed_=resumeRequested_=false;return false;
        }
        if(!visible()){show();focus();}
        if(room_){
            if(!automaticFailed_)try{
                if(!room_->error().empty())throw std::runtime_error(room_->error());
                if(peer_.empty())for(const auto& missing:state.at("missingPeers"))if(!automaticVerified_.contains(missing.at("peer").get<std::string>())){
                    peer_=missing.at("peer");owner_=missing.at("owner");break;
                }
                if(!peer_.empty()&&state.contains("frame")){
                    if(!pending_){input_->beginReplacement(peer_);pending_=true;}
                    if(!offered_)if(const auto offer=input_->replacement(peer_).offer(time)){room_->sendRecovery(peer_,*offer);offered_=true;}
                    while(auto reply=room_->takeRecovery()){
                        if(reply->at("peer")!=peer_||!offered_||answered_)throw std::runtime_error("Unexpected original reconnect reply");
                        input_->replacement(peer_).acceptAnswer(nlohmann::ordered_json::parse(reply->at("payload").get<std::string>()),time);answered_=true;
                    }
                    if(answered_&&!verifying_&&input_->replacement(peer_).ready(time)){input_->beginReplacementVerification(peer_);verifying_=true;}
                    if(verifying_&&input_->replacementVerified(peer_)){automaticVerified_.insert(peer_);peer_.clear();pending_=offered_=answered_=verifying_=false;}
                }
                const bool allVerified=!automaticVerified_.empty()&&std::all_of(state.at("missingPeers").begin(),state.at("missingPeers").end(),[&](const auto& missing){return automaticVerified_.contains(missing.at("peer").template get<std::string>());});
                if(allVerified&&!resumeRequested_&&input_->recoveryTransportsReady()){input_->beginRecoveryAgreement(owner_);resumeRequested_=true;}
                message_=resumeRequested_?"Every player is confirming the paused state.":"Reconnecting and verifying the original game automatically.";
            }catch(const std::exception& error){automaticFailed_=true;message_=error.what();}
            status_->SetInnerRML(escape(fmt::format("Game paused. {} seconds remaining. {}",(state.at("remainingMs").get<uint64_t>()+999)/1000,message_)));return true;
        }
        if(peer_.empty()&&!state.at("missingPeers").empty()){peer_=state.at("missingPeers")[0].at("peer");owner_=state.at("missingPeers")[0].at("owner");}
        // Readiness can fail when the original deadline expires or a new
        // channel closes. Keep that failure in the paused UI instead of
        // unwinding the engine's retained input callback.
        if(answered_&&!verifying_)act([this,time]{
            if(input_->replacement(peer_).ready(time)){
                input_->beginReplacementVerification(peer_);verifying_=true;
            }
        });
        if(verifying_&&input_->replacementVerified(peer_)){
            pending_=false;
            message_=state.at("aligning").get<bool>()||state.at("agreement").get<bool>()?
                "Waiting for all players to confirm resume.":"Connection and paused state verified. Reconnect other missing players or ask all players to resume.";
        }
        status_->SetInnerRML(escape(fmt::format("Game paused. Reconnect within {} seconds. Selected player: {}. {}",
            (state.at("remainingMs").get<uint64_t>()+999)/1000,peer_,message_)));
        return true;
    }
protected:
    std::string invitation_;
    bool handle_nav_command(Rml::Event& event,NavCommand command)override{
        if(command==NavCommand::Cancel||command==NavCommand::Menu)return true;
        const int direction=(command==NavCommand::Down||command==NavCommand::Right)?1:(command==NavCommand::Up||command==NavCommand::Left)?-1:0;
        if(direction){for(int i=0;i<int(buttons_.size());++i)if(buttons_[i]->contains(event.GetTargetElement())){
            const int next=i+direction;if(next>=0&&next<int(buttons_.size()))buttons_[next]->focus();return true;}}
        return Document::handle_nav_command(event,command);
    }
};
}
