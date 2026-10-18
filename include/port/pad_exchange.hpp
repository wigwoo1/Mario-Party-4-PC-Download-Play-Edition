#pragma once
#include <dolphin/pad.h>
#include "lockstep.hpp"

namespace partyboard::net {
inline Pad packPad(const PADStatus& p) {
    return {uint8_t(p.button), uint8_t(p.button >> 8), uint8_t(p.stickX), uint8_t(p.stickY),
            uint8_t(p.substickX), uint8_t(p.substickY), p.triggerLeft, p.triggerRight,
            p.analogA, p.analogB, uint8_t(p.err)};
}
inline PADStatus unpackPad(const Pad& p) {
    PADStatus result{};
    result.button = u16(p[0] | (u16(p[1]) << 8));
    result.stickX = s8(p[2]); result.stickY = s8(p[3]);
    result.substickX = s8(p[4]); result.substickY = s8(p[5]);
    result.triggerLeft = p[6]; result.triggerRight = p[7];
    result.analogA = p[8]; result.analogB = p[9]; result.err = s8(p[10]);
    result.extButton = 0; // Native UI shortcuts never travel in game input.
    return result;
}
enum class Exchange { Waiting, Ready, Stopped };
class PadExchange {
public:
    PadExchange(SessionId session, std::array<uint32_t, 4> owners, uint32_t local)
        : PadExchange(session,owners,local,defaultMapping(owners,local)) {}
    PadExchange(SessionId session, std::array<uint32_t,4> owners, uint32_t local,
                std::array<int8_t,4> localPadSlots)
        : queue_(session, owners), session_(session), owners_(owners), localPadSlots_(localPadSlots), local_(local) {
        if (!local) throw std::invalid_argument("Local connection ID required");
        uint8_t used=0;
        for(unsigned slot=0;slot<4;++slot) {
            const auto lane=localPadSlots_[slot];
            if(owners_[slot]!=local_) {
                if(lane!=-1)throw std::invalid_argument("Remote slot cannot map a local controller");
            } else {
                if(lane<0||lane>=4||(used&(1u<<lane)))throw std::invalid_argument("Unique local controller lanes required");
                used|=uint8_t(1u<<lane);
            }
        }
    }
    // Freeze once before any game input processing; retries must use poll().
    bool begin(const PADStatus pads[4], uint32_t checkpoint) {
        if (pending_ || queue_.stopped() != Stop::None) return false;
        pending_ = true; checkpoint_ = checkpoint; outgoingCount_ = 0;
        for (uint8_t slot = 0; slot < 4; ++slot) if (owners_[slot] == local_) {
            auto packet = encode(Input{session_, queue_.nextFrame(), checkpoint, slot, packPad(pads[localPadSlots_[slot]])});
            if (queue_.receive(local_, packet) != Receive::Accepted) { recordStop("begin-local-packet"); cancel(); return false; }
            outgoing_[outgoingCount_++] = packet;
        }
        return true;
    }
    std::span<const Packet> outgoing() const { return {outgoing_.data(), outgoingCount_}; }
    Receive receive(uint32_t authenticatedPeer, std::span<const uint8_t> packet) {
        // Remote transport traffic must never impersonate this local connection.
        if (authenticatedPeer == local_) return Receive::Unauthorized;
        const auto result = queue_.receive(authenticatedPeer, packet);
        if (result == Receive::Conflict || result == Receive::Divergence || result == Receive::Halted)
            recordStop("receive-remote-packet");
        return result;
    }
    Exchange poll(uint32_t checkpoint, PADStatus destination[4]) {
        if (!pending_ || queue_.stopped() != Stop::None) return Exchange::Stopped;
        if (checkpoint != checkpoint_) { recordStop("poll-checkpoint-drift"); cancel(); return Exchange::Stopped; }
        const auto frame = queue_.advance(checkpoint);
        if (!frame) {
            if (queue_.stopped() != Stop::None) recordStop("poll-advance");
            return queue_.stopped() == Stop::None ? Exchange::Waiting : Exchange::Stopped;
        }
        // No caller-visible mutation occurs while waiting or on a failed checkpoint.
        for (unsigned slot = 0; slot < 4; ++slot) destination[slot] = unpackPad(frame->pads[slot]);
        pending_ = false; outgoingCount_ = 0;
        return Exchange::Ready;
    }
    void cancel() { recordStop("cancel"); queue_.cancel(); pending_ = false; outgoingCount_ = 0; }
    const char* firstStopSite() const { return firstStopSite_; }
    Stop firstStopState() const { return firstStopState_; }
    uint64_t nextFrame() const { return queue_.nextFrame(); }
    std::array<int8_t,4> localPadSlots() const { return localPadSlots_; }
private:
    void recordStop(const char* site) {
        if (!firstStopSite_) { firstStopSite_ = site; firstStopState_ = queue_.stopped(); }
    }
    const char* firstStopSite_{};
    Stop firstStopState_ = Stop::None;
    static std::array<int8_t,4> defaultMapping(const std::array<uint32_t,4>& owners,uint32_t local) {
        std::array<int8_t,4> result{};
        for(unsigned slot=0;slot<4;++slot)result[slot]=owners[slot]==local?int8_t(slot):int8_t(-1);
        return result;
    }
    Lockstep queue_;
    SessionId session_;
    std::array<uint32_t, 4> owners_;
    std::array<int8_t,4> localPadSlots_;
    uint32_t local_, checkpoint_{};
    bool pending_{};
    std::array<Packet, 4> outgoing_{};
    size_t outgoingCount_{};
};
}
