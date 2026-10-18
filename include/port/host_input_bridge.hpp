#pragma once
#include "host_input_router.hpp"
#include "input_mailbox.hpp"
#include <functional>
namespace partyboard::net {
// Transport sender must copy bytes and return promptly. Call pump/sendLocal on
// the game thread; RTC callbacks call enqueue only. Authentication, agreed start
// and verified replacement reconciliation belong to the session coordinator.
class HostInputBridge {
public:
    struct Binding { uint8_t slot; InputMailbox::Route route; };
    using Sender = std::function<bool(uint8_t, std::span<const uint8_t>)>;
    HostInputBridge(SessionId session, uint8_t hostSlot, uint8_t activeSlots, Sender send)
        : router_(session,hostSlot,activeSlots), host_(hostSlot), active_(activeSlots), send_(std::move(send)) {
        if (!send_) throw std::invalid_argument("Input sender required");
    }
    Binding attach(uint8_t authenticatedSlot) {
        if (authenticatedSlot>=4 || authenticatedSlot==host_ || !(active_&(1u<<authenticatedSlot)))
            throw std::invalid_argument("Assigned remote slot required");
        return {authenticatedSlot, mailbox_.bind(authenticatedSlot+1)};
    }
    void retire(Binding binding) { mailbox_.retire(binding.route); }
    InputMailbox::Push enqueue(Binding binding, std::span<const uint8_t> packet) {
        if (binding.route.peer != uint32_t(binding.slot+1) || !router_.remote(binding.slot,packet))
            return InputMailbox::Push::Malformed;
        return mailbox_.push(binding.route,packet);
    }
    bool sendLocal(std::span<const uint8_t> packet) {
        const auto route=router_.local(packet);
        if (!route || failed_) return false;
        return broadcast(route->recipients,packet);
    }
    template<class Receiver> bool pump(Receiver& receiver) {
        if (failed_) return false;
        struct Relay {
            Receiver& receiver; HostInputBridge& bridge;
            Receive receive(uint32_t owner, std::span<const uint8_t> packet) {
                const auto route=bridge.router_.remote(uint8_t(owner-1),packet);
                if (!route || route->owner!=owner) return Receive::Unauthorized;
                const auto result=receiver.receive(owner,packet);
                bridge.lastReceive_=result;
                bridge.lastInput_=decode(packet);
                if (result==Receive::Accepted || result==Receive::Duplicate || result==Receive::Stale) {
                    if (!bridge.broadcast(route->recipients,packet)) return Receive::Halted;
                }
                return result;
            }
        } relay{receiver,*this};
        if (!mailbox_.drain(relay)) failed_=true;
        return !failed_;
    }
    Receive lastReceive() const { return lastReceive_; }
    const std::optional<Input>& lastInput() const { return lastInput_; }
    const char* failureReason() const { return failureReason_; }
private:
    bool broadcast(uint8_t mask,std::span<const uint8_t> packet) {
        try {
            for(uint8_t slot=0;slot<4;++slot)if(mask&(1u<<slot))if(!send_(slot,packet)) { failureReason_="relay-send-rejected";failed_=true;return false; }
        } catch (...) { failureReason_="relay-send-exception";failed_=true;return false; }
        return true;
    }
    HostInputRouter router_;
    InputMailbox mailbox_;
    uint8_t host_, active_;
    Sender send_;
    bool failed_{}; // Game-thread-only state; callbacks never read or mutate it.
    Receive lastReceive_=Receive::Accepted;
    std::optional<Input> lastInput_;
    const char* failureReason_="mailbox-drain-rejected";
};
}
