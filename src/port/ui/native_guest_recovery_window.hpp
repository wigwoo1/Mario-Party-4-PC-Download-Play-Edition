#pragma once
#include "window.hpp"
#include <port/native_guest_game_input.hpp>
#include <port/native_room_client.hpp>
#include <SDL3/SDL_clipboard.h>
#include <fmt/format.h>
namespace partyboard::ui {
// Original guest owner remains alive until this document is detached.
class NativeGuestRecoveryWindow : public WindowSmall {
    net::NativeGuestGameInput* input_;
    net::NativeRoomClient* room_=nullptr;
    std::optional<nlohmann::ordered_json> automaticOffer_;
    bool automaticFailed_=false;
    Rml::Element* status_;
    std::vector<std::unique_ptr<Button>> buttons_;
    std::string message_,reply_;
    std::function<std::string()> readClipboard_;
    std::function<bool(std::string_view)> writeClipboard_;
    bool pending_=false;
    uint64_t nextUpdate_=0;
    template<class Action>void act(Action action){try{if(!input_)throw std::runtime_error("Online session ended");action();}catch(const std::exception& error){message_=error.what();}}
public:
    explicit NativeGuestRecoveryWindow(net::NativeGuestGameInput& input,
            std::function<std::string()> readClipboard={},std::function<bool(std::string_view)> writeClipboard={},net::NativeRoomClient* room=nullptr)
        :WindowSmall("modal","modal-dialog"),input_(&input),room_(room),readClipboard_(std::move(readClipboard)),writeClipboard_(std::move(writeClipboard)){
        append(mDialog,"div")->SetInnerRML("Online Multiplayer — Reconnect");status_=append(mDialog,"div");
        auto* actions=append(mDialog,"div");actions->SetClass("modal-actions",true);
        auto button=[&](const std::string& text,ButtonCallback callback){auto item=std::make_unique<Button>(actions,text);item->root()->SetClass("modal-btn",true);item->on_pressed(std::move(callback));buttons_.push_back(std::move(item));};
        if(!room_){button("Paste host reconnect invitation",[this]{act([this]{
            if(pending_||!reply_.empty())throw std::runtime_error("A reconnect attempt is already assigned.");
            std::string code;
            if(readClipboard_)code=readClipboard_();
            else {char* raw=SDL_GetClipboardText();if(!raw)throw std::runtime_error("Could not read clipboard.");code=raw;SDL_free(raw);}
            if(code.empty()||code.size()>110000)throw std::runtime_error("Paste a valid reconnect invitation.");
            input_->beginReplacement(nlohmann::ordered_json::parse(code));pending_=true;
            message_="Preparing reconnect reply.";
        });});
        button("Copy reconnect reply",[this]{act([this]{
            if(reply_.empty())throw std::runtime_error("Wait for the reconnect reply to be prepared.");
            if(!(writeClipboard_?writeClipboard_(reply_):SDL_SetClipboardText(reply_.c_str())))throw std::runtime_error("Could not copy reconnect reply.");
            message_="Reply copied. Share it with the host. Waiting for verification and all players.";
        });});
        }
        button("Return to lobby",[]{PartyBoard_GameRequestStop();});
    }
    bool replyAvailable()const{return !reply_.empty();}
    void detach(){input_=nullptr;hide(false);}
    bool focus()override{return !buttons_.empty()&&buttons_.front()->focus();}
    bool pump(){
        if(!input_)return false;
        const auto time=net::NativeEngineInputIO::monotonicNow();if(time<nextUpdate_)return false;nextUpdate_=time+100;
        const auto state=input_->recoveryStatus();
        if(!state.at("paused").get<bool>()){
            if(visible())hide(false);pending_=false;reply_.clear();message_.clear();automaticOffer_.reset();automaticFailed_=false;return false;
        }
        if(!visible()){show();focus();}
        if(room_){
            if(!automaticFailed_)try{
                if(!room_->error().empty())throw std::runtime_error(room_->error());
                while(auto event=room_->take()){
                    if(event->at("kind")!="recovery-offer"||event->at("from")!="host"||pending_||automaticOffer_)throw std::runtime_error("Unexpected host reconnect message");
                    automaticOffer_=nlohmann::ordered_json::parse(event->at("payload").get<std::string>());
                }
                if(automaticOffer_&&input_->canBeginReplacement()){input_->beginReplacement(*automaticOffer_);pending_=true;automaticOffer_.reset();}
                if(pending_&&reply_.empty())if(const auto answer=input_->replacementAnswer()){reply_=answer->dump();room_->send("host","recovery-answer",reply_);}
                message_="Reconnecting and confirming the original paused game automatically.";
            }catch(const std::exception& error){automaticFailed_=true;message_=error.what();}
            status_->SetInnerRML(escape(fmt::format("Game paused. {} seconds remaining. {}",(state.at("remainingMs").get<uint64_t>()+999)/1000,message_)));return true;
        }
        if(pending_&&reply_.empty())act([this]{if(const auto answer=input_->replacementAnswer()){reply_=answer->dump();message_="Reconnect reply prepared. Copy it and share it with the host.";}});
        status_->SetInnerRML(escape(fmt::format("Game paused. {} seconds remaining. {}",(state.at("remainingMs").get<uint64_t>()+999)/1000,message_)));
        return true;
    }
protected:
    bool handle_nav_command(Rml::Event& event,NavCommand command)override{
        if(command==NavCommand::Cancel||command==NavCommand::Menu)return true;
        const int direction=(command==NavCommand::Down||command==NavCommand::Right)?1:(command==NavCommand::Up||command==NavCommand::Left)?-1:0;
        if(direction){for(int i=0;i<int(buttons_.size());++i)if(buttons_[i]->contains(event.GetTargetElement())){const int next=i+direction;if(next>=0&&next<int(buttons_.size()))buttons_[next]->focus();return true;}}
        return Document::handle_nav_command(event,command);
    }
};
}
