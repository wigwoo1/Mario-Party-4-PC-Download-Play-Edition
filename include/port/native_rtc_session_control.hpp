#pragma once
#include "port/native_rtc_control_channel.hpp"
#include <array>
#include <map>

namespace partyboard::net {
// Game-thread session owner. Authentication precedes attachment; replacement
// requires explicit retirement by the recovery coordinator. Each protocol has
// an independent mailbox, preserving all three routes for the same guest.
class NativeRtcSessionControl {
public:
    bool hasPeer(const std::string& peer)const{return peers_.contains(peer);}
    using Purpose=NativeRtcControlChannel::Purpose;
private:
    static size_t index(Purpose purpose){
        switch(purpose){case Purpose::Preparation:return 0;case Purpose::Start:return 1;case Purpose::Lobby:return 2;
        default:throw std::invalid_argument("Production session control purpose required");}
    }
    std::array<std::shared_ptr<NativeControlMailbox>,3> mailboxes_{
        std::make_shared<NativeControlMailbox>(),std::make_shared<NativeControlMailbox>(),std::make_shared<NativeControlMailbox>()};
    using Channels=std::array<std::unique_ptr<NativeRtcControlChannel>,3>;
    std::map<std::string,Channels> peers_;
    bool closed_=false;
public:
    void attachAuthenticated(const std::string& peer,Purpose purpose,std::shared_ptr<rtc::DataChannel> channel){
        const auto slot=index(purpose);
        if(closed_)throw std::runtime_error("Native session control closed");
        if(peer.empty()||peer.size()>128||(!peers_.contains(peer)&&peers_.size()>=3))
            throw std::invalid_argument("At most three authenticated guests required");
        const auto found=peers_.find(peer);
        if(found!=peers_.end()&&found->second[slot])throw std::invalid_argument("Duplicate native session channel");
        auto owner=std::make_unique<NativeRtcControlChannel>(mailboxes_[slot],peer,std::move(channel),purpose);
        peers_[peer][slot]=std::move(owner);
    }
    bool connected(const std::string& peer)const{
        const auto found=peers_.find(peer);
        if(closed_||found==peers_.end())return false;
        for(const auto& channel:found->second)if(!channel||!channel->connected())return false;
        return true;
    }
    bool send(const std::string& peer,Purpose purpose,const std::string& text){
        const auto slot=index(purpose);const auto found=peers_.find(peer);
        return !closed_&&found!=peers_.end()&&found->second[slot]&&found->second[slot]->send(text);
    }
    std::optional<NativeControlMailbox::Message> take(Purpose purpose){return mailboxes_[index(purpose)]->take();}
    void retireAuthenticated(const std::string& peer){peers_.erase(peer);}
    void close(){if(closed_)return;closed_=true;peers_.clear();for(const auto& mailbox:mailboxes_)mailbox->close();}
    ~NativeRtcSessionControl(){close();}
};
}
