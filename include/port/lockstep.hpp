#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>

namespace partyboard::net {
using SessionId = std::array<uint8_t, 16>;
// Same explicit 11-byte raw controller layout as input traces; never native padding.
using Pad = std::array<uint8_t, 11>;
using Packet = std::array<uint8_t, 48>;
struct Input {
    SessionId session{};
    uint64_t frame{};
    uint32_t checkpoint{}; // State BEFORE consuming this frame's inputs.
    uint8_t slot{};
    Pad pad{};
    bool operator==(const Input&) const = default;
};
inline uint64_t readLE(const uint8_t* p, unsigned size) {
    uint64_t value = 0;
    for (unsigned i = 0; i < size; ++i) value |= uint64_t(p[i]) << (8 * i);
    return value;
}
inline void writeLE(uint8_t* p, uint64_t value, unsigned size) {
    for (unsigned i = 0; i < size; ++i) p[i] = uint8_t(value >> (8 * i));
}
inline Packet encode(const Input& input) {
    if (input.slot >= 4) throw std::invalid_argument("Invalid controller slot");
    Packet p{'P', 'B', 'I', 'P', 1, input.slot};
    for (unsigned i = 0; i < 16; ++i) p[8 + i] = input.session[i];
    writeLE(p.data() + 24, input.frame, 8);
    writeLE(p.data() + 32, input.checkpoint, 4);
    for (unsigned i = 0; i < 11; ++i) p[36 + i] = input.pad[i];
    return p;
}
inline std::optional<Input> decode(std::span<const uint8_t> p) {
    if (p.size() != 48 || p[0] != 'P' || p[1] != 'B' || p[2] != 'I' || p[3] != 'P' ||
        p[4] != 1 || p[5] >= 4 || p[6] || p[7] || p[47]) return {};
    Input input{};
    input.slot = p[5];
    for (unsigned i = 0; i < 16; ++i) input.session[i] = p[8 + i];
    input.frame = readLE(p.data() + 24, 8);
    input.checkpoint = uint32_t(readLE(p.data() + 32, 4));
    for (unsigned i = 0; i < 11; ++i) input.pad[i] = p[36 + i];
    return input;
}

enum class Receive { Accepted, Duplicate, Stale, TooFar, WrongSession, Unauthorized,
                     Malformed, Conflict, Divergence, Halted };
enum class Stop { None, Conflict, Divergence, Cancelled, FrameLimit };
struct Frame {
    uint64_t index{};
    uint32_t checkpoint{};
    std::array<Pad, 4> pads{};
};

// Transport-independent, bounded lockstep. Call on ONE simulation/network thread.
// The caller binds peer IDs to authenticated connections; packets cannot claim an ID.
class Lockstep {
public:
    static constexpr uint64_t Window = 120;
    static constexpr uint64_t FrameLimit = 10000000;
    Lockstep(SessionId session, std::array<uint32_t, 4> owners)
        : session_(session), owners_(owners) {
        bool nonzero = false;
        for (auto b : session) nonzero |= b != 0;
        for (unsigned i = 0; i < 4; ++i) if (owners[i]) active_ |= uint8_t(1u << i);
        if (!nonzero || !active_) throw std::invalid_argument("Session ID and active slots required");
    }
    Receive receive(uint32_t peer, std::span<const uint8_t> packet) {
        if (stop_ != Stop::None) return Receive::Halted;
        const auto decoded = decode(packet);
        if (!decoded) return Receive::Malformed;
        const auto& input = *decoded;
        if (input.session != session_) return Receive::WrongSession;
        if (!peer || owners_[input.slot] != peer) return Receive::Unauthorized;
        if (input.frame < next_) return Receive::Stale;
        if (input.frame >= FrameLimit || input.frame - next_ >= Window) return Receive::TooFar;
        auto& pending = pending_[input.frame % Window];
        const uint8_t bit = uint8_t(1u << input.slot);
        if (pending.mask & bit) {
            if (pending.inputs[input.slot] == input) return Receive::Duplicate;
            stop_ = Stop::Conflict;
            return Receive::Conflict;
        }
        for (unsigned i = 0; i < 4; ++i) {
            if ((pending.mask & (1u << i)) && pending.inputs[i].checkpoint != input.checkpoint) {
                stop_ = Stop::Divergence;
                return Receive::Divergence;
            }
        }
        pending.inputs[input.slot] = input;
        pending.mask |= bit;
        return Receive::Accepted;
    }
    uint8_t missingSlots() const {
        return uint8_t(active_ & ~pending_[next_ % Window].mask);
    }
    // No prediction: nullopt means do NOT advance the game simulation.
    // The local checkpoint is mandatory even for peers without a local controller.
    std::optional<Frame> advance(uint32_t localCheckpoint) {
        if (stop_ != Stop::None || missingSlots()) return {};
        auto& pending = pending_[next_ % Window];
        Frame frame{};
        frame.index = next_;
        frame.checkpoint = localCheckpoint;
        for (unsigned i = 0; i < 4; ++i) {
            if (active_ & (1u << i)) {
                if (pending.inputs[i].checkpoint != localCheckpoint) {
                    stop_ = Stop::Divergence;
                    return {};
                }
                frame.pads[i] = pending.inputs[i].pad;
            } else {
                frame.pads[i][10] = 255; // PAD_ERR_NO_CONTROLLER, explicitly disconnected.
            }
        }
        pending = {};
        if (++next_ == FrameLimit) stop_ = Stop::FrameLimit;
        return frame;
    }
    void cancel() { if (stop_ == Stop::None) stop_ = Stop::Cancelled; }
    Stop stopped() const { return stop_; }
    uint64_t nextFrame() const { return next_; }
private:
    struct Pending { uint8_t mask{}; std::array<Input, 4> inputs{}; };
    SessionId session_;
    std::array<uint32_t, 4> owners_;
    uint8_t active_{};
    uint64_t next_{};
    Stop stop_{Stop::None};
    std::array<Pending, Window> pending_{};
};
}
