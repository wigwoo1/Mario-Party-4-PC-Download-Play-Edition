#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

namespace partyboard::net {
// Observations only: never changes transport, packet acceptance or recovery.
class NativeTransportDiagnostics {
    mutable std::mutex mutex_;
    std::chrono::steady_clock::time_point started_=std::chrono::steady_clock::now();
    std::deque<nlohmann::ordered_json> events_;
    uint64_t sequence_=0;
public:
    void record(std::string type,std::string detail={}) {
        type.resize(std::min(type.size(),size_t(32)));
        detail.resize(std::min(detail.size(),size_t(192)));
        std::lock_guard lock(mutex_);
        const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now()-started_).count();
        if(events_.size()==32)events_.pop_front();
        events_.push_back({{"sequence",++sequence_},{"elapsedMs",elapsed},{"type",type},{"detail",detail}});
    }
    nlohmann::ordered_json snapshot()const {
        std::lock_guard lock(mutex_);
        return {{"events",events_},{"totalEvents",sequence_},{"droppedEvents",sequence_-events_.size()}};
    }
};
}
