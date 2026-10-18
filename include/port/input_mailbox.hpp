#pragma once
#include "lockstep.hpp"
#include <algorithm>
#include <deque>
#include <map>
#include <mutex>

namespace partyboard::net {
// Network callbacks only copy bounded packets here. Drain on the simulation
// thread; only the caller may commit an input frame or advance game state.
class InputMailbox {
public:
    struct Route { uint32_t peer; uint64_t generation; };
    enum class Push { Queued, Retired, Malformed, Overflow };
    static constexpr size_t Capacity = 4 * Lockstep::Window;
    // Call only after the coordinator authenticates the original/replacement
    // connection. A packet can never choose its own route identity.
    Route bind(uint32_t peer) {
        std::lock_guard lock(mutex_);
        if (!peer || (!routes_.contains(peer) && routes_.size() >= 4))
            throw std::invalid_argument("Invalid input route");
        if (generation_ == UINT64_MAX) throw std::overflow_error("Input route generation exhausted");
        const Route route{peer, ++generation_};
        routes_[peer] = route.generation;
        discardRetired();
        return route;
    }
    void retire(Route route) {
        std::lock_guard lock(mutex_);
        if (current(route)) { routes_.erase(route.peer); discardRetired(); }
    }
    Push push(Route route, std::span<const uint8_t> bytes) {
        std::lock_guard lock(mutex_);
        if (!current(route)) return Push::Retired;
        if (!decode(bytes)) return Push::Malformed;
        if (packets_.size() == Capacity) { overflow_ = true; return Push::Overflow; }
        Entry entry{route, {}};
        std::copy(bytes.begin(), bytes.end(), entry.packet.begin());
        packets_.push_back(entry);
        return Push::Queued;
    }
    template<class Receiver> bool drain(Receiver& receiver) {
        std::lock_guard lock(mutex_);
        if (overflow_) return false; // Coordinator must halt; never silently lose input.
        while (!packets_.empty()) {
            auto entry = packets_.front(); packets_.pop_front();
            if (!current(entry.route)) continue;
            const auto result = receiver.receive(entry.route.peer, entry.packet);
            if (result != Receive::Accepted && result != Receive::Duplicate && result != Receive::Stale) {
                overflow_ = true;
                return false;
            }
        }
        return true;
    }
private:
    struct Entry { Route route; Packet packet; };
    void discardRetired() {
        // Replacement reconciliation retransmits missing inputs. Unconsumed
        // packets from old callbacks must not occupy the replacement's capacity.
        std::erase_if(packets_, [this](const Entry& entry) { return !current(entry.route); });
    }
    bool current(Route route) const {
        const auto found = routes_.find(route.peer);
        return found != routes_.end() && found->second == route.generation;
    }
    std::mutex mutex_;
    std::map<uint32_t, uint64_t> routes_;
    std::deque<Entry> packets_;
    uint64_t generation_{};
    bool overflow_{};
};
}
