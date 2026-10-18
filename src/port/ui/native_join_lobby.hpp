#pragma once
#include "window.hpp"
#include "../native_lobby_music.hpp"
#include <port/settings.h>
#include <port/native_guest_join_session.hpp>
#include <port/native_guest_prepared_lobby.hpp>
#include <port/native_restart_intent.hpp>
#include <port/native_room_client.hpp>
#include <SDL3/SDL_clipboard.h>
#include <fmt/format.h>
#include <functional>
#include <string_view>

namespace partyboard::ui {
class NativeJoinMenuProbe;
struct NativeJoinClipboard {
    std::function<std::string()> read;
    std::function<bool(std::string_view)> write;
};
// Keep the private staging stream alive until this document/session closes.
// Engine preparation and Start are separately coordinated by its caller.
class NativeJoinLobby : public WindowSmall {
    friend class NativeJoinMenuProbe;
    std::ostream& staging_;
    NativeLobbyMusic music_;
    std::unique_ptr<net::NativeGuestJoinSession> session_;
    std::unique_ptr<net::NativeRoomClient> room_;
    bool manualLobby_=false,automaticReplySent_=false,gameRunning_=false;
    std::vector<std::unique_ptr<Button>> buttons_;
    Rml::Element* status_;
    Rml::Element* musicStatus_;
    Rml::Element* route_;
    Rml::Element* players_;
    Rml::Element* sessionEndedNotice_=nullptr;
    bool cancelled_=false;
    std::string message_;
    std::string actionError_;
    net::NativeGuestPreparedLobby* prepared_=nullptr;
    Button* readyButton_=nullptr;
    bool readyRequested_=false;
    std::optional<uint64_t> readyMembership_;
    std::unique_ptr<net::NativeGuestLobbySnapshot> downloadingLobby_;
    std::optional<net::NativeGuestInvitation::Message> latestLobby_;
    NativeJoinClipboard clipboard_;
    std::string readClipboard()const{
        if(clipboard_.read)return clipboard_.read();
        char* raw=SDL_GetClipboardText();if(!raw)throw std::runtime_error("Could not read clipboard.");
        std::string text(raw);SDL_free(raw);return text;
    }
    bool writeClipboard(std::string_view text)const{
        if(clipboard_.write)return clipboard_.write(text);
        const std::string owned(text);return SDL_SetClipboardText(owned.c_str());
    }
    void drainDownloadingLobby(){
        while(auto message=session_->takeControl("partyboard-lobby-v1")){
            const auto* text=std::get_if<std::string>(&message->payload);
            if(!text||text->size()>65536)throw std::invalid_argument("Bounded download lobby snapshot required");
            if(!downloadingLobby_){
                const auto snapshot=nlohmann::ordered_json::parse(*text).at("snapshot");
                downloadingLobby_=std::make_unique<net::NativeGuestLobbySnapshot>(snapshot.at("assetId"),snapshot.at("total"),session_->transport().assignedPeer());
            }
            // Progress does not grant readiness. The prepared coordinator binds
            // the retained snapshot to the independently verified configuration.
            downloadingLobby_->receive(*text);latestLobby_=std::move(*message);
        }
    }
    void displayPlayers(const net::PreparedStartLobby& snapshot){
        std::string rows;
        for(const auto& player:snapshot.players){
            const auto label=player.peer==session_->transport().assignedPeer()?"You":player.slot==0?"Host":"Guest";
            const auto state=player.ready?"Ready":!player.verified?"Downloading":!player.shaders?"Preparing":"Not ready";
            rows+=fmt::format("<div>Player {} ({}): {}</div>",unsigned(player.slot)+1,label,state);
        }
        players_->SetInnerRML(rows);
    }
    template<class Action>void act(Action action){try{action();actionError_.clear();}catch(const std::exception& error){actionError_=error.what();}}
    static float selectedMusicGain(){
        const auto& audio=partyboard::getSettings().audio;
        return std::clamp(audio.masterVolume.getValue(),0,100)/100.0f
            *std::clamp(audio.mainMusicVolume.getValue(),0,100)/100.0f;
    }
    void updateMusicVolume(){music_.setGain(selectedMusicGain());}
    void cancel(){room_.reset();music_.stop();prepared_=nullptr;if(session_)session_->close();cancelled_=true;hide(false);}
public:
    bool sessionEndedNoticeVisible()const{return sessionEndedNotice_&&sessionEndedNotice_->IsVisible(true)&&sessionEndedNotice_->GetInnerRML()==net::NativeSessionEndedNotice;}
    bool initialInvitationAvailable()const{return !cancelled_&&!session_&&!buttons_.empty()&&buttons_.front()->root()->IsVisible(true)&&!buttons_.front()->disabled();}
    explicit NativeJoinLobby(std::ostream& staging,NativeJoinClipboard clipboard={},bool sessionEndedNotice=false,bool manualLobby=false):WindowSmall("modal","modal-dialog"),staging_(staging),clipboard_(std::move(clipboard)){
        manualLobby_=manualLobby;
        append(mDialog,"div")->SetInnerRML("Online Multiplayer — Join");
        if(sessionEndedNotice){sessionEndedNotice_=append(mDialog,"div");sessionEndedNotice_->SetInnerRML(net::NativeSessionEndedNotice);}
        status_=append(mDialog,"div");players_=append(mDialog,"div");
        musicStatus_=append(mDialog,"div");
        route_=append(mDialog,"div");
        auto* actions=append(mDialog,"div");actions->SetClass("modal-actions",true);
        auto button=[&](const std::string& text,ButtonCallback callback){auto item=std::make_unique<Button>(actions,text);item->root()->SetClass("modal-btn",true);item->on_pressed(std::move(callback));buttons_.push_back(std::move(item));};
        if(manualLobby_){
        button("Paste host invitation",[this]{act([this]{
            if(session_||cancelled_)throw std::runtime_error("This Join invitation is already assigned.");
            std::string code=readClipboard();
            const auto first=code.find_first_not_of(" \t\r\n"),last=code.find_last_not_of(" \t\r\n");
            code=first==std::string::npos?std::string{}:code.substr(first,last-first+1);
            session_=std::make_unique<net::NativeGuestJoinSession>(code,staging_);
            if(const char* base=SDL_GetBasePath())music_.start(base,partyboard_settings_audio_channels(),selectedMusicGain());
            updateMusicVolume();
            message_="Preparing reply. Copy it shortly and share it with the host.";
        });});
        button("Copy reply",[this]{act([this]{
            if(!session_)throw std::runtime_error("Paste the host invitation first.");
            const auto reply=session_->transport().reply();if(!reply)throw std::runtime_error("Reply is still being prepared.");
            if(!writeClipboard(*reply))throw std::runtime_error("Could not copy reply.");
            message_="Reply copied. Share it with the host to connect.";
        });});
        button("Ready",[this]{act([this]{
            if(!prepared_||!session_||!prepared_->ownsSession(*session_)||!prepared_->hasSnapshot()||prepared_->committed())
                throw std::runtime_error("Wait for game preparation before choosing Ready.");
            prepared_->setReady(!readyRequested_);readyRequested_=!readyRequested_;
            message_=readyRequested_?"Ready. Waiting for the host to start.":"Not ready.";
        });});
        readyButton_=buttons_.back().get();readyButton_->set_disabled(true);
        }else{
            button("Paste lobby link",[this]{act([this]{
                if(room_||session_||cancelled_)throw std::runtime_error("This Join lobby is already assigned.");
                room_=std::make_unique<net::NativeRoomClient>(net::NativeRoomAddress::parse(readClipboard(),true),false);
                message_="Joining the host. Downloads and shader preparation will run automatically.";
            });});
        }
        button("Cancel",[this]{cancel();});
    }
    net::NativeRoomClient* automaticRoom(){return room_.get();}
    void diagnosticPasteAutomaticLink(const std::string& link){
        if(manualLobby_||room_||session_||!writeClipboard(link))throw std::runtime_error("Unassigned automatic Join menu required");
        buttons_.front()->focus();SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=SDLK_RETURN;event.key.scancode=SDL_SCANCODE_RETURN;SDL_PushEvent(&event);event.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&event);
    }
    bool cancelled()const{return cancelled_;}
    nlohmann::ordered_json musicState()const{return {{"active",music_.active()},{"error",music_.error()},{"sourceChannels",music_.sourceChannels()},{"deviceChannels",music_.deviceChannels()},
        {"decodedFrames",{music_.decodedFrames(0),music_.decodedFrames(1)}},{"completedTracks",music_.completedTracks()},{"queuedBytes",music_.queuedBytes()},{"gain",music_.gain()}};}
    net::NativeGuestJoinSession* session(){return session_.get();}
    // Caller retains this owner until detachPrepared(), before gameplay/cleanup.
    void bindPrepared(net::NativeGuestPreparedLobby& prepared){
        if(cancelled_||prepared_||!session_||!prepared.ownsSession(*session_))
            throw std::runtime_error("Original prepared Join document required");
        if(latestLobby_)prepared.receiveSnapshot(*latestLobby_);
        latestLobby_.reset();downloadingLobby_.reset();
        prepared_=&prepared;
    }
    void detachPrepared(){music_.stop();gameRunning_=prepared_&&prepared_->committed();prepared_=nullptr;readyMembership_.reset();readyRequested_=false;if(readyButton_)readyButton_->set_disabled(true);players_->SetInnerRML("");}
    bool focus()override{return !buttons_.empty()&&buttons_.front()->focus();}
    void update()override{
        if(cancelled_||gameRunning_)return;
        // Join owns the device. Join the failed worker before closing its stream.
        if(music_.error()&&music_.active())music_.stop();
        if(music_.active())updateMusicVolume();
        if(prepared_&&prepared_->committed())music_.stop();
        musicStatus_->SetInnerRML(music_.error()?"Lobby music could not play. You can still join the game.":"");
        if(room_&&(!prepared_||!prepared_->committed()))act([this]{
            if(!room_->error().empty())throw std::runtime_error(room_->error());
            while(auto event=room_->take()){
                if(session_||event->at("type")!="signal"||event->at("kind")!="offer"||event->at("from")!="host")throw std::runtime_error("Unexpected host connection message.");
                session_=std::make_unique<net::NativeGuestJoinSession>(event->at("payload"),staging_);
                if(const char* base=SDL_GetBasePath())music_.start(base,partyboard_settings_audio_channels(),selectedMusicGain());
                updateMusicVolume();message_="Connecting to the host.";
            }
            if(session_&&!automaticReplySent_)if(const auto reply=session_->transport().reply()){room_->send("host","answer",*reply);automaticReplySent_=true;}
        });
        if(session_){
            session_->pump();
            route_->SetInnerRML(std::string("Connection to host: ")+net::nativeConnectionRouteLabel(session_->transport().connectionRoute()));
            if(!prepared_){
                try{drainDownloadingLobby();if(downloadingLobby_)displayPlayers(downloadingLobby_->current());}
                catch(const std::exception& error){actionError_=error.what();session_->close();}
            }
            if(!session_->error().empty())message_=session_->error();
            else if(prepared_){
                if(readyButton_)readyButton_->set_disabled(!prepared_->hasSnapshot()||prepared_->committed());
                if(prepared_->hasSnapshot()){
                    const auto membership=prepared_->snapshot().membership;
                    if(readyMembership_&&*readyMembership_!=membership)readyRequested_=false;
                    readyMembership_=membership;
                    if(!manualLobby_&&!prepared_->committed()&&!readyRequested_)act([this]{prepared_->setReady(true);readyRequested_=true;});
                    displayPlayers(prepared_->snapshot());
                }
                message_=prepared_->committed()?"Starting game.":!prepared_->hasSnapshot()?"Waiting for player information.":readyRequested_?"Ready. Waiting for the host to start.":"Game prepared. Choose Ready when you want to play.";
            }
            else if(session_->assetsComplete())message_="Game assets verified. Waiting for game and shader preparation.";
            else if(session_->receivedBytes())message_=fmt::format("Downloading game assets: {:.1f} MiB verified.",double(session_->receivedBytes())/(1024.*1024.));
        }
        status_->SetInnerRML(escape(actionError_.empty()?(message_.empty()?(manualLobby_?"Paste your host's invitation to join.":"Paste your host's lobby link to join."):message_):actionError_));
    }
protected:
    bool handle_nav_command(Rml::Event& event,NavCommand command)override{
        if(command==NavCommand::Cancel||command==NavCommand::Menu){cancel();return true;}
        const int direction=(command==NavCommand::Down||command==NavCommand::Right)?1:(command==NavCommand::Up||command==NavCommand::Left)?-1:0;
        if(direction){for(int i=0;i<int(buttons_.size());++i)if(buttons_[i]->contains(event.GetTargetElement())){
            for(int next=i+direction;next>=0&&next<int(buttons_.size());next+=direction){if(!buttons_[next]->disabled()){buttons_[next]->focus();break;}}return true;}}
        return Document::handle_nav_command(event,command);
    }
};
}
