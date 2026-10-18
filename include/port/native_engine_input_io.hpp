#pragma once
#include "input_trace.h"
#include <filesystem>
#include "main.h"
#include "lockstep.hpp"
#include "native_input_timing.hpp"
#include <chrono>
#include <functional>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>

namespace partyboard::net {
// Game-thread adapter for NativeInputPollBinding. The session transport owns
// authenticated queues, event pumping and bounded waits; capture always reads
// the running engine. No remote packet supplies this local checkpoint or RNG.
class NativeEngineInputIO {
    std::function<bool()> pump_;
    std::function<bool(const Packet&)> send_;
    std::function<void()> wait_;
    std::function<void(const PartyBoardInputState&, const PADStatus*)> accepted_;
    std::function<void(const PartyBoardInputState&, PADStatus*)> diagnosticLocalInput_;
    NativeInputTimer timer_;
public:
    NativeEngineInputIO(std::function<bool()> pump,
                        std::function<bool(const Packet&)> send,
                        std::function<void()> wait,
                        std::function<void(const PartyBoardInputState&, const PADStatus*)> accepted = {},
                        NativeInputTimer timer = NativeInputTimer{})
        : pump_(std::move(pump)), send_(std::move(send)), wait_(std::move(wait)), accepted_(std::move(accepted)),timer_(std::move(timer)) {
        if (!pump_ || !send_ || !wait_) throw std::invalid_argument("Owned transport callbacks required");
    }
    static uint64_t monotonicNow() {
        return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    uint64_t now() const {return monotonicNow();}
    // Test driver only. Configure while the engine is stopped; the exchange
    // still selects owned local lanes and computes checkpoint/RNG itself.
    void setDiagnosticLocalInput(std::function<void(const PartyBoardInputState&, PADStatus*)> input) {
        if(PartyBoard_GameLifecyclePhase()!=2||PartyBoard_GameFrameCount()!=0)
            throw std::runtime_error("Diagnostic local input requires stopped pre-frame engine");
        diagnosticLocalInput_=std::move(input);
    }
    void prepareLocalPads(const PartyBoardInputState& state, PADStatus* pads) {
        timer_.beginPoll();
        if(diagnosticLocalInput_)diagnosticLocalInput_(state,pads);
    }
    PartyBoardInputState capture() const {
        PartyBoardInputState state{};
        if (!PartyBoardCaptureInputState(&state)) throw std::runtime_error("Native engine input state unavailable");
        return state;
    }
    bool pump() { auto elapsed=timer_.pump();return pump_(); }
    void stopped(const char* reason) const noexcept {
        std::fprintf(stderr,"Native input poll stopped: %s\n",reason);
        std::fflush(stderr);
    }
    bool send(const Packet& packet) { auto elapsed=timer_.send();return send_(packet); }
    void wait() { auto elapsed=timer_.wait();wait_(); }
    const NativeInputTiming& timing()const noexcept{return timer_.snapshot();}
    void accepted(const PartyBoardInputState& state, const PADStatus* pads) {
        timer_.accepted();
        if (accepted_) accepted_(state, pads);
    }
};
}
