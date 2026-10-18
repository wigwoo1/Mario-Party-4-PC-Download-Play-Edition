#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
#include "lockstep.hpp"

namespace partyboard::net {
using Digest = std::array<uint8_t, 32>;
struct Compatibility {
    uint32_t protocol{1};
    Digest build{}, assets{}, save{}, settings{};
    uint32_t seed{};
    bool operator==(const Compatibility&) const = default;
};
// All-zero fingerprints mean verification is unfinished, never a compatible game.
inline bool hasFingerprints(const Compatibility& config) {
    if (config.protocol != 1) return false;
    for (const auto* digest : {&config.build, &config.assets, &config.save, &config.settings}) {
        bool nonzero = false;
        for (const auto byte : *digest) nonzero |= byte != 0;
        if (!nonzero) return false;
    }
    return true;
}
using HelloPacket = std::array<uint8_t, 144>;
inline HelloPacket encodeHello(const Compatibility& config) {
    if (!hasFingerprints(config)) throw std::invalid_argument("Incomplete compatibility verification");
    HelloPacket bytes{'P', 'B', 'H', 'I', 1};
    writeLE(bytes.data() + 8, config.protocol, 4);
    writeLE(bytes.data() + 12, config.seed, 4);
    unsigned offset = 16;
    for (const auto* digest : {&config.build, &config.assets, &config.save, &config.settings})
        for (auto byte : *digest) bytes[offset++] = byte;
    return bytes;
}
inline std::optional<Compatibility> decodeHello(std::span<const uint8_t> bytes) {
    if (bytes.size() != 144 || bytes[0] != 'P' || bytes[1] != 'B' || bytes[2] != 'H' ||
        bytes[3] != 'I' || bytes[4] != 1 || bytes[5] || bytes[6] || bytes[7]) return {};
    Compatibility config;
    config.protocol = uint32_t(readLE(bytes.data() + 8, 4));
    config.seed = uint32_t(readLE(bytes.data() + 12, 4));
    unsigned offset = 16;
    for (auto* digest : {&config.build, &config.assets, &config.save, &config.settings})
        for (auto& byte : *digest) byte = bytes[offset++];
    if (!hasFingerprints(config)) return {};
    return config;
}
enum class Mismatch { None, Protocol, Build, Assets, Save, Settings, Seed };
inline Mismatch compare(const Compatibility& expected, const Compatibility& offered) {
    if (expected.protocol != offered.protocol) return Mismatch::Protocol;
    if (expected.build != offered.build) return Mismatch::Build;
    if (expected.assets != offered.assets) return Mismatch::Assets;
    if (expected.save != offered.save) return Mismatch::Save;
    if (expected.settings != offered.settings) return Mismatch::Settings;
    if (expected.seed != offered.seed) return Mismatch::Seed;
    return Mismatch::None;
}
// Host-side readiness state only, not a network service or authentication layer.
// Each assignment/settings change invalidates every readiness acknowledgement.
class ReadyGate {
public:
    ReadyGate(uint32_t host, Compatibility config) : host_(host), config_(config) {
        if (!host || !hasFingerprints(config)) throw std::invalid_argument("Invalid host or incomplete verification");
    }
    bool assign(uint32_t caller, uint8_t slot, uint32_t owner) {
        if (caller != host_ || slot >= 4 || started_) return false;
        if (owners_[slot] != owner) { owners_[slot] = owner; invalidate(); }
        return true;
    }
    bool configure(uint32_t caller, const Compatibility& config) {
        if (caller != host_ || started_ || !hasFingerprints(config)) return false;
        if (config_ != config) { config_ = config; invalidate(); }
        return true;
    }
    bool ready(uint32_t peer, uint64_t revision, const Compatibility& offered, bool value) {
        if (!peer || started_ || revision != revision_) return false;
        uint8_t mask = 0;
        for (unsigned i = 0; i < 4; ++i) if (owners_[i] == peer) mask |= uint8_t(1u << i);
        if (!mask) return false;
        // A failed re-check removes an earlier ready acknowledgement as well.
        ready_ &= uint8_t(~mask);
        if (compare(config_, offered) != Mismatch::None) return false;
        if (value) ready_ |= mask;
        return true;
    }
    bool canStart() const {
        uint8_t assigned = 0;
        for (unsigned i = 0; i < 4; ++i) if (owners_[i]) assigned |= uint8_t(1u << i);
        return !started_ && assigned && ready_ == assigned;
    }
    bool start(uint32_t caller, uint64_t revision) {
        if (caller != host_ || revision != revision_ || !canStart()) return false;
        started_ = true;
        return true;
    }
    // Caller invokes this after transport confirms disconnection. During a game,
    // return false so the coordinator stops the game instead of replacing a peer.
    bool disconnected(uint32_t peer) {
        if (started_ || peer == host_) return false;
        bool removed = false;
        for (auto& owner : owners_) if (owner == peer && owner) { owner = 0; removed = true; }
        if (removed) invalidate();
        return true;
    }
    uint64_t revision() const { return revision_; }
    const std::array<uint32_t, 4>& owners() const { return owners_; }
private:
    void invalidate() { ready_ = 0; ++revision_; }
    uint32_t host_;
    Compatibility config_;
    std::array<uint32_t, 4> owners_{};
    uint64_t revision_{1};
    uint8_t ready_{};
    bool started_{};
};
}
