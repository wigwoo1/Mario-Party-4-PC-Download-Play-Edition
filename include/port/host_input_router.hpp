#pragma once
#include "lockstep.hpp"
namespace partyboard::net {
// Same star topology and slot+1 owner IDs as web/lobby-input-router.mjs.
// Channels are authenticated and bound to slots by the lobby coordinator.
class HostInputRouter {
public:
    struct Route { uint32_t owner; uint8_t recipients; };
    HostInputRouter(SessionId session, uint8_t hostSlot, uint8_t activeSlots)
        : session_(session), host_(hostSlot), active_(activeSlots) {
        bool nonzero = false; for (auto value : session) nonzero |= value != 0;
        if (!nonzero || hostSlot >= 4 || (activeSlots & 0xf0) || !(activeSlots & (1u << hostSlot)))
            throw std::invalid_argument("Invalid host input membership");
    }
    std::optional<Route> local(std::span<const uint8_t> bytes) const {
        return validate(host_, bytes);
    }
    std::optional<Route> remote(uint8_t authenticatedSlot, std::span<const uint8_t> bytes) const {
        if (authenticatedSlot == host_) return {};
        return validate(authenticatedSlot, bytes);
    }
private:
    std::optional<Route> validate(uint8_t slot, std::span<const uint8_t> bytes) const {
        auto input = decode(bytes);
        if (slot >= 4 || !(active_ & (1u << slot)) || !input || input->slot != slot || input->session != session_)
            return {};
        // Receive into lockstep first; relay only accepted/duplicate/stale
        // packets. Frame-window/checkpoint rejection must not be broadcast.
        return Route{uint32_t(slot + 1), uint8_t(active_ & ~(1u << host_) & ~(1u << slot))};
    }
    SessionId session_;
    uint8_t host_, active_;
};
}
