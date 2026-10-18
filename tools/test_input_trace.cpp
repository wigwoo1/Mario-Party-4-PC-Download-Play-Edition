#include "../include/port/input_trace.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
#ifdef __EMSCRIPTEN__
static int freopen_s(FILE** output, const char* path, const char* mode, FILE* stream) {
    *output = std::freopen(path, mode, stream);
    return *output ? 0 : 1;
}
#endif

extern "C" {
s32 rnd_seed;
static u32 fakeState;
void PartyBoardRandomSeed(u32 seed) { fakeState = seed; rnd_seed = (s32)seed; }
u32 PartyBoardRandomState(void) { return fakeState; }
static u32 fakeCheckpoint;
u32 PartyBoardStateCheckpoint(void) { return fakeCheckpoint; }
void PartyBoardStateSnapshot(unsigned long long sample) {
    std::fprintf(stderr, "Trace fixture snapshot: sample=%llu checkpoint=%u\n", sample, fakeCheckpoint);
}
}
int main(int argc, char** argv) {
    assert(argc == 2);
    const std::filesystem::path dir(argv[1]);
    std::filesystem::create_directories(dir);
    const auto diagnosticPath = (dir / "diagnostics.log").string();
    FILE* diagnostic = nullptr;
    assert(freopen_s(&diagnostic, diagnosticPath.c_str(), "w", stderr) == 0);
    const auto path = (dir / "record.bin").string();
    PADStatus pads[4]{};
    for (int i = 0; i < 4; ++i) {
        pads[i].button = 0x100u << i;
        pads[i].stickX = -128 + i; pads[i].stickY = 127 - i;
        pads[i].substickX = -64; pads[i].substickY = 63;
        pads[i].triggerLeft = 255; pads[i].triggerRight = 129;
        pads[i].analogA = 123; pads[i].analogB = 234;
        pads[i].err = -i;
    }
    u32 rumble = 0xF0000000;
    assert(PartyBoardTraceOpen(path.c_str(), 0, 42));
    assert(PartyBoardTraceSample(pads, &rumble));
    fakeState = 123; rnd_seed = 456;
    assert(PartyBoardTraceSample(pads, &rumble));
    assert(PartyBoardTraceClose() == 0);
    assert(std::filesystem::file_size(path) == 16 + 2 * 68);
    assert(!PartyBoardTraceOpen(path.c_str(), 0, 99)); // Never overwrite.
    assert(std::filesystem::file_size(path) == 152);
    assert(PartyBoardTraceOpen(path.c_str(), 1, 99)); // Seed comes from file.
    assert(fakeState == 42 && rnd_seed == 42);
    PADStatus restored[4]{};
    rumble = 0;
    assert(PartyBoardTraceSample(restored, &rumble));
    assert(rumble == 0xF0000000);
    for (int i = 0; i < 4; ++i) {
        assert(restored[i].button == pads[i].button);
        assert(restored[i].stickX == pads[i].stickX && restored[i].stickY == pads[i].stickY);
        assert(restored[i].substickX == -64 && restored[i].substickY == 63);
        assert(restored[i].triggerLeft == 255 && restored[i].triggerRight == 129);
        assert(restored[i].analogA == 123 && restored[i].analogB == 234);
        assert(restored[i].err == -i);
    }
    fakeState = 123; rnd_seed = 456;
    assert(PartyBoardTraceSample(restored, &rumble));
    assert(!PartyBoardTraceSample(restored, &rumble)); // Clean EOF.
    assert(PartyBoardTraceClose() == 0);
    assert(PartyBoardTraceOpen(path.c_str(), 1, 0));
    fakeState = 43;
    assert(!PartyBoardTraceSample(restored, &rumble));
    assert(PartyBoardTraceClose() == 2);
    assert(PartyBoardTraceOpen(path.c_str(), 1, 0));
    rnd_seed = 44;
    const auto preservedButton = restored[0].button;
    assert(!PartyBoardTraceSample(restored, &rumble));
    assert(restored[0].button == preservedButton); // Reject before applying input.
    assert(PartyBoardTraceClose() == 2);
    assert(PartyBoardTraceOpen(path.c_str(), 1, 0));
    fakeCheckpoint = 1;
    assert(!PartyBoardTraceSample(restored, &rumble));
    assert(PartyBoardTraceClose() == 2);
    fakeCheckpoint = 0;
    std::ifstream input(path, std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(input)), {});
    for (size_t length : {size_t(0), size_t(15), size_t(17), size_t(79)}) {
        const auto bad = (dir / ("truncated-" + std::to_string(length))).string();
        std::ofstream output(bad, std::ios::binary);
        output.write(bytes.data(), length); output.close();
        const auto opened = PartyBoardTraceOpen(bad.c_str(), 1, 0);
        if (length < 16) assert(!opened);
        else { assert(opened); assert(!PartyBoardTraceSample(restored, &rumble)); }
        assert(PartyBoardTraceClose() == 2);
    }
    const auto bounded = (dir / "bounded.bin").string();
    assert(PartyBoardTraceOpen(bounded.c_str(), 0, 42));
    PartyBoardTraceSetRecordLimit(1);
    assert(PartyBoardTraceSample(pads, &rumble));
    assert(!PartyBoardTraceSample(pads, &rumble));
    assert(PartyBoardTraceClose() == 0);
    assert(std::filesystem::file_size(bounded) == 16 + 68);
    const auto stimulusPath = (dir / "stimulus-v2.bin").string();
    bytes[7] = 2;
    { std::ofstream out(stimulusPath, std::ios::binary); out.write(bytes.data(), bytes.size()); }
    const auto freshPath = (dir / "fresh.bin").string();
    assert(PartyBoardTraceOpen(freshPath.c_str(), 0, 99));
    assert(PartyBoardTraceScriptOpen(stimulusPath.c_str()));
    assert(fakeState == 99 && rnd_seed == 99); // Old seed/checkpoints are not imported.
    PADStatus scripted[4]{};
    rumble = 1234;
    assert(PartyBoardTraceSample(scripted, &rumble));
    assert(scripted[0].button == pads[0].button && scripted[0].stickX == -128);
    assert(rumble == 1234); // Rumble is an output of the new simulation.
    assert(PartyBoardTraceSample(scripted, &rumble));
    assert(!PartyBoardTraceSample(scripted, &rumble));
    assert(PartyBoardTraceClose() == 0);
    assert(PartyBoardTraceOpen(freshPath.c_str(), 1, 0));
    assert(!PartyBoardTraceScriptOpen(stimulusPath.c_str())); // Cannot bypass replay validation.
    assert(PartyBoardTraceSample(restored, &rumble));
    assert(PartyBoardTraceSample(restored, &rumble));
    assert(!PartyBoardTraceSample(restored, &rumble));
    assert(PartyBoardTraceClose() == 0);
    for (int failure = 0; failure < 3; ++failure) {
        auto invalid = bytes;
        if (failure == 0) invalid[16] = 1; // Sequence must start at zero.
        if (failure == 1) invalid.resize(16); // Empty script.
        if (failure == 2) invalid.resize(17); // Partial sample.
        const auto invalidPath = (dir / ("invalid-script-" + std::to_string(failure))).string();
        { std::ofstream out(invalidPath, std::ios::binary); out.write(invalid.data(), invalid.size()); }
        const auto outPath = invalidPath + ".new";
        assert(PartyBoardTraceOpen(outPath.c_str(), 0, 42));
        assert(PartyBoardTraceScriptOpen(invalidPath.c_str()));
        assert(!PartyBoardTraceSample(scripted, &rumble));
        assert(PartyBoardTraceClose() == 2);
    }
    const auto mergedPath = (dir / "merged.bin").string();
    fakeCheckpoint = 0;
    assert(PartyBoardTraceOpen(mergedPath.c_str(), 0, 42));
    assert(PartyBoardTraceScriptOpen(path.c_str()));
    assert(PartyBoardTraceSampleWithExchange(pads, &rumble, [](PADStatus value[4]) {
        assert(value[1].button == 0x200); // Script applied before exchange.
        value[1].button = 0x888;
        value[1].stickX = -17;
        return 1;
    }));
    assert(PartyBoardTraceClose() == 0);
    assert(PartyBoardTraceOpen(mergedPath.c_str(), 1, 42));
    assert(PartyBoardTraceSample(restored, &rumble));
    assert(restored[1].button == 0x888 && restored[1].stickX == -17);
    assert(!PartyBoardTraceSample(restored, &rumble));
    assert(PartyBoardTraceClose() == 0);
    const auto abortedPath = (dir / "aborted-exchange.bin").string();
    for (int field = 0; field < 3; ++field) {
        const auto mutatedPath = (dir / ("mutated-exchange-" + std::to_string(field))).string();
        fakeCheckpoint = 0;
        assert(PartyBoardTraceOpen(mutatedPath.c_str(), 0, 42));
        PartyBoardInputExchange mutate[] = {
            [](PADStatus[4]) { ++fakeState; return 1; },
            [](PADStatus[4]) { ++rnd_seed; return 1; },
            [](PADStatus[4]) { ++fakeCheckpoint; return 1; }
        };
        assert(!PartyBoardTraceSampleWithExchange(pads, &rumble, mutate[field]));
        assert(PartyBoardTraceClose() == 2);
        assert(std::filesystem::file_size(mutatedPath) == 16);
    }
    fakeCheckpoint = 0;
    assert(PartyBoardTraceOpen(abortedPath.c_str(), 0, 42));
    assert(!PartyBoardTraceSampleWithExchange(pads, &rumble, [](PADStatus[4]) { return 0; }));
    assert(PartyBoardTraceClose() == 0);
    assert(std::filesystem::file_size(abortedPath) == 16); // Failed input not recorded.
    assert(PartyBoardTraceSampleWithExchange(pads, &rumble, [](PADStatus value[4]) {
        value[0].button = 1234;
        return 1;
    })); // Exchange still runs when tracing is disabled.
    assert(pads[0].button == 1234);
    fflush(stderr);
    // freopen_s opens with exclusive sharing on Windows; release before reading.
    const auto assertionPath = (dir / "assertions.log").string();
    assert(freopen_s(&diagnostic, assertionPath.c_str(), "w", stderr) == 0);
    std::ifstream diagnostics(diagnosticPath);
    assert(diagnostics.is_open());
    const std::string messages((std::istreambuf_iterator<char>(diagnostics)), {});
    assert(messages.find("sample=0 field=frand expected=42 actual=43") != std::string::npos);
    assert(messages.find("sample=0 field=rand8 expected=42 actual=44") != std::string::npos);
    assert(messages.find("sample=0 field=selected_state expected=0 actual=1") != std::string::npos);
    puts("Input trace integration tests passed");
}
