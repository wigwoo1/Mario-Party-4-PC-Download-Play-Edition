/* Mathematical DSP bus/writeback tests plus the owned voice-render path.
 * The external reference is Dolphin's documented free DSP mix_add/ramp
 * instruction sequence, NOT a claim of Nintendo ROM fidelity:
 * https://github.com/dolphin-emu/dolphin/blob/master/docs/DSP/free_dsp_rom/dsp_rom.ds
 * MULX fractional product, ADDP, ASR16, separate per-voice writeback and the
 * final MOVP/AX0.L return motivate the hand-calculated cases below. Original
 * ROM coefficients, microcode scratch aliasing and studio effects remain open.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/port/musyx_pcm_runtime.h"
#include "../src/port/musyx_depop_voice.h"

static void test_integer_bus(void) {
    _PB pb = {0};
    int64_t products[32 * 9] = {0};
    int32_t bus[32 * 9] = {0};
    for(unsigned f = 0; f < 32; ++f) {
        products[f * 9] = 16384; /* +0.5: each voice writes zero. */
        products[f * 9 + 1] = -1; /* -1/32768: arithmetic floor is -1. */
    }
    assert(PartyboardMusyxStudioMixVoice200(bus, products, 32, &pb));
    assert(PartyboardMusyxStudioMixVoice200(bus, products, 32, &pb));
    for(unsigned f = 0; f < 32; ++f) assert(bus[f * 9] == 0 && bus[f * 9 + 1] == -2);
    assert(pb.dpop.aL == 0 && pb.dpop.aR == 65535); /* Not the mixed -2. */
    products[31 * 9] = 32767LL * 32767;
    products[31 * 9 + 1] = -32768LL * 32767;
    assert(PartyboardMusyxStudioMixVoice200(bus, products, 32, &pb));
    assert(pb.dpop.aL == 32766 && pb.dpop.aR == (uint16_t)-32767);
    bus[0] = INT32_MAX; products[0] = 32768;
    bus[1] = INT32_MIN; products[1] = -1;
    assert(PartyboardMusyxStudioMixVoice200(bus, products, 32, &pb));
    assert(bus[0] == INT32_MIN && bus[1] == INT32_MAX);
    {
        _PB saved = pb;
        int32_t saved_bus[32 * 9];
        memcpy(saved_bus, bus, sizeof bus);
        products[31 * 9 + 8] = 1073741825LL;
        assert(!PartyboardMusyxStudioMixVoice200(bus, products, 32, &pb));
        assert(!memcmp(&pb, &saved, sizeof pb) && !memcmp(bus, saved_bus, sizeof bus));
        assert(!PartyboardMusyxStudioMixVoice200(bus, products, 0, &pb));
        pb.mixerCtrl = 16;
        assert(!PartyboardMusyxStudioMixVoice200(bus, products, 32, &pb));
    }
}
static void test_routing_depop(void) {
    for(unsigned ctrl = 0; ctrl < 16; ++ctrl) {
        _PB pb = {0}; DSPvoice voice = {0}; DSPhostDPop sum = {0};
        int64_t products[9]; int32_t bus[9] = {0};
        const unsigned enabled[9] = {1,1,ctrl&4,ctrl&1,ctrl&1,(ctrl&1)&&(ctrl&4),
            ctrl&2,ctrl&2,(ctrl&2)&&(ctrl&4)};
        pb.mixerCtrl = (uint16_t)ctrl;
        for(unsigned c = 0; c < 9; ++c) products[c] = (int64_t)(c + 1) * 32768;
        assert(PartyboardMusyxStudioMixVoice200(bus, products, 1, &pb));
        for(unsigned c = 0; c < 9; ++c) assert(bus[c] == (enabled[c] ? (int32_t)c + 1 : 0));
        voice.pb = &pb; voice.postBreak = 1; pb.state = 1;
        assert(PartyboardMusyxDepopVoice200(&sum, &voice));
        assert(!voice.postBreak && !pb.state);
        assert(sum.l == 1 && sum.r == 2 && sum.s == (enabled[2] ? 3 : 0));
        assert(sum.lA == (enabled[3] ? 4 : 0) && sum.rA == (enabled[4] ? 5 : 0));
        assert(sum.sA == (enabled[5] ? 6 : 0) && sum.lB == (enabled[6] ? 7 : 0));
        assert(sum.rB == (enabled[7] ? 8 : 0) && sum.sB == (enabled[8] ? 9 : 0));
    }
}
static void test_owned_voice(void) {
    const uint8_t pcm[2] = {0x40, 0};
    PartyboardMusyxSampleMemory memory = {0};
    PartyboardMusyxPcmRuntime runtime = {0};
    DSPvoice voice = {0}; _PB pb = {0};
    int32_t bus[160 * 9] = {0};
    int32_t saved_bus[160 * 9];
    DSPhostDPop sum = {0};
    voice.pb = &pb; voice.state = 1; voice.smp_id = 12; voice.mesgCallBackUserValue = 123;
    voice.smp_info.addr = (void *)pcm; voice.smp_info.length = 1;
    voice.smp_info.loopLength = 1; voice.smp_info.compType = 2;
    voice.smp_info.info = 32000;
    voice.srcTypeSelect = 2; /* Bypass: no synthetic or replacement ROM bank. */
    voice.playInfo.pitch = 65536;
    voice.adsr.data.linear.sLevel = 32767;
    voice.volL = 16384; voice.volR = 8192; voice.volS = 4096;
    assert(PartyboardMusyxSampleMemoryAdd(&memory, pcm, sizeof pcm));
    assert(PartyboardMusyxPcmRuntimeStart(&runtime, &voice, &memory, 65536, NULL, 0));
    /* Sustain an actual registered looping PCM voice using the original
     * ADSR scheduler. Avoid making fixture attack timing the mixer oracle. */
    runtime.envelope.adsr.state = 3;
    runtime.envelope.adsr.currentVolume = 32767 * 65536;
    {
        PartyboardMusyxPcmRuntime saved = runtime;
        voice.flags = 0x80000000U;
        assert(!PartyboardMusyxPcmRuntimeRenderStudio200(&runtime, &voice,
            SND_STUDIO_TYPE_STD, 2, 1, bus));
        assert(!memcmp(&saved, &runtime, sizeof runtime) && !voice.playInfo.posHi);
        voice.flags = 0; pb.itd.flag = 1;
        assert(!PartyboardMusyxPcmRuntimeRenderStudio200(&runtime, &voice,
            SND_STUDIO_TYPE_STD, 2, 1, bus));
        assert(!memcmp(&saved, &runtime, sizeof runtime) && !voice.playInfo.posHi);
        pb.itd.flag = 0;
    }
    assert(PartyboardMusyxPcmRuntimeRenderStudio200(&runtime, &voice,
        SND_STUDIO_TYPE_STD, 2, 1, bus) == 96);
    for(unsigned f = 0; f < 160; ++f) {
        assert(bus[f * 9] == (f < 64 ? 0 : 8191));
        assert(bus[f * 9 + 1] == (f < 64 ? 0 : 4095));
        assert(bus[f * 9 + 2] == (f < 64 ? 0 : 2047));
        for(unsigned c = 3; c < 9; ++c) assert(!bus[f * 9 + c]);
    }
    assert(pb.dpop.aL == 8191 && pb.dpop.aR == 4095 && pb.dpop.aS == 2047);
    memset(bus, 0, sizeof bus); voice.state = 2; memset(voice.changed, 0, sizeof voice.changed);
    assert(PartyboardMusyxPcmRuntimeRenderStudio200(&runtime, &voice,
        SND_STUDIO_TYPE_STD, 0, 0, bus) == 160);
    for(unsigned f = 0; f < 160; ++f) assert(bus[f * 9] == 8191);
    assert(PartyboardMusyxDepopVoice200(&sum, &voice));
    assert(sum.l == 8191 && sum.r == 4095 && sum.s == 2047);
    /* Removing this exact sample registration invalidates the retained
     * decoder. No stale PCM or depop writeback may escape afterward. */
    memcpy(saved_bus, bus, sizeof bus);
    assert(PartyboardMusyxSampleMemoryRemove(&memory, pcm));
    assert(!PartyboardMusyxPcmRuntimeRenderStudio200(&runtime, &voice,
        SND_STUDIO_TYPE_STD, 0, 0, bus));
    assert(!memcmp(bus, saved_bus, sizeof bus) && !runtime.active);
}
int main(void) {
    test_integer_bus(); test_routing_depop(); test_owned_voice();
    puts("PASS studio voice mixing: per-voice quantization, negative floors, bus wrap, routing, residual ownership, planned startup/continuation and retired samples");
    return 0;
}
