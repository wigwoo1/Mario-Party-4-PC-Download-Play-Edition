#pragma once
#include "lockstep.hpp"
#include <deque>
#include <algorithm>
namespace partyboard::net {
// Game-thread queue only. Transport acceptance is separate from peer readiness.
class DeferredInputPackets {
public:
    static constexpr size_t Capacity = 256;
    bool retain(uint8_t slot, std::span<const uint8_t> bytes) {
        if (slot >= 4 || bytes.size() != Packet{}.size()) return false;
        Packet packet{}; std::copy(bytes.begin(), bytes.end(), packet.begin());
        for (const auto& item : pending_) if (item.slot == slot && item.packet == packet) return true;
        if (pending_.size() == Capacity) return false;
        pending_.push_back({slot, packet}); return true;
    }
    template<class Sender> bool flush(Sender send) {
        while (!pending_.empty()) {
            const auto& item = pending_.front();
            if (!send(item.slot, item.packet)) return false;
            pending_.pop_front();
        }
        return true;
    }
    size_t size() const { return pending_.size(); }
private:
    struct Item { uint8_t slot; Packet packet; };
    std::deque<Item> pending_;
};
}
