#include "port/input_trace.h"
#include "port/trace_codec.hpp"
#include <cstdio>
#include <exception>
#include <cstdlib>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

extern "C" s32 rnd_seed;
extern "C" void PartyBoardStateSnapshot(unsigned long long sample);
namespace {
FILE* file = nullptr;
FILE* script = nullptr;
bool replayMode = false;
bool failed = false;
uint64_t sampleIndex = 0;
u32 recordLimit = 0;
constexpr uint64_t MaxSamples = 10000000;
void report(const char* reason) {
    fprintf(stderr, "Input trace: sample %llu: %s\n", (unsigned long long)sampleIndex, reason);
    failed = true;
}
void compare(const char* field, uint64_t expected, uint64_t actual) {
    if (expected == actual) return;
    fprintf(stderr, "Input trace divergence: sample=%llu field=%s expected=%llu actual=%llu\n",
            (unsigned long long)sampleIndex, field,
            (unsigned long long)expected, (unsigned long long)actual);
    throw std::runtime_error("Input replay checkpoint divergence");
}
}

extern "C" int PartyBoardTraceOpen(const char* path, int replay, u32 seed) {
    if (file) return 0;
    failed = false;
    replayMode = replay != 0;
    sampleIndex = 0;
    recordLimit = 0;
    // Exclusive creation prevents accidentally overwriting a recording.
    file = fopen(path, replayMode ? "rb" : "wbx");
    if (!file) { report("Cannot open trace (recording requires a new file)"); return 0; }
    try {
        auto h = partyboard::trace::header(seed);
        if (replayMode) {
            if (fread(h.data(), 1, h.size(), file) != h.size())
                throw std::runtime_error("Truncated input trace header");
            seed = partyboard::trace::seed(h);
        } else if (fwrite(h.data(), 1, h.size(), file) != h.size()) {
            throw std::runtime_error("Cannot write input trace header");
        }
        PartyBoardRandomSeed(seed);
        fprintf(stderr, "Input trace: %s, seed %u\n", replayMode ? "replaying" : "recording", seed);
        return 1;
    } catch (const std::exception& e) {
        report(e.what());
        fclose(file); file = nullptr;
        return 0;
    }
}

extern "C" int PartyBoardTraceScriptOpen(const char* path) {
    if (!file || replayMode || script || failed) return 0;
    script = fopen(path, "rb");
    if (!script) { report("Cannot open input-only script"); return 0; }
    partyboard::trace::Header h{};
    const auto bytes = fread(h.data(), 1, h.size(), script);
    // Version 2 and 3 share the raw controller layout. No checkpoint is imported.
    const auto version = h[7];
    h[7] = 3;
    try {
        if (bytes != h.size() || (version != 2 && version != 3))
            throw std::runtime_error("Input script requires a version-2 or version-3 trace");
        partyboard::trace::seed(h); // Validate reserved bytes; deliberately ignore old seed.
        fprintf(stderr, "Input script: controller stimulus only; recording NEW checkpoints\n");
        return 1;
    } catch (const std::exception& e) {
        report(e.what()); fclose(script); script = nullptr; return 0;
    }
}

extern "C" int PartyBoardTraceSample(PADStatus pads[4], u32* rumble) {
    return PartyBoardTraceSampleWithExchange(pads, rumble, nullptr);
}

