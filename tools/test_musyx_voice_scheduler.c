#include <assert.h>
#include <stdio.h>
#include "musyx/hardware.h"
#include "../src/port/musyx_voice_scheduler.h"
void hwSetTimeOffset(u8 offset);

static DSPvoice voices[4];
static _PB pbs[4];
static PartyboardMusyxSampleMemory memory;
static PartyboardMusyxVoiceScheduler scheduler;
static int32_t buses[2 * PARTYBOARD_MUSYX_BLOCK_SAMPLES];
static unsigned callbacks, callback_ids[8];
static u32 on_message(u32 message, u32 user) {
    int found = 0;
    assert(message == 0 && callbacks < 8);
    for(unsigned i = 0; i < 4; ++i) if(voices[i].mesgCallBackUserValue == user) {
        DSPvoice *cursor = dspStudio[voices[i].studio].voiceRoot;
        assert(voices[i].state);
        while(cursor && cursor != &voices[i]) cursor = cursor->next;
        assert(cursor == &voices[i]); /* Callback precedes original unlink. */
        found = 1;
    }
    assert(found); callback_ids[callbacks++] = user;
    return 0;
}
static void reset(void) {
    memset(voices, 0, sizeof voices); memset(pbs, 0, sizeof pbs);
    memset(dspStudio, 0, sizeof dspStudio); memset(&scheduler, 0, sizeof scheduler);
    memset(&memory, 0, sizeof memory); memset(buses, 0, sizeof buses);
    memset(callback_ids, 0, sizeof callback_ids); callbacks = 0;
    dspVoice = voices; salMessageCallback = on_message;
    for(unsigned s = 0; s < 2; ++s) { dspStudio[s].state = 1; dspStudio[s].type = SND_STUDIO_TYPE_STD; }
    assert(PartyboardMusyxVoiceSchedulerInit(&scheduler, 4, 2, &memory, NULL, 0));
    assert(!PartyboardMusyxVoiceSchedulerInit(&scheduler, 4, 2, &memory, NULL, 0));
}
static void start(unsigned slot, uint8_t *pcm, unsigned samples, unsigned loop,
        unsigned studio, unsigned user, unsigned offset) {
    DSPvoice *voice = &voices[slot];
    if(voice->state) salDeactivateVoice(voice);
    memset(voice, 0, sizeof(*voice)); memset(&pbs[slot], 0, sizeof pbs[slot]);
    voice->pb = &pbs[slot]; voice->mesgCallBackUserValue = user; voice->smp_id = (uint16_t)user;
    voice->smp_info.addr = pcm; voice->smp_info.info = 32000; voice->smp_info.length = samples;
    voice->smp_info.loopLength = loop; voice->smp_info.compType = 2;
    voice->srcTypeSelect = 2; voice->singleOffset = (uint8_t)offset;
    voice->playInfo.pitch = 65536; voice->adsr.data.linear.sLevel = 32767;
    voice->volL = 16384; voice->virtualSampleID = UINT32_MAX;
    voice->prio = user;
    salActivateVoice(voice, (uint8_t)studio);
}
static void clear_controls(void) {
    for(unsigned i = 0; i < 4; ++i) memset(voices[i].changed, 0, sizeof voices[i].changed);
}
static void test_polyphony_and_stop(void) {
    uint8_t positive[2] = {0x40,0}, negative[2] = {0xe0,0};
    reset();
    assert(PartyboardMusyxSampleMemoryAdd(&memory, positive, sizeof positive));
    assert(PartyboardMusyxSampleMemoryAdd(&memory, negative, sizeof negative));
    start(0, positive, 1, 1, 0, 11, 2); start(1, negative, 1, 1, 0, 12, 0);
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    for(unsigned f = 0; f < 160; ++f) assert(buses[f * 9] == (f < 64 ? -4096 : 4095));
    assert(scheduler.rendered_voices == 2 && !callbacks && voices[0].state == 2);
    clear_controls();
    for(unsigned block = 0; block < 5; ++block) {
        assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
        for(unsigned f = 0; f < 160; ++f) assert(buses[f * 9] == 4095);
    }
    salDeactivateVoice(&voices[0]);
    assert(PartyboardMusyxSampleMemoryRemove(&memory, positive));
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    assert(scheduler.retired_voices == 1 && dspStudio[0].hostDPopSum.l == 8191);
    for(unsigned f = 0; f < 160; ++f) assert(buses[f * 9] == -4096);
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    assert(!scheduler.retired_voices && dspStudio[0].hostDPopSum.l == 8191);
    salDeactivateVoice(&voices[1]);
    assert(PartyboardMusyxSampleMemoryRemove(&memory, negative));
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    assert(scheduler.retired_voices == 1 && dspStudio[0].hostDPopSum.l == 4095);
    for(unsigned f = 0; f < sizeof buses / sizeof *buses; ++f) assert(!buses[f]);
    assert(!callbacks);
}
static void test_reuse(void) {
    uint8_t pcm[2] = {0x40,0};
    uint64_t generation;
    reset(); assert(PartyboardMusyxSampleMemoryAdd(&memory, pcm, sizeof pcm));
    generation = PartyboardMusyxSampleMemoryGeneration(&memory, pcm);
    start(0, pcm, 1, 1, 0, 21, 0);
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    salDeactivateVoice(&voices[0]);
    assert(PartyboardMusyxSampleMemoryRemove(&memory, pcm));
    pcm[0] = 0xe0; /* Same address, new owner/payload, reused DSP/PB slot. */
    assert(PartyboardMusyxSampleMemoryAdd(&memory, pcm, sizeof pcm));
    assert(PartyboardMusyxSampleMemoryGeneration(&memory, pcm) != generation);
    start(0, pcm, 1, 1, 0, 22, 0);
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    assert(scheduler.retired_voices == 1 && dspStudio[0].hostDPopSum.l == 8191);
    for(unsigned f = 0; f < 160; ++f) assert(buses[f * 9] == -4096);
    assert(scheduler.voice[0].runtime.callback_user_value == 22);
    assert(scheduler.voice[0].runtime.sample_generation != generation);
    clear_controls();
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    assert(!scheduler.retired_voices && dspStudio[0].hostDPopSum.l == 8191 && !callbacks);
}
static void test_natural_completion(void) {
    uint8_t pcm[320];
    reset();
    for(unsigned i = 0; i < 160; ++i) { pcm[i * 2] = 0x40; pcm[i * 2 + 1] = 0; }
    assert(PartyboardMusyxSampleMemoryAdd(&memory, pcm, sizeof pcm));
    start(0, pcm, 160, 0, 0, 31, 0); start(1, pcm, 160, 0, 0, 39, 0);
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    assert(!callbacks && scheduler.rendered_voices == 2);
    assert(voices[0].playInfo.posHi == 160 && voices[1].playInfo.posHi == 160);
    clear_controls();
    /* Unused invalid source controls and retired payload cannot prevent the
     * original finite block-entry completion callback. */
    voices[0].changed[0] = 0x100; voices[0].srcTypeSelect = 99;
    assert(PartyboardMusyxSampleMemoryRemove(&memory, pcm));
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    assert(callbacks == 2 && callback_ids[0] == 39 && callback_ids[1] == 31);
    assert(scheduler.completed_voices == 2 && !dspStudio[0].voiceRoot);
    assert(!voices[0].state && !voices[1].state && !dspStudio[0].hostDPopSum.l);
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    assert(callbacks == 2 && !scheduler.retired_voices && !dspStudio[0].hostDPopSum.l);
    for(unsigned f = 0; f < sizeof buses / sizeof *buses; ++f) assert(!buses[f]);
}
static void test_move_and_retired_sample(void) {
    uint8_t pcm[14];
    PartyboardMusyxPcmRuntime saved;
    reset();
    for(unsigned i = 0; i < 7; ++i) { pcm[i * 2] = (uint8_t)((i + 1) * 0x10); pcm[i * 2 + 1] = 0; }
    assert(PartyboardMusyxSampleMemoryAdd(&memory, pcm, sizeof pcm));
    start(0, pcm, 7, 7, 0, 41, 0);
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    assert(pbs[0].dpop.aL == 12287);
    clear_controls(); salReconnectVoice(&voices[0], 1);
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    assert(dspStudio[0].hostDPopSum.l == 12287 && !dspStudio[1].hostDPopSum.l);
    assert(scheduler.retired_voices == 1 && buses[PARTYBOARD_MUSYX_BLOCK_SAMPLES] == 14335);
    for(unsigned f = 0; f < PARTYBOARD_MUSYX_BLOCK_SAMPLES; ++f) assert(!buses[f]);
    assert(!dspStudio[0].alienVoiceRoot && !dspStudio[1].alienVoiceRoot);
    saved = scheduler.voice[0].runtime;
    assert(PartyboardMusyxSampleMemoryRemove(&memory, pcm));
    assert(!PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    assert(!memcmp(&saved, &scheduler.voice[0].runtime, sizeof saved));
    assert(scheduler.failed_voice == 0);
    salDeactivateVoice(&voices[0]);
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    assert(scheduler.retired_voices == 1 && !scheduler.voice[0].retained);
}
static void test_break_and_rejection(void) {
    uint8_t pcm[2] = {0x40,0};
    reset(); assert(PartyboardMusyxSampleMemoryAdd(&memory, pcm, sizeof pcm));
    start(0, pcm, 1, 1, 0, 51, 0);
    voices[0].next = &voices[0];
    assert(!PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    assert(voices[0].state == 1 && !scheduler.voice[0].retained);
    voices[0].next = NULL; voices[0].flags = 0x80000000U;
    assert(!PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    voices[0].flags = 0; voices[0].srcTypeSelect = 0;
    assert(!PartyboardMusyxVoiceSchedulerRender(&scheduler, buses)); /* No coefficient provider. */
    voices[0].srcTypeSelect = 2;
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    clear_controls(); voices[0].postBreak = 1;
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    assert(!voices[0].state && dspStudio[0].hostDPopSum.l == 8191 && !callbacks);
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    assert(!scheduler.retired_voices && dspStudio[0].hostDPopSum.l == 8191);
    start(0, pcm, 1, 1, 0, 52, 0); voices[0].changed[0] = 0x20; voices[0].startupBreak = 1;
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    assert(!voices[0].state && !voices[0].startupBreak && !callbacks);
}
static void test_move_to_disabled_studio(void) {
    uint8_t pcm[14];
    size_t position;
    reset();
    for(unsigned i = 0; i < 7; ++i) { pcm[i * 2] = (uint8_t)((i + 1) * 0x10); pcm[i * 2 + 1] = 0; }
    assert(PartyboardMusyxSampleMemoryAdd(&memory, pcm, sizeof pcm));
    start(0, pcm, 7, 7, 0, 61, 0);
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    clear_controls(); position = scheduler.voice[0].runtime.sample.source.pcm.position;
    dspStudio[1].state = 0; salReconnectVoice(&voices[0], 1);
    for(unsigned i = 0; i < 3; ++i) {
        assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
        assert(scheduler.voice[0].owned && !scheduler.voice[0].retained);
        assert(scheduler.voice[0].runtime.sample.source.pcm.position == position);
        for(unsigned f = 0; f < sizeof buses / sizeof *buses; ++f) assert(!buses[f]);
    }
    assert(dspStudio[0].hostDPopSum.l == 12287);
    dspStudio[1].state = 1;
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    assert(buses[PARTYBOARD_MUSYX_BLOCK_SAMPLES] == 14335 && scheduler.voice[0].retained);
    assert(!scheduler.retired_voices && dspStudio[0].hostDPopSum.l == 12287);
    {
        DSPvoice replacement[4] = {0};
        dspVoice = replacement;
        assert(!PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
        dspVoice = voices;
    }
}
static void test_scheduled_keyoff_and_break(void) {
    uint8_t pcm[2] = {0x40,0};
    unsigned release_blocks;
    reset(); assert(PartyboardMusyxSampleMemoryAdd(&memory, pcm, sizeof pcm));
    start(0, pcm, 1, 1, 0, 71, 0); voices[0].adsr.data.linear.rTime = 4;
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    clear_controls(); hwSetTimeOffset(2); hwKeyOff(0); hwSetTimeOffset(0);
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    assert(buses[0] == 8191 && buses[63 * 9] == 8191 && buses[159 * 9] < 8191);
    release_blocks = 1;
    while(voices[0].state && release_blocks < 8) {
        clear_controls(); assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
        ++release_blocks;
    }
    assert(!voices[0].state && callbacks == 1 && callback_ids[0] == 71);
    assert(!dspStudio[0].hostDPopSum.l && !scheduler.voice[0].owned);
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    assert(callbacks == 1 && !scheduler.retired_voices);
    reset(); assert(PartyboardMusyxSampleMemoryAdd(&memory, pcm, sizeof pcm));
    start(0, pcm, 1, 1, 0, 72, 0);
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    clear_controls(); voices[0].changed[2] = 0x20;
    assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
    assert(voices[0].postBreak && voices[0].state == 2 && !callbacks);
    {
        int32_t residual = PartyboardMusyxDepopSigned(pbs[0].dpop.aL);
        assert(residual > 0 && residual < 8191);
        clear_controls(); assert(PartyboardMusyxVoiceSchedulerRender(&scheduler, buses));
        assert(!voices[0].state && scheduler.retired_voices == 1);
        assert(dspStudio[0].hostDPopSum.l == residual && !callbacks);
    }
}
int main(void) {
    test_polyphony_and_stop(); test_reuse(); test_natural_completion();
    test_move_and_retired_sample(); test_break_and_rejection(); test_move_to_disabled_studio();
    test_scheduled_keyoff_and_break();
    puts("PASS owned voice scheduler: polyphony, delayed startup, slot/sample reuse, one-shot completion, priority callbacks, studio move, retired samples and bounded rejection");
    return 0;
}
