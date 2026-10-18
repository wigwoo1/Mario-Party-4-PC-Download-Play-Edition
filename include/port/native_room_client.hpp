#pragma once
#include "native_room_http.hpp"
#include "recovery_credentials.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <regex>
#include <thread>

namespace partyboard::net {
struct NativeRoomAddress {
    std::string origin,room,joinKey;
    static NativeRoomAddress parse(std::string text,bool joining=false){
        if(text.size()>2048)throw std::invalid_argument("Lobby link is too long.");
        const auto first=text.find_first_not_of(" \t\r\n"),last=text.find_last_not_of(" \t\r\n");text=first==std::string::npos?std::string{}:text.substr(first,last-first+1);
        static const std::regex pattern(R"(^(https?)://([A-Za-z0-9](?:[A-Za-z0-9.-]*[A-Za-z0-9])?|\[[0-9a-fA-F:]+\])(?::([0-9]{1,5}))?/(?:index\.html)?(?:#room=([A-Za-z0-9_-]{22})&key=([A-Za-z0-9_-]{43}))?$)");
        std::smatch match;if(!std::regex_match(text,match,pattern))throw std::invalid_argument("Use a complete lobby website address or join link.");
        const std::string scheme=match[1],host=match[2],port=match[3];
        if(!port.empty()&&(std::stoul(port)==0||std::stoul(port)>65535))throw std::invalid_argument("Invalid lobby website port.");
        if(scheme!="https"&&host!="127.0.0.1"&&host!="localhost"&&host!="[::1]")throw std::invalid_argument("Remote lobbies require HTTPS.");
        NativeRoomAddress value{scheme+"://"+host+(port.empty()?"":":"+port),match[4],match[5]};
        if(joining!=!value.room.empty())throw std::invalid_argument(joining?"Paste the host's complete lobby join link.":"Choose the lobby website without a private join link.");return value;
    }
};

// Owned worker for the browser-compatible room API. Game/UI callers only enqueue
// bounded commands and consume copied events; no request runs on their thread.
class NativeRoomClient {
public:
    using Json=nlohmann::ordered_json;
    using Http=std::function<NativeRoomHttpResponse(const NativeRoomHttpRequest&,std::stop_token)>;
private:
    struct Command {std::string action,to,kind,payload,id;};
    struct State {
        mutable std::mutex mutex;std::condition_variable_any cv;
        Json identity;std::deque<Json> events;std::deque<Command> commands;
        size_t eventBytes=0,commandBytes=0;std::string error;bool sealed=false,finished=false,retrying=false;
    };
    std::shared_ptr<State> state_=std::make_shared<State>();
    NativeRoomAddress address_;bool hosting_;
    std::jthread worker_;
    static bool token(const std::string& value,size_t size){return value.size()==size&&value.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")==std::string::npos;}
    static bool peer(const std::string& value){return value=="host"||std::regex_match(value,std::regex("guest[1-9][0-9]{0,8}"));}
    static void validateIdentity(const Json& value,bool hosting){
        if(!value.is_object()||value.value("version",0)!=1||!token(value.value("room",std::string{}),22)||!token(value.value("token",std::string{}),43)||!peer(value.value("peer",std::string{}))||
           (value.at("peer")=="host")!=hosting||(hosting&&!token(value.value("joinKey",std::string{}),43)))throw std::runtime_error("Invalid lobby connection identity.");
    }
    void enqueue(Command command){
        std::lock_guard lock(state_->mutex);if(state_->finished||!state_->error.empty()||state_->identity.is_null())throw std::runtime_error("Connect to the lobby service first.");
        const auto bytes=command.payload.size();if(state_->commands.size()>=64||bytes>1024*1024-state_->commandBytes)throw std::runtime_error("Lobby connection queue is full.");
        state_->commandBytes+=bytes;state_->commands.push_back(std::move(command));state_->cv.notify_all();
    }
public:
    explicit NativeRoomClient(NativeRoomAddress address,bool hosting,Http http=nativeRoomHttp):address_(std::move(address)),hosting_(hosting){
        // Do not accept a hand-built address that bypasses URL policy.
        address_=NativeRoomAddress::parse(address_.origin+"/"+(hosting?"":"#room="+address_.room+"&key="+address_.joinKey),!hosting);
        if(!http)throw std::invalid_argument("Native lobby HTTP adapter required.");
        const auto registrationId=createRecoveryNonce();
        worker_=std::jthread([state=state_,address=address_,hosting,registrationId,http=std::move(http)](std::stop_token stop){
            Json identity;uint64_t cursor=0;std::optional<std::chrono::steady_clock::time_point> failureAt;
            const auto pause=[&](unsigned ms,bool commands=true){std::unique_lock lock(state->mutex);state->cv.wait_for(lock,stop,std::chrono::milliseconds(ms),[&]{return commands&&!state->commands.empty();});};
            try{
                while(!stop.stop_requested()){
                    try{
                        Command command;bool hasCommand=false;
                        if(!identity.is_null()){std::lock_guard lock(state->mutex);if(!state->commands.empty()){command=state->commands.front();hasCommand=true;}}
                        NativeRoomHttpRequest request;request.url=address.origin+"/api/lobbies";request.method="POST";request.body=Json{{"version",1},{"id",registrationId}}.dump();
                        if(identity.is_null()){
                            if(!hosting){request.url+="/"+address.room+"/join";request.body=Json{{"version",1},{"joinKey",address.joinKey},{"id",registrationId}}.dump();}
                        }else{
                            request.url+="/"+identity.at("room").get<std::string>();request.bearer=identity.at("token");
                            if(!hasCommand){request.url+="/events?after="+std::to_string(cursor);request.method="GET";request.body.clear();}
                            else if(command.action=="send"){request.url+="/signals";request.body=Json{{"version",1},{"to",command.to},{"kind",command.kind},{"payload",command.payload},{"id",command.id}}.dump();}
                            else if(command.action=="seal"){request.url+="/seal";request.body=Json{{"version",1}}.dump();}
                            else {request.url+="/players/"+command.to;request.method="DELETE";request.body.clear();}
                        }
                        const auto response=http(request,stop);if(stop.stop_requested())break;
                        if(response.status>=500||response.status==429)throw std::runtime_error("Lobby connection service is temporarily unavailable.");
                        if(response.status!=200){
                            // Invalid credentials, expired rooms and history loss require a
                            // fresh lobby, not an endless background retry.
                            std::lock_guard lock(state->mutex);state->error="Lobby service rejected this session (HTTP "+std::to_string(response.status)+"). Open a fresh lobby.";break;
                        }
                        const auto value=Json::parse(response.body);
                        if(value.value("version",0)!=1)throw std::invalid_argument("Unsupported lobby service response.");
                        if(identity.is_null()){
                            validateIdentity(value,hosting);if(!hosting&&value.at("room")!=address.room)throw std::invalid_argument("Lobby service changed the joined room.");
                            identity=value;std::lock_guard lock(state->mutex);state->identity=value;
                        }else if(hasCommand){
                            if(command.action=="send"&&!value.value("sent",false))throw std::invalid_argument("Connection message was not accepted.");
                            if(command.action=="seal"&&!value.value("sealed",false))throw std::invalid_argument("Lobby start was not accepted.");
                            if(command.action=="remove"&&!value.value("closed",false))throw std::invalid_argument("Guest removal was not accepted.");
                            std::lock_guard lock(state->mutex);state->commandBytes-=state->commands.front().payload.size();state->commands.pop_front();if(command.action=="seal")state->sealed=true;
                        }else{
                            if(!value.contains("events")||!value.at("events").is_array()||value.at("events").size()>64||!value.at("cursor").is_number_unsigned())throw std::invalid_argument("Invalid lobby event response.");
                            const auto end=value.at("cursor").get<uint64_t>();if(end<cursor||end>9007199254740991ULL)throw std::invalid_argument("Invalid lobby event cursor.");
                            std::deque<Json> checked;size_t bytes=0;auto next=cursor;
                            for(const auto& event:value.at("events")){
                                const auto sequence=event.at("sequence").get<uint64_t>();if(!event.at("sequence").is_number_unsigned()||sequence<=next||sequence>end)throw std::invalid_argument("Invalid lobby event order.");
                                const auto type=event.at("type").get<std::string>();
                                if(type=="joined"||type=="left"){if(!hosting||!peer(event.at("peer"))||event.at("peer")=="host")throw std::invalid_argument("Invalid lobby membership event.");}
                                else if(type=="signal"){
                                    const std::string from=event.at("from"),kind=event.at("kind"),payload=event.at("payload");
                                    if(!peer(from)||(hosting?(from=="host"||(kind!="answer"&&kind!="recovery-answer")):(from!="host"||(kind!="offer"&&kind!="recovery-offer")))||payload.size()>100000)throw std::invalid_argument("Invalid assigned lobby signal.");
                                }else throw std::invalid_argument("Unknown lobby connection event.");
                                bytes+=event.dump().size();checked.push_back(event);next=sequence;
                            }
                            if(next!=end)throw std::invalid_argument("Incomplete lobby event history.");
                            {std::lock_guard lock(state->mutex);if(checked.size()>128-state->events.size()||bytes>1024*1024-state->eventBytes)throw std::invalid_argument("Lobby event queue exceeds its bounds.");state->eventBytes+=bytes;for(auto& event:checked)state->events.push_back(std::move(event));}cursor=end;
                        }
                        failureAt.reset();{std::lock_guard lock(state->mutex);state->retrying=false;}
                        if(!hasCommand)pause(750);
                    }catch(const nlohmann::json::exception&){throw std::invalid_argument("Invalid JSON lobby response.");}
                    catch(const std::invalid_argument&){throw;}
                    catch(const std::exception&){
                        if(stop.stop_requested())break;failureAt=failureAt.value_or(std::chrono::steady_clock::now());
                        if(std::chrono::steady_clock::now()-*failureAt>=std::chrono::seconds(60))throw std::runtime_error("Lobby connection service remained unavailable for 60 seconds.");
                        {std::lock_guard lock(state->mutex);state->retrying=true;}pause(1000,false);
                    }
                }
            }catch(const std::exception& error){if(!stop.stop_requested()){std::lock_guard lock(state->mutex);state->error=error.what();}}
            // Best-effort bounded revocation; expiry is the fallback if offline.
            if(!identity.is_null())try{http({address.origin+"/api/lobbies/"+identity.at("room").get<std::string>(),"DELETE",identity.at("token"),"",350},{});}catch(...){}
            std::lock_guard lock(state->mutex);state->finished=true;
        });
    }
    NativeRoomClient(const NativeRoomClient&)=delete;
    bool ready()const{std::lock_guard lock(state_->mutex);return !state_->identity.is_null()&&!state_->finished&&state_->error.empty();}
    bool sealed()const{std::lock_guard lock(state_->mutex);return state_->sealed;}
    bool retrying()const{std::lock_guard lock(state_->mutex);return state_->retrying;}
    std::string error()const{std::lock_guard lock(state_->mutex);return state_->error;}
    std::string joinLink()const{std::lock_guard lock(state_->mutex);if(!hosting_||state_->identity.is_null()||state_->finished)throw std::runtime_error("Wait for the host lobby to open.");return address_.origin+"/#room="+state_->identity.at("room").get<std::string>()+"&key="+state_->identity.at("joinKey").get<std::string>();}
    std::optional<Json> take(){std::lock_guard lock(state_->mutex);if(state_->events.empty())return std::nullopt;auto value=std::move(state_->events.front());state_->eventBytes-=value.dump().size();state_->events.pop_front();return value;}
    void send(std::string to,std::string kind,std::string payload){
        if(!peer(to)||payload.size()>100000||(hosting_?(to=="host"||(kind!="offer"&&kind!="recovery-offer")):(to!="host"||(kind!="answer"&&kind!="recovery-answer"))))throw std::invalid_argument("Invalid assigned connection message.");
        enqueue({"send",std::move(to),std::move(kind),std::move(payload),createRecoveryNonce()});
    }
    void seal(){if(!hosting_)throw std::runtime_error("Host lobby required.");enqueue({"seal"});}
    void remove(std::string guest){if(!hosting_||!peer(guest)||guest=="host")throw std::invalid_argument("Host and guest identity required.");enqueue({"remove",std::move(guest)});}
    void close(){worker_.request_stop();state_->cv.notify_all();if(worker_.joinable())worker_.join();std::lock_guard lock(state_->mutex);state_->identity=Json();state_->events.clear();state_->commands.clear();state_->eventBytes=state_->commandBytes=0;}
    ~NativeRoomClient(){close();}
};
}
