#pragma once
#include "port/input_mailbox.hpp"
#include <functional>
namespace partyboard::net {
// Star-topology guest route. Only the original authenticated host PAD channel
// may call enqueueHost; RTC callbacks enqueue and the game thread drains.
class GuestInputBridge {
public:
    struct Diagnostics {
        std::array<uint64_t,4> received{};
        std::array<std::optional<uint64_t>,4> lastReceivedFrame{};
        uint64_t rejected=0,sendAttempts=0,sendFailures=0;
        std::optional<uint64_t> lastSentFrame;
    };
private:
    SessionId session_;
    uint8_t local_,active_;
    InputMailbox mailbox_;
    std::array<std::optional<InputMailbox::Route>,4> routes_;
    std::function<bool(std::span<const uint8_t>)> send_;
    bool stopped_=false;
    mutable std::mutex diagnosticsMutex_;
    Diagnostics diagnostics_;
public:
    GuestInputBridge(SessionId session,uint8_t local,uint8_t active,
            std::function<bool(std::span<const uint8_t>)> send)
        :session_(session),local_(local),active_(active),send_(std::move(send)){
        bool nonzero=false;for(const auto byte:session)nonzero|=byte!=0;
        if(!nonzero||local==0||local>=4||(active&0xf0)||!(active&1)||!(active&(1u<<local))||!send_)
            throw std::invalid_argument("Original guest PAD membership required");
        for(uint8_t slot=0;slot<4;++slot)if(slot!=local&&(active&(1u<<slot)))routes_[slot]=mailbox_.bind(slot+1);
    }
    InputMailbox::Push enqueueHost(std::span<const uint8_t> bytes){
        const auto input=decode(bytes);
        if(!input||input->session!=session_||input->slot==local_||!routes_[input->slot]){
            std::lock_guard lock(diagnosticsMutex_);++diagnostics_.rejected;
            return InputMailbox::Push::Malformed;
        }
        const auto result=mailbox_.push(*routes_[input->slot],bytes);
        std::lock_guard lock(diagnosticsMutex_);
        if(result==InputMailbox::Push::Queued){
            ++diagnostics_.received[input->slot];diagnostics_.lastReceivedFrame[input->slot]=input->frame;
        }else ++diagnostics_.rejected;
        return result;
    }
    bool sendLocal(std::span<const uint8_t> bytes){
        const auto input=decode(bytes);
        if(stopped_||!input||input->session!=session_||input->slot!=local_)return false;
        bool sent=false;try{sent=send_(bytes);}catch(...){}
        std::lock_guard lock(diagnosticsMutex_);++diagnostics_.sendAttempts;
        if(sent)diagnostics_.lastSentFrame=input->frame;else ++diagnostics_.sendFailures;
        return sent;
    }
    Diagnostics diagnostics()const{std::lock_guard lock(diagnosticsMutex_);return diagnostics_;}
    template<class Receiver>bool pump(Receiver& receiver){
        if(stopped_||!mailbox_.drain(receiver)){stopped_=true;return false;}return true;
    }
    // After stopped engine/input detach. Retirement rejects late callback data.
    void closeStopped(){
        stopped_=true;for(auto& route:routes_)if(route)mailbox_.retire(*route);
    }
};
}
