#pragma once
#include <rtc/rtc.hpp>
#include "host_input_bridge.hpp"
#include "recoverable_pad_exchange.hpp"
#include "deferred_input_packets.hpp"
#include <memory>
#include <mutex>

namespace partyboard::net {
// Authenticated lobby coordinator supplies channel/slot bindings. Network
// callbacks only enqueue; pump runs on the game thread and never resumes a
// disconnected session. Keep the owner alive until the engine stops/detaches.
class NativeRtcHostInput {
    struct State {
        std::mutex mutex;
        std::array<std::shared_ptr<rtc::DataChannel>,4> channels;
        std::array<uint64_t,4> generations{};
        uint8_t closed = 0;
        bool stopped = false;
        bool malformed = false;
        std::unique_ptr<HostInputBridge> bridge;
        DeferredInputPackets deferred;
    };
    std::shared_ptr<State> state_;
    const char* failureReason_="none";
public:
    const char* failureReason() const { return failureReason_; }
    Receive lastReceive() const { return state_->bridge->lastReceive(); }
    const std::optional<Input>& lastInput() const { return state_->bridge->lastInput(); }
    NativeRtcHostInput(SessionId session,uint8_t host,uint8_t active) : state_(std::make_shared<State>()) {
        std::weak_ptr<State> weak = state_;
        state_->bridge = std::make_unique<HostInputBridge>(session,host,active,
            [weak](uint8_t slot,std::span<const uint8_t> bytes) {
                const auto state = weak.lock();
                if (!state) return false;
                std::shared_ptr<rtc::DataChannel> channel;
                { std::lock_guard lock(state->mutex);
                  if (state->stopped) return false;
                  channel = state->channels[slot]; }
                if (!channel || !channel->isOpen()) {
                    std::lock_guard lock(state->mutex);
                    state->closed |= uint8_t(1u<<slot);
                    return state->deferred.retain(slot,bytes);
                }
                // libdatachannel false means accepted into its buffered queue,
                // not rejected. Retrying that packet would duplicate delivery.
                try { channel->send(reinterpret_cast<const rtc::byte*>(bytes.data()),bytes.size()); }
                catch (...) {
                    if (channel->isOpen()) throw;
                    std::lock_guard lock(state->mutex);
                    state->closed |= uint8_t(1u<<slot);
                    return state->deferred.retain(slot,bytes);
                }
                return true;
            });
    }
    NativeRtcHostInput(const NativeRtcHostInput&) = delete;
    NativeRtcHostInput& operator=(const NativeRtcHostInput&) = delete;
    void attachAuthenticated(uint8_t slot,std::shared_ptr<rtc::DataChannel> channel,
            std::function<void(InputMailbox::Push)> observe = {}) {
        if (!channel || channel->label() != "partyboard-input-v1")
            throw std::invalid_argument("Dedicated authenticated input channel required");
        const auto binding = state_->bridge->attach(slot);
        { std::lock_guard lock(state_->mutex);
          if (state_->stopped) throw std::runtime_error("RTC input owner stopped");
          state_->channels[slot] = channel;
          state_->generations[slot] = binding.route.generation;
          state_->closed &= uint8_t(~(1u<<slot)); }
        std::weak_ptr<State> weak = state_;
        channel->onClosed([weak,binding] {
            if (const auto state = weak.lock()) {
                std::lock_guard lock(state->mutex);
                if (!state->stopped && state->generations[binding.slot] == binding.route.generation)
                    state->closed |= uint8_t(1u<<binding.slot);
            }
        });
        channel->onMessage([weak,binding,observe](rtc::message_variant message) {
            const auto state = weak.lock();
            if (!state) return;
            { std::lock_guard lock(state->mutex);
              if (state->stopped || state->generations[binding.slot] != binding.route.generation) return; }
            const auto bytes = std::get_if<rtc::binary>(&message);
            const auto result = bytes ? state->bridge->enqueue(binding,
                std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes->data()),bytes->size()))
                : InputMailbox::Push::Malformed;
            if (observe) observe(result);
            // Unauthorized/malformed packets never enter the mailbox. Overflow
            // is a loss of valid input and must stop the session.
            if (result == InputMailbox::Push::Overflow) {
                std::lock_guard lock(state->mutex);
                if (!state->stopped && state->generations[binding.slot] == binding.route.generation)
                    state->malformed = true;
            }
        });
    }
    bool pump(RecoverablePadExchange& exchange,uint64_t now,
            std::function<void(uint8_t)> disconnected = {}) {
        uint8_t closed;
        { std::lock_guard lock(state_->mutex);
          if (state_->stopped || state_->malformed) { failureReason_=state_->stopped?"transport-owner-stopped":"mailbox-overflow";exchange.cancel(); return false; }
          closed = state_->closed; state_->closed = 0; }
        for (uint8_t slot=0;slot<4;++slot) if (closed & (1u<<slot)) {
            if (!exchange.disconnected(slot+1,now)) {failureReason_="disconnection-rejected";return false;}
            if (disconnected) disconnected(slot);
        }
        if (exchange.tick(now) == ReconnectPolicy::State::Paused) return true;
        // Flush only after the coordinator has authenticated and resumed recovery.
        if (!state_->deferred.flush([&](uint8_t slot,const Packet& packet) {
            std::shared_ptr<rtc::DataChannel> channel;
            { std::lock_guard lock(state_->mutex); channel=state_->channels[slot]; }
            if (!channel || !channel->isOpen()) {
                std::lock_guard lock(state_->mutex);state_->closed|=uint8_t(1u<<slot);return false;
            }
            try { channel->send(reinterpret_cast<const rtc::byte*>(packet.data()),packet.size()); }
            catch (...) {
                if(channel->isOpen())throw;
                std::lock_guard lock(state_->mutex);state_->closed|=uint8_t(1u<<slot);return false;
            }
            return true;
        })) {
            { std::lock_guard lock(state_->mutex);closed=state_->closed;state_->closed=0; }
            for(uint8_t slot=0;slot<4;++slot)if(closed&(1u<<slot)) {
                if(!exchange.disconnected(slot+1,now)){failureReason_="disconnection-rejected";return false;}
                if(disconnected)disconnected(slot);
            }
            return true; // Packet retained, game poll paused before commit.
        }
        const bool pumped=state_->bridge->pump(exchange);
        if(!pumped)failureReason_=state_->bridge->failureReason();
        { std::lock_guard lock(state_->mutex);closed=state_->closed;state_->closed=0; }
        for(uint8_t slot=0;slot<4;++slot)if(closed&(1u<<slot)) {
            if(!exchange.disconnected(slot+1,now)){failureReason_="disconnection-rejected";return false;}
            if(disconnected)disconnected(slot);
        }
        return pumped;
    }
    bool send(const Packet& packet) { return state_->bridge->sendLocal(packet); }
    // Call only after successful stopped input detach. Late callbacks hold weak
    // references and cannot revive this owner or authorize reconnect recovery.
    void closeStopped() {
        std::array<std::shared_ptr<rtc::DataChannel>,4> channels;
        { std::lock_guard lock(state_->mutex); state_->stopped = true; channels.swap(state_->channels); }
        for (const auto& channel:channels) if (channel) channel->close();
    }
};
std::unique_ptr<NativeRtcHostInput> createNativeRtcHostInput(SessionId session,
    uint8_t hostSlot,uint8_t activeSlots);
}
