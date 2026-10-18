#pragma once
#include <string>
#include <map>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <cstdint>

namespace partyboard::net {
// Game-thread coordinator binds/retires authenticated peers and consumes events.
// Network callbacks only enqueue text; JSON parsing and state changes occur on
// the coordinator thread. Control traffic never enters the PAD input mailbox.
class NativeControlMailbox {
public:
    struct Route {std::string peer;uint64_t generation;};
    struct Message {Route route;std::string text;};
    enum class Push {Queued,Retired,Malformed,Overflow};
    static constexpr size_t PacketLimit=65536,ByteLimit=262144,MessageLimit=32;
    Route bind(const std::string& authenticatedPeer){
        std::lock_guard lock(mutex_);
        if(closed_)throw std::runtime_error("Native control owner closed");
        if(authenticatedPeer.empty()||authenticatedPeer.size()>128||
            (!routes_.contains(authenticatedPeer)&&routes_.size()>=4))
            throw std::invalid_argument("Bounded authenticated control peer required");
        if(generation_==UINT64_MAX)throw std::overflow_error("Control route generation exhausted");
        Route route{authenticatedPeer,++generation_};routes_[route.peer]=route.generation;
        discardRetired();return route;
    }
    void retire(const Route& route){std::lock_guard lock(mutex_);if(current(route)){routes_.erase(route.peer);discardRetired();}}
    Push push(const Route& route,const std::string& text){
        std::lock_guard lock(mutex_);
        if(!current(route)||closed_)return Push::Retired;
        if(text.empty()||text.size()>PacketLimit)return Push::Malformed;
        if(failed_||messages_.size()>=MessageLimit||text.size()>ByteLimit-bytes_){failed_=true;return Push::Overflow;}
        messages_.push_back({route,text});bytes_+=text.size();return Push::Queued;
    }
    std::optional<Message> take(){
        std::lock_guard lock(mutex_);
        if(failed_)throw std::runtime_error("Native control message loss; cancel agreement");
        if(closed_)return std::nullopt;
        discardRetired();if(messages_.empty())return std::nullopt;
        auto message=std::move(messages_.front());messages_.pop_front();bytes_-=message.text.size();return message;
    }
    void close(){std::lock_guard lock(mutex_);closed_=true;routes_.clear();messages_.clear();bytes_=0;}
private:
    bool current(const Route& route)const{const auto found=routes_.find(route.peer);return found!=routes_.end()&&found->second==route.generation;}
    void discardRetired(){for(auto it=messages_.begin();it!=messages_.end();){if(!current(it->route)){bytes_-=it->text.size();it=messages_.erase(it);}else ++it;}}
    std::mutex mutex_;std::map<std::string,uint64_t> routes_;std::deque<Message> messages_;
    uint64_t generation_=0;size_t bytes_=0;bool failed_=false,closed_=false;
};
}
