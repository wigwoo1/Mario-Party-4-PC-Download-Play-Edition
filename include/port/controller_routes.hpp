#pragma once
#include "pad_exchange.hpp"

namespace partyboard::net {
struct CapabilityOffer {
    SessionId session{};
    uint64_t revision{};
    uint32_t mask{};
    bool operator==(const CapabilityOffer&) const = default;
};
using CapabilityPacket = std::array<uint8_t, 36>;
inline bool validCapabilityOffer(const CapabilityOffer& offer) {
    bool session = false;
    for (auto byte : offer.session) session |= byte != 0;
    return session && offer.revision && !(offer.mask & 0x0fffffffu);
}
inline CapabilityPacket encodeCapabilities(const CapabilityOffer& offer) {
    if (!validCapabilityOffer(offer)) throw std::invalid_argument("Invalid capability offer");
    CapabilityPacket packet{'P', 'B', 'C', 'P', 1};
    for (unsigned i = 0; i < 16; ++i) packet[8 + i] = offer.session[i];
    writeLE(packet.data() + 24, offer.revision, 8);
    writeLE(packet.data() + 32, offer.mask, 4);
    return packet;
}
inline std::optional<CapabilityOffer> decodeCapabilities(std::span<const uint8_t> packet) {
    if (packet.size() != 36 || packet[0] != 'P' || packet[1] != 'B' ||
        packet[2] != 'C' || packet[3] != 'P' || packet[4] != 1 ||
        packet[5] || packet[6] || packet[7]) return {};
    CapabilityOffer result;
    for (unsigned i = 0; i < 16; ++i) result.session[i] = packet[8 + i];
    result.revision = readLE(packet.data() + 24, 8);
    result.mask = uint32_t(readLE(packet.data() + 32, 4));
    return validCapabilityOffer(result) ? std::optional(result) : std::nullopt;
}
// One immutable ownership revision. Recreate after lobby ownership or device changes.
// A distinct magic prevents reflecting an offer as an acknowledgement.
inline CapabilityPacket encodeCapabilityAck(const CapabilityOffer& agreement) {
    auto packet = encodeCapabilities(agreement);
    packet[3] = 'A';
    return packet;
}
inline std::optional<CapabilityOffer> decodeCapabilityAck(std::span<const uint8_t> packet) {
    if (packet.size() != 36 || packet[3] != 'A') return {};
    CapabilityPacket offer{};
    for (unsigned i = 0; i < offer.size(); ++i) offer[i] = packet[i];
    offer[3] = 'P';
    return decodeCapabilities(offer);
}
// The transport supplies authenticated peer identity and binds this revision to session.
class ControllerCapabilities {
public:
    ControllerCapabilities(std::array<uint32_t, 4> owners, uint64_t revision)
        : owners_(owners), revision_(revision) {
        bool active = false;
        for (auto owner : owners) active |= owner != 0;
        if (!active || !revision) throw std::invalid_argument("Active ownership revision required");
    }
    bool offer(uint32_t peer, uint64_t revision, uint32_t mask) {
        if (cancelled_ || !peer || revision != revision_) return false;
        uint32_t allowed = 0;
        for (unsigned i = 0; i < 4; ++i)
            if (owners_[i] == peer) allowed |= 0x80000000u >> i;
        if (!allowed || (mask & ~allowed)) return false;
        for (unsigned i = 0; i < 4; ++i) if (owners_[i] == peer && received_[i])
            if (bool(mask & (0x80000000u >> i)) != bool(mask_ & (0x80000000u >> i))) return false;
        for (unsigned i = 0; i < 4; ++i) if (owners_[i] == peer) received_[i] = true;
        mask_ |= mask;
        return true;
    }
    bool receive(uint32_t authenticatedPeer, const SessionId& expectedSession,
                 std::span<const uint8_t> packet) {
        const auto decoded = decodeCapabilities(packet);
        return decoded && decoded->session == expectedSession &&
               offer(authenticatedPeer, decoded->revision, decoded->mask);
    }
    std::optional<uint32_t> agreedMask() const {
        if (cancelled_) return {};
        for (unsigned i = 0; i < 4; ++i) if (owners_[i] && !received_[i]) return {};
        return mask_;
    }
    // Separate from offers: the peer acknowledges the complete combined mask.
    bool acknowledge(uint32_t peer, uint64_t revision, uint32_t combinedMask) {
        const auto combined = agreedMask();
        if (!peer || revision != revision_ || !combined || *combined != combinedMask) return false;
        bool active = false;
        for (unsigned i = 0; i < 4; ++i) if (owners_[i] == peer) {
            acknowledged_[i] = true;
            active = true;
        }
        return active;
    }
    bool ready() const {
        if (!agreedMask()) return false;
        for (unsigned i = 0; i < 4; ++i) if (owners_[i] && !acknowledged_[i]) return false;
        return true;
    }
    bool receiveAck(uint32_t authenticatedPeer, const SessionId& expectedSession,
                    std::span<const uint8_t> packet) {
        const auto decoded = decodeCapabilityAck(packet);
        return decoded && decoded->session == expectedSession &&
               acknowledge(authenticatedPeer, decoded->revision, decoded->mask);
    }
    // On disconnect or configuration change, invalidate before making a new revision.
    void cancel() { cancelled_ = true; }
    uint64_t revision() const { return revision_; }
    const std::array<uint32_t, 4>& owners() const { return owners_; }
private:
    std::array<uint32_t, 4> owners_;
    std::array<bool, 4> received_{};
    std::array<bool, 4> acknowledged_{};
    bool cancelled_{};
    uint64_t revision_;
    uint32_t mask_{};
};
// Maps each game slot to a local SDL/PAD slot, or -1 for remote/inactive slots.
// Recreate only at a negotiated lobby/recovery boundary, never mid-frame.
class ControllerRoutes {
public:
    ControllerRoutes(std::array<uint32_t, 4> owners, uint32_t local,
                     std::array<int, 4> devices) : devices_(devices) {
        if (!local) throw std::invalid_argument("Local peer ID required");
        std::array<bool, 4> used{};
        for (unsigned slot = 0; slot < 4; ++slot) {
            const int device = devices[slot];
            if (owners[slot] != local) {
                if (device != -1) throw std::invalid_argument("Cannot route a remote or inactive slot");
            } else {
                if (device < 0 || device >= 4 || used[device])
                    throw std::invalid_argument("Local slots need distinct valid controllers");
                used[device] = true;
            }
        }
    }
    std::array<PADStatus, 4> route(const PADStatus physical[4]) const {
        std::array<PADStatus, 4> result{};
        for (unsigned slot = 0; slot < 4; ++slot) {
            result[slot].err = PAD_ERR_NO_CONTROLLER;
            if (devices_[slot] >= 0)
                result[slot] = unpackPad(packPad(physical[devices_[slot]]));
        }
        return result;
    }
    // Local contribution only. The session must negotiate and combine all owners'
    // masks; a machine's physical mask must never stand in for the session mask.
    uint32_t rumbleContribution(uint32_t physicalMask) const {
        uint32_t result = 0;
        for (unsigned slot = 0; slot < 4; ++slot)
            if (devices_[slot] >= 0 && (physicalMask & (0x80000000u >> devices_[slot])))
                result |= 0x80000000u >> slot;
        return result;
    }
    // Route game motor commands only to a controller owned on this machine.
    // -1 means ignore the command; never pass it through as a device index.
    int motorDevice(unsigned gameSlot) const {
        return gameSlot < devices_.size() ? devices_[gameSlot] : -1;
    }
private:
    std::array<int, 4> devices_;
};
}