extern "C" int PartyBoardTraceSampleWithExchange(PADStatus pads[4], u32* rumble,
                                                PartyBoardInputExchange exchange) {
    if (!file) return exchange ? exchange(pads) : 1;
    if (failed) return 0;
    try {
        if (!replayMode && recordLimit && sampleIndex == recordLimit) {
            fprintf(stderr, "Input trace: recording complete (%llu samples)\n", (unsigned long long)sampleIndex);
            return 0;
        }
        if (sampleIndex < 4) {
#ifdef __EMSCRIPTEN__
            const bool diagnostics = EM_ASM_INT({return globalThis.testTraceState ? 1 : 0;});
#else
            const char* setting = std::getenv("PARTYBOARD_TRACE_STATE_DIAGNOSTICS");
            const bool diagnostics = setting && setting[0] == '1' && setting[1] == '\0';
#endif
            if (diagnostics) PartyBoardStateSnapshot(sampleIndex);
        }
        partyboard::trace::Sample s{};
        partyboard::trace::Sample stimulus{};
        if (script) {
            const auto bytes = fread(stimulus.data(), 1, stimulus.size(), script);
            if (!bytes && feof(script) && sampleIndex) {
                fprintf(stderr, "Input script: complete (%llu samples); new recording finished\n",
                        (unsigned long long)sampleIndex);
                return 0;
            }
            if (bytes != stimulus.size()) throw std::runtime_error("Empty or truncated input script");
            const auto sequence = uint64_t(partyboard::trace::get32(stimulus.data())) |
                (uint64_t(partyboard::trace::get32(stimulus.data() + 4)) << 32);
            if (sequence != sampleIndex) throw std::runtime_error("Input script sample sequence mismatch");
        }
        if (replayMode) {
            const auto bytes = fread(s.data(), 1, s.size(), file);
            if (bytes == 0 && feof(file)) {
                fprintf(stderr, "Input trace: replay complete (%llu samples); RNG and selected-state checkpoints matched\n",
                        (unsigned long long)sampleIndex);
                fclose(file); file = nullptr;
                return 0;
            }
            if (bytes != s.size()) throw std::runtime_error("Truncated input trace sample");
            const auto sequence = uint64_t(partyboard::trace::get32(s.data())) |
                (uint64_t(partyboard::trace::get32(s.data() + 4)) << 32);
            compare("sequence", sampleIndex, sequence);
            compare("frand", partyboard::trace::get32(s.data() + 8), PartyBoardRandomState());
            compare("rand8", partyboard::trace::get32(s.data() + 12), (u32)rnd_seed);
            compare("selected_state", partyboard::trace::get32(s.data() + 64), PartyBoardStateCheckpoint());
            *rumble = partyboard::trace::get32(s.data() + 16);
        } else {
            partyboard::trace::stamp(s, sampleIndex, PartyBoardRandomState(), (u32)rnd_seed, *rumble);
            partyboard::trace::put32(s.data() + 64, PartyBoardStateCheckpoint());
        }
        if (sampleIndex >= MaxSamples) throw std::runtime_error("Input trace sample limit reached");
        for (int i = 0; i < 4; ++i) {
            auto* p = s.data() + 20 + i * 11;
            auto& pad = pads[i];
            if (replayMode || script) {
                if (script) p = stimulus.data() + 20 + i * 11;
                pad.button = u16(p[0] | (u16(p[1]) << 8));
                pad.stickX = s8(p[2]); pad.stickY = s8(p[3]);
                pad.substickX = s8(p[4]); pad.substickY = s8(p[5]);
                pad.triggerLeft = p[6]; pad.triggerRight = p[7];
                pad.analogA = p[8]; pad.analogB = p[9]; pad.err = s8(p[10]);
                pad.extButton = 0; // Native UI controls are deliberately outside game replay.
            }
        }
        if (exchange && !exchange(pads)) return 0;
        // Waiting may service networking/UI, but must never advance simulation.
        // Check before recording accepted input or allowing the game to consume it.
        if (exchange) {
            compare("exchange_frand", partyboard::trace::get32(s.data() + 8), PartyBoardRandomState());
            compare("exchange_rand8", partyboard::trace::get32(s.data() + 12), (u32)rnd_seed);
            compare("exchange_selected_state", partyboard::trace::get32(s.data() + 64), PartyBoardStateCheckpoint());
        }
        if (!replayMode) {
            for (int i = 0; i < 4; ++i) {
                auto* p = s.data() + 20 + i * 11;
                const auto& pad = pads[i];
                p[0] = pad.button & 255; p[1] = pad.button >> 8;
                p[2] = pad.stickX; p[3] = pad.stickY;
                p[4] = pad.substickX; p[5] = pad.substickY;
                p[6] = pad.triggerLeft; p[7] = pad.triggerRight;
                p[8] = pad.analogA; p[9] = pad.analogB; p[10] = pad.err;
            }
        }
        if (!replayMode && fwrite(s.data(), 1, s.size(), file) != s.size())
            throw std::runtime_error("Cannot write input trace sample");
        ++sampleIndex;
        return 1;
    } catch (const std::exception& e) { report(e.what()); return 0; }
}

extern "C" int PartyBoardTraceClose(void) {
    if (script) { fclose(script); script = nullptr; }
    if (file) {
        if (fclose(file)) report("Cannot finish input trace file");
        file = nullptr;
        fprintf(stderr, "Input trace: closed after %llu samples\n", (unsigned long long)sampleIndex);
    }
    return failed ? 2 : 0;
}

extern "C" void PartyBoardTraceSetRecordLimit(u32 samples) {
    recordLimit = samples;
}
