#pragma once
#include "pad_exchange.hpp"
#include <chrono>
#include <thread>

namespace partyboard::net {
enum class Pump { Continue, Cancel, Disconnected };
enum class WaitResult { Ready, Cancelled, Disconnected, Timeout, StateMismatch,
                        TransportError, ClockError, InvalidTimeout };
struct WaitIO {
    virtual ~WaitIO() = default;
    // These callbacks must be bounded/nonblocking and must NOT simulate or render.
    // send() must copy packet bytes before returning if delivery is asynchronous.
    virtual bool send(std::span<const Packet> packets) = 0;
    virtual bool sendPending() const { return false; }
    virtual Pump pump(PadExchange& exchange) = 0; // Receive packets and service close/cancel.
    virtual uint32_t checkpoint() = 0;
    virtual uint64_t milliseconds() {
        return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    virtual void idle() { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
};
inline WaitResult waitForInput(PadExchange& exchange, PADStatus pads[4], WaitIO& io,
                               uint32_t timeoutMs) {
    const auto stop = [&](WaitResult reason) { exchange.cancel(); return reason; };
    if (!timeoutMs || timeoutMs > 60000) return stop(WaitResult::InvalidTimeout);
    try {
        const auto start = io.milliseconds();
        auto last = start;
        const auto frozenCheckpoint = io.checkpoint();
        if (!exchange.begin(pads, frozenCheckpoint)) return stop(WaitResult::StateMismatch);
        if (!io.send(exchange.outgoing())) return stop(WaitResult::TransportError);
        for (;;) {
            const auto action = io.pump(exchange);
            if (action == Pump::Cancel) return stop(WaitResult::Cancelled);
            if (action == Pump::Disconnected) return stop(WaitResult::Disconnected);
            const auto now = io.milliseconds();
            if (now < last) return stop(WaitResult::ClockError);
            last = now;
            if (now - start >= timeoutMs) return stop(WaitResult::Timeout);
            // A queued send must not conceal simulation drift during the wait.
            const auto currentCheckpoint = io.checkpoint();
            if (currentCheckpoint != frozenCheckpoint) return stop(WaitResult::StateMismatch);
            if (io.sendPending()) { io.idle(); continue; }
            switch (exchange.poll(currentCheckpoint, pads)) {
            case Exchange::Ready: return WaitResult::Ready;
            case Exchange::Stopped: return stop(WaitResult::StateMismatch);
            case Exchange::Waiting: break;
            }
            io.idle();
        }
    } catch (...) {
        return stop(WaitResult::TransportError);
    }
}
}
