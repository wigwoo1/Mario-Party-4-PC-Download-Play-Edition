#pragma once
#include "port/native_control_mailbox.hpp"
#include <rtc/rtc.hpp>
#include <atomic>
#include <memory>

namespace partyboard::net {
// One authenticated control transport owner; coordinator retains the shared
// mailbox and checks connected() before readiness/start delivery. This owner
// never installs callbacks on the binary gameplay input channel.
class NativeRtcControlChannel {
    struct State {
        std::shared_ptr<NativeControlMailbox> mailbox;
        NativeControlMailbox::Route route;
        std::atomic<bool> closed{false};
    };
    std::shared_ptr<State> state_;
    std::shared_ptr<rtc::DataChannel> channel_;
public:
    enum class Purpose { Diagnostic, Preparation, Start, Lobby, Recovery };
    static const char* label(Purpose purpose){
        switch(purpose){
        case Purpose::Diagnostic:return "partyboard-control-v1";
        case Purpose::Preparation:return "partyboard-preparation-v1";
        case Purpose::Start:return "partyboard-start-v1";
        case Purpose::Lobby:return "partyboard-lobby-v1";
        case Purpose::Recovery:return "partyboard-recovery-v1";
        }
        throw std::invalid_argument("Unknown native control channel purpose");
    }
    // Each purpose retains its own mailbox so one peer's preparation route
    // cannot replace its start/lobby route. Labels match the browser frontend.
    NativeRtcControlChannel(std::shared_ptr<NativeControlMailbox> mailbox,
            const std::string& authenticatedPeer,std::shared_ptr<rtc::DataChannel> channel,
            Purpose purpose=Purpose::Diagnostic){
        if(!mailbox||!channel||channel->label()!=label(purpose))
            throw std::invalid_argument("Dedicated authenticated native control channel required");
        state_=std::make_shared<State>();state_->mailbox=std::move(mailbox);
        state_->route=state_->mailbox->bind(authenticatedPeer);channel_=std::move(channel);
        const std::weak_ptr<State> weak=state_;
        channel_->onMessage([weak](rtc::message_variant message){
            if(const auto state=weak.lock();state&&!state->closed){
                const auto text=std::get_if<std::string>(&message);
                if(!text){state->closed=true;state->mailbox->retire(state->route);return;}
                const auto result=state->mailbox->push(state->route,*text);
                if(result==NativeControlMailbox::Push::Malformed||result==NativeControlMailbox::Push::Overflow)
                    state->closed=true;
            }
        });
        channel_->onClosed([weak]{if(const auto state=weak.lock()){state->closed=true;state->mailbox->retire(state->route);}});
    }
    NativeRtcControlChannel(const NativeRtcControlChannel&)=delete;
    NativeRtcControlChannel& operator=(const NativeRtcControlChannel&)=delete;
    ~NativeRtcControlChannel(){close();}
    bool connected()const{return state_&&!state_->closed&&channel_&&channel_->isOpen();}
    bool send(const std::string& packet){
        if(packet.empty()||packet.size()>NativeControlMailbox::PacketLimit||!connected())return false;
        try{channel_->send(packet);return true;}catch(...){state_->closed=true;return false;}
    }
    void close(){
        if(!state_)return;
        state_->closed=true;state_->mailbox->retire(state_->route);
        if(channel_){channel_->resetCallbacks();channel_->close();channel_.reset();}
        state_.reset();
    }
};
}
