#pragma once
#include "window.hpp"
#include <port/native_host_lobby_session.hpp>
#include <port/native_package_preparation.hpp>
#include <port/native_prepared_start.hpp>
#include <port/native_host_game_input.hpp>
#include <port/native_restart_intent.hpp>
#include <port/native_room_host_lobby.hpp>
#include <cstdlib>
#include <SDL3/SDL_clipboard.h>
#include <fmt/format.h>

namespace partyboard::ui {
// Fresh-process frontend. Keep this document/session alive through the later
// prepared start coordinator; closing it destroys all pre-game connections.
class NativeHostLobby : public WindowSmall {
    net::NativeHostLobbySession session_;
    std::unique_ptr<net::NativeRoomHostLobby> room_;
    bool manualLobby_=false,hostPublished_=false,automaticStartRequested_=false,automaticStartProposed_=false;
    Button* startButton_=nullptr;
    std::vector<std::unique_ptr<Button>> buttons_;
    Rml::Element* status_=nullptr;
    Rml::Element* players_=nullptr;
    Rml::Element* sessionEndedNotice_=nullptr;
    std::string pendingPeer_,message_;
    unsigned assigned_=0;
    bool cancelled_=false;
    unsigned updates_=0;
    std::filesystem::path packagePath_;
    std::unique_ptr<net::NativePackagePreparation> packageWorker_;
    std::optional<nlohmann::ordered_json> package_;
    std::string packageError_;
    bool packageStarted_=false;
    bool diagnosticDescriptorSeed_=false;
    template<class Action> void act(Action action){
        try{action();}catch(const std::exception& error){message_=error.what();}
    }
    // Caller observes cancellation before disposing the document/session owner.
    void cancel(){room_.reset();packageWorker_.reset();session_.close();cancelled_=true;hide(false);}
public:
    bool sessionEndedNoticeVisible()const{return sessionEndedNotice_&&sessionEndedNotice_->IsVisible(true)&&sessionEndedNotice_->GetInnerRML()==net::NativeSessionEndedNotice;}
    explicit NativeHostLobby(const std::filesystem::path& disc,std::filesystem::path packagePath={},bool diagnosticDescriptorSeed=false,bool sessionEndedNotice=false,bool manualLobby=false):WindowSmall("modal","modal-dialog"),session_(disc),packagePath_(std::move(packagePath)),diagnosticDescriptorSeed_(diagnosticDescriptorSeed){
        manualLobby_=manualLobby;
        append(mDialog,"div")->SetInnerRML("Online Multiplayer — Host");
        if(sessionEndedNotice){sessionEndedNotice_=append(mDialog,"div");sessionEndedNotice_->SetInnerRML(net::NativeSessionEndedNotice);}
        status_=append(mDialog,"div");players_=append(mDialog,"div");
        auto* actions=append(mDialog,"div");actions->SetClass("modal-actions",true);
        auto button=[&](const std::string& text,ButtonCallback callback){
            auto item=std::make_unique<Button>(actions,text);item->root()->SetClass("modal-btn",true);
            item->on_pressed(std::move(callback));buttons_.push_back(std::move(item));
        };
        if(manualLobby_){
        button("Copy invitation",[this]{act([this]{
            if(!session_.prepared())throw std::runtime_error("Preparing game assets. Please wait.");
            if(pendingPeer_.empty()){
                if(assigned_==3)throw std::runtime_error("All three guest slots are assigned.");
                pendingPeer_=session_.addGuest("Guest "+std::to_string(assigned_+1));
                ++assigned_;
            }
            const auto invitation=session_.invitation(pendingPeer_);
            if(!invitation)throw std::runtime_error("Preparing invitation. Select Copy invitation again shortly.");
            if(!SDL_SetClipboardText(invitation->c_str()))throw std::runtime_error("Could not copy invitation.");
            message_="Invitation copied. Share it with this guest, then paste their reply.";
        });});
        button("Paste guest reply",[this]{act([this]{
            if(pendingPeer_.empty())throw std::runtime_error("Create a guest invitation first.");
            char* clipboard=SDL_GetClipboardText();
            if(!clipboard)throw std::runtime_error("Could not read clipboard.");
            std::string reply(clipboard);SDL_free(clipboard);
            const auto first=reply.find_first_not_of(" \t\r\n"),last=reply.find_last_not_of(" \t\r\n");
            reply=first==std::string::npos?std::string{}:reply.substr(first,last-first+1);
            session_.acceptReply(pendingPeer_,reply);pendingPeer_.clear();message_="Guest reply accepted.";
        });});
        button("Toggle ready",[this]{act([this]{toggleReady();});});
        }else{
            button("Paste lobby website",[this]{act([this]{
                if(room_)throw std::runtime_error("Cancel this lobby before changing its website.");
                char* raw=SDL_GetClipboardText();if(!raw)throw std::runtime_error("Could not read clipboard.");
                const std::string website(raw);SDL_free(raw);openAutomaticWebsite(website);
            });});
            button("Copy lobby link",[this]{act([this]{
                if(!room_)throw std::runtime_error("Choose the lobby website first.");
                const auto link=room_->joinLink();if(!SDL_SetClipboardText(link.c_str()))throw std::runtime_error("Could not copy lobby link.");
                message_="Lobby link copied. Share it privately with up to three guests.";
            });});
            if(const auto* website=std::getenv("PARTYBOARD_LOBBY_URL"))act([this,website]{openAutomaticWebsite(website);});
            else message_="Choose the shared lobby website, then copy your lobby link. A public lobby website has not been configured yet.";
        }
        button("Start",[this]{act([this]{proposeStart();});});
        startButton_=buttons_.back().get();if(!manualLobby_)startButton_->set_disabled(true);
        button("Cancel",[this]{cancel();});
    }
    void openAutomaticWebsite(const std::string& website){
        room_=std::make_unique<net::NativeRoomHostLobby>(session_,net::NativeRoomAddress::parse(website));
        message_="Opening the lobby. Game assets and known shaders prepare automatically.";
    }
    net::NativeRoomHostLobby* automaticRoom(){return room_.get();}
    bool automatic()const{return !manualLobby_;}
    void diagnosticPressAutomaticStart(){
        if(manualLobby_||!startButton_||startButton_->disabled())throw std::runtime_error("Enabled automatic Start button required");
        startButton_->focus();SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=SDLK_RETURN;event.key.scancode=SDL_SCANCODE_RETURN;SDL_PushEvent(&event);event.type=SDL_EVENT_KEY_UP;SDL_PushEvent(&event);
    }
    bool cancelled()const{return cancelled_;}
    bool prepared()const{return session_.prepared()&&(packagePath_.empty()||package_.has_value());}
    const std::string& error()const{return packageError_.empty()?session_.error():packageError_;}
    void setPipelineSeed(std::shared_ptr<const net::NativePipelineSeed> seed){session_.setPipelineSeed(std::move(seed));}
    unsigned updates()const{return updates_;}
    const nlohmann::ordered_json& measuredPackage()const{
        if(!package_||cancelled_||!error().empty())throw std::runtime_error("Native lobby runtime package unavailable");
        return *package_;
    }
    void publishHostPrepared(const net::PreparedStartRuntime& captured,const nlohmann::ordered_json& receipt,std::optional<std::string> installedPipelineSeed=std::nullopt){
        session_.publishHostPrepared(captured,receipt,std::move(installedPipelineSeed));
        hostPublished_=true;message_="Host shaders prepared. Waiting for guests.";
        printf("Native owned lobby prepared snapshot: %s\n",session_.snapshot().dump().c_str());
    }
    void toggleReady(){
        const auto& package=measuredPackage();
        const auto& configuration=package.at("configuration");
        const auto captured=net::captureNativePreparedStart(configuration.at("assetId"),package.at("buildId"));
        session_.setHostReady(captured,!session_.hostReady());
        message_=session_.hostReady()?"Host ready. Waiting for guests.":"Host is not ready.";
    }
    bool hostReady()const{return session_.hostReady();}
    void proposeStart(){
        if(!manualLobby_){if(!room_)throw std::runtime_error("Choose the lobby website first.");room_->requestSeal();automaticStartRequested_=true;message_="Confirming the synchronized start with every player.";return;}
        proposePreparedStart();
    }
    void proposePreparedStart(){
        const auto& package=measuredPackage();
        const auto captured=net::captureNativePreparedStart(package.at("configuration").at("assetId"),package.at("buildId"));
        session_.proposePreparedStart(captured);
        message_="Waiting for player start acknowledgments.";
    }
    bool startAgreed()const{return session_.acknowledgmentsComplete();}
    std::unique_ptr<net::NativeHostGameInput> createGameInput(const net::PreparedStartRuntime& captured,std::function<bool()> pump){
        return std::make_unique<net::NativeHostGameInput>(session_,captured,std::move(pump));
    }
    // Diagnostic harness uses the same owned session/assigned channels as the
    // clipboard actions. This never supplies receipts or readiness for guests.
    std::string diagnosticAssignGuest(){
        if(!prepared()||assigned_>=3)throw std::runtime_error("Native diagnostic guest unavailable");
        const auto peer=session_.addGuest("Guest "+std::to_string(assigned_+1));
        ++assigned_;return peer;
    }
    std::optional<nlohmann::ordered_json> diagnosticInvitation(const std::string& peer)const{
        const auto code=session_.invitation(peer);
        if(!code)return std::nullopt;
        return nlohmann::ordered_json{{"type","initial-invitation"},{"peer",peer},{"code",*code},{"manifest",session_.manifest()}};
    }
    void diagnosticAcceptReply(const std::string& peer,const std::string& code){session_.acceptReply(peer,code);}
    nlohmann::ordered_json diagnosticSnapshot()const{return session_.snapshot();}
    nlohmann::ordered_json diagnosticConnectionRoutes()const{
        nlohmann::ordered_json routes=nlohmann::ordered_json::object(),mapping=nlohmann::ordered_json::object();
        const auto snapshot=session_.snapshot();
        for(const auto& player:snapshot.at("players")){
            const auto peer=player.at("peer").get<std::string>();
            if(peer!="host"){
                routes[peer]=net::nativeConnectionRouteLabel(session_.connectionRoute(peer));
                mapping[peer]=session_.routerPreparation(peer);
            }
        }
        return {{"routes",routes},{"renderedRows",players_->GetInnerRML()},{"routerEnabled",session_.routerEnabled()},{"mappingPreparation",mapping}};
    }
    unsigned diagnosticUpdates()const{return updates_;}
    bool diagnosticConnected(const std::string& peer)const{return session_.connected(peer);}
    bool diagnosticAssetComplete(const std::string& peer)const{return session_.assetsComplete(peer);}
    uint64_t diagnosticSentBytes(const std::string& peer)const{return session_.assetSentBytes(peer);}
    bool diagnosticReceiptsComplete()const{return session_.preparedReceiptsComplete();}
    nlohmann::ordered_json diagnosticReceipts()const{return session_.preparedReceipts();}
    void probeUnacknowledgedInput(const net::PreparedStartRuntime& captured){
        const auto before=session_.snapshot();
        bool startRefused=false;
        try{session_.proposePreparedStart(captured);}catch(const std::exception&){startRefused=true;}
        if(!startRefused||session_.snapshot()!=before||startAgreed())throw std::runtime_error("Native start without prepared guests accepted");
        bool refused=false;
        try{net::NativeHostGameInput input(session_,captured,[]{return true;});}
        catch(const std::exception&){refused=true;}
        if(!refused||PartyBoardNativeOnlineInputRequired())throw std::runtime_error("Unacknowledged native gameplay input accepted");
    }
    bool focus()override{return !buttons_.empty()&&buttons_.front()->focus();}
    void update()override{
        if(cancelled_||!visible())return;
        ++updates_;
        if(room_)room_->tick();
        if(!session_.tick())message_=session_.error();
        if(session_.prepared()&&!packagePath_.empty()&&packageError_.empty()){
            try{
                if(!packageStarted_){
                    packageStarted_=true;
                    if(std::filesystem::file_size(packagePath_)>262144)throw std::runtime_error("Runtime package descriptor exceeds bound");
                    std::ifstream input(packagePath_);nlohmann::ordered_json descriptor;input>>descriptor;
                    std::vector<net::NativeRuntimeArtifact> files;
                    for(const auto& file:descriptor.at("files"))files.push_back({file.at("name"),net::nativeRuntimeArtifactPath(packagePath_,file.at("path").get<std::string>())});
                    packageWorker_=std::make_unique<net::NativePackagePreparation>(std::move(files),descriptor.at("configuration").at("buildId"),session_.manifest().at("id"),diagnosticDescriptorSeed_?std::optional<uint32_t>(descriptor.at("configuration").at("seed").get<uint32_t>()):std::nullopt);
                }
                if(packageWorker_&&packageWorker_->done()){
                    const auto result=packageWorker_->result();
                    if(!result)throw std::runtime_error(packageWorker_->error());
                    session_.retainConfiguration(result->at("configuration"),result->at("buildId"));
                    package_=*result;packageWorker_.reset();
                }
            }catch(const std::exception& error){packageError_=error.what();message_=packageError_;packageWorker_.reset();}
        }
        if(!manualLobby_&&session_.prepared())act([this]{
            if(hostPublished_&&!automaticStartProposed_&&!session_.hostReady())toggleReady();
            const auto snapshot=session_.snapshot();
            startButton_->set_disabled(!room_||!room_->ready()||!room_->idle()||snapshot.at("canStart")!=true||automaticStartRequested_);
            if(automaticStartRequested_&&!automaticStartProposed_&&room_&&room_->sealed()&&snapshot.at("canStart")==true&&session_.preparedReceiptsComplete()){
                proposePreparedStart();room_->freeze();automaticStartProposed_=true;
            }
            if(room_&&!room_->error().empty())message_=room_->error();
        });
        status_->SetInnerRML(escape(message_.empty()?(session_.prepared()?"Waiting for guests and shader preparation.":"Preparing game assets..."):message_));
        if(!session_.prepared())return;
        std::string rows;
        const auto snapshot=session_.snapshot();
        for(const auto& player:snapshot.at("players")){
            const auto peer=player.at("peer").get<std::string>();
            const auto route=peer=="host"?"Local host":net::nativeConnectionRouteLabel(session_.connectionRoute(peer));
            rows+=escape(fmt::format("{}: {:.0f}% — {} · {}",player.at("name").get<std::string>(),player.at("progress").get<double>()*100.,player.at("phase").get<std::string>(),route))+"<br/>";
        }
        players_->SetInnerRML(rows);
    }
protected:
    bool handle_nav_command(Rml::Event& event,NavCommand command)override{
        if(command==NavCommand::Cancel||command==NavCommand::Menu){cancel();return true;}
        const int direction=(command==NavCommand::Down||command==NavCommand::Right)?1:
            (command==NavCommand::Up||command==NavCommand::Left)?-1:0;
        if(direction){
            for(int index=0;index<int(buttons_.size());++index)if(buttons_[index]->contains(event.GetTargetElement())){
                const int next=index+direction;
                if(next>=0&&next<int(buttons_.size()))buttons_[next]->focus();
                return true;
            }
        }
        return Document::handle_nav_command(event,command);
    }
};
}

