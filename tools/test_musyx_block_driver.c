#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/port/musyx_block_driver.h"

static DSPvoice voices[2];
static _PB pbs[2];
static PartyboardMusyxSampleMemory memory;
static PartyboardMusyxVoiceScheduler scheduler;
static PartyboardMusyxBlockDriver driver;
static int32_t scratch[PARTYBOARD_MUSYX_BLOCK_SAMPLES];
static uint8_t pcm[2] = {0x40,0};
typedef struct { unsigned calls; int response; uint32_t hash; int test_reentry; } Sink;
typedef struct { Sink sink; unsigned aux_calls; int32_t buffers[3][480]; } AuxProbe;
static AuxProbe aux_probe;
static void initialize(unsigned tick_rate) {
    memset(voices, 0, sizeof voices); memset(pbs, 0, sizeof pbs);
    memset(dspStudio, 0, sizeof dspStudio); memset(&memory, 0, sizeof memory);
    memset(&scheduler, 0, sizeof scheduler); memset(&driver, 0, sizeof driver);
    dspVoice = voices; salNumVoices = 2; salMaxStudioNum = 1; salFrame = salAuxFrame = 0;
    salHooks.malloc = malloc; salHooks.free = free;
    memset(&synthInfo, 0, sizeof synthInfo);
    synthInfo.voiceNum = synthInfo.maxMusic = synthInfo.maxSFX = 2;
    synthInfo.studioNum = 1; synthInfo.numSamples = 32;
    synthInit(32000, 2); seqInit(); dataInitStack(); streamInit(); vsInit(); s3dInit(0); sndActive = 1;
    dspStudio[0].state = 1; dspStudio[0].type = SND_STUDIO_TYPE_STD;
    assert(PartyboardMusyxSampleMemoryAdd(&memory, pcm, sizeof pcm));
    assert(PartyboardMusyxVoiceSchedulerInit(&scheduler, 2, 1, &memory, NULL, 0));
    assert(PartyboardMusyxBlockDriverInit(&driver, &scheduler, tick_rate));
    assert(!PartyboardMusyxBlockDriverInit(&driver, &scheduler, tick_rate));
    voices[0].pb = &pbs[0]; voices[0].smp_id = 1; voices[0].mesgCallBackUserValue = 7;
    voices[0].smp_info.addr = pcm; voices[0].smp_info.info = 32000;
    voices[0].smp_info.length = voices[0].smp_info.loopLength = 1; voices[0].smp_info.compType = 2;
    voices[0].srcTypeSelect = 2; voices[0].playInfo.pitch = 65536;
    voices[0].adsr.data.linear.sLevel = 32767; voices[0].volL = 16384;
    voices[0].virtualSampleID = UINT32_MAX;
    salActivateVoice(&voices[0], 0);
}
static void finish(void) {
    salDeactivateVoice(&voices[0]);
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, scratch));
    s3dExit(); synthExit(); synthVoice = NULL; sndActive = 0;
}
static int sink(void *context, int32_t *buses, unsigned studios) {
    Sink *state = context;
    assert(studios == 1 && buses == scratch);
    assert(driver.busy && !driver.failed);
    /* SDK time and buffers are observed before this block's control phase. */
    assert(synthRealTime == (uint64_t)state->calls * 5 * 256);
    assert(salFrame == (state->calls & 1) && salAuxFrame == state->calls % 3);
    for(unsigned frame = 0; frame < 160; ++frame) {
        assert(buses[frame * 9] == 8191);
        for(unsigned channel = 1; channel < 9; ++channel) assert(!buses[frame * 9 + channel]);
        for(unsigned channel = 0; channel < 9; ++channel)
            state->hash = (state->hash ^ (uint32_t)buses[frame * 9 + channel]) * 16777619U;
    }
    if(state->test_reentry) {
        uint64_t before = driver.mixed_blocks;
        assert(!PartyboardMusyxBlockDriverTick(&driver, scratch, sink, context));
        assert(driver.mixed_blocks == before && !driver.failed);
    }
    if(state == &aux_probe.sink) {
        assert(aux_probe.aux_calls == state->calls);
        voices[1].changed[0] = 0x123; /* Aux processing must precede clearing. */
    }
    ++state->calls;
    return state->response;
}
static void aux_callback(u8 reason, SND_AUX_INFO *info, void *context) {
    AuxProbe *probe = context;
    unsigned slot = (probe->aux_calls + 2) % 3;
    assert(reason == 0 && probe == &aux_probe);
    assert(probe->sink.calls == probe->aux_calls + 1);
    assert(synthRealTime == (uint64_t)probe->aux_calls * 1280);
    assert(voices[1].changed[0] == 0x123);
    assert(info->data.bufferUpdate.left == probe->buffers[slot]);
    assert(info->data.bufferUpdate.right == probe->buffers[slot] + 160);
    assert(info->data.bufferUpdate.surround == probe->buffers[slot] + 320);
    info->data.bufferUpdate.left[0] = (int32_t)++probe->aux_calls;
}
static uint32_t run_sink_mode(int response) {
    Sink state = {0};
    state.response = response; state.hash = 2166136261U; state.test_reentry = 1;
    initialize(60);
    for(unsigned tick = 0; tick < 120; ++tick) {
        assert(PartyboardMusyxBlockDriverTick(&driver, scratch, sink, &state));
        assert(driver.completed_blocks == (uint64_t)(tick + 1) * 200 / 60);
        assert(driver.control_periods == driver.completed_blocks * 5);
        assert(synthRealTime == driver.control_periods * 256);
        assert(hwGetTimeOffset() == 0);
    }
    assert(state.calls == 400 && driver.mixed_blocks == 400 && driver.ticks == 120);
    assert(!driver.remainder && !driver.failed && !driver.busy);
    assert(driver.accepted_blocks == (response > 0 ? 400 : 0));
    assert(driver.dropped_blocks == (response <= 0 ? 400 : 0));
    assert(driver.device_errors == (response < 0 ? 400 : 0));
    assert(voices[0].state == 2 && scheduler.voice[0].owned);
    assert(scheduler.voice[0].runtime.sample.source.pcm.position == 1);
    finish(); return state.hash;
}
static void test_tick_rates_and_no_device(void) {
    initialize(1000);
    for(unsigned tick = 0; tick < 1000; ++tick)
        assert(PartyboardMusyxBlockDriverTick(&driver, scratch, NULL, NULL));
    assert(driver.completed_blocks == 200 && driver.control_periods == 1000);
    assert(synthRealTime == 256000 && driver.dropped_blocks == 200 && !driver.remainder);
    finish();
    initialize(1);
    assert(PartyboardMusyxBlockDriverTick(&driver, scratch, NULL, NULL));
    assert(driver.completed_blocks == 200 && synthRealTime == 256000);
    finish();
}
static void test_control_clear_and_fail_latch(void) {
    initialize(60);
    for(unsigned i = 0; i < 5; ++i) voices[1].changed[i] = 0xffffffff;
    assert(PartyboardMusyxBlockDriverTick(&driver, scratch, NULL, NULL));
    for(unsigned i = 0; i < 5; ++i) assert(!voices[1].changed[i]);
    {
        uint64_t blocks = driver.mixed_blocks, time = synthRealTime;
        voices[0].flags |= 0x80000000U;
        assert(!PartyboardMusyxBlockDriverTick(&driver, scratch, NULL, NULL));
        assert(driver.failed && !driver.busy && driver.mixed_blocks == blocks && synthRealTime == time);
        voices[0].flags = 0;
        assert(!PartyboardMusyxBlockDriverTick(&driver, scratch, NULL, NULL));
        assert(driver.mixed_blocks == blocks && synthRealTime == time);
    }
    finish();
}
static void test_original_aux_phase(void) {
    initialize(60); memset(&aux_probe, 0, sizeof aux_probe);
    aux_probe.sink.response = 1;
    for(unsigned slot = 0; slot < 3; ++slot) dspStudio[0].auxA[slot] = aux_probe.buffers[slot];
    dspStudio[0].auxAHandler = aux_callback; dspStudio[0].auxAUser = &aux_probe;
    for(unsigned tick = 0; tick < 60; ++tick) {
        assert(PartyboardMusyxBlockDriverTick(&driver, scratch, sink, &aux_probe.sink));
        assert(aux_probe.aux_calls == driver.completed_blocks && !voices[1].changed[0]);
    }
    assert(aux_probe.aux_calls == 200 && synthRealTime == 256000);
    finish();
}
int main(void) {
    uint32_t accepted = run_sink_mode(1), dropped = run_sink_mode(0), error = run_sink_mode(-1);
    assert(accepted == dropped && dropped == error);
    test_tick_rates_and_no_device(); test_control_clear_and_fail_latch(); test_original_aux_phase();
    printf("PASS audio block driver: exact simulation clock, original control phases, identical accepted/dropped/error PCM hash=%u, no-device progress and failure/reentry guards\n", accepted);
    return 0;
}
