#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/port/musyx_block_driver.h"
#include "../src/port/musyx_studio_process.h"
#include "../src/port/musyx_aux_runtime.h"

/* Original hw_dspctrl.c command construction determines ring selection,
 * previous-main inputs, master selection and DoDepopFade. Numeric boundaries
 * were cross-checked with Dolphin's primary AX command documentation/code:
 * https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/HW/DSPHLE/UCodes/AX.cpp
 * https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/HW/DSPHLE/UCodes/AX.h
 * These tests use independent hand-calculated vectors, not copied emulator
 * implementation or a claim of original DSP ROM/hardware equivalence. */
static DSPvoice voices[2];
static _PB pbs[2];
static _SPB spbs[3];
static int32_t main_buffers[3][2][480], aux_buffers[3][2][3][480], surround[160];
static int32_t scratch[3 * 160 * 9];
static PartyboardMusyxStudioProcessor processor;
static PartyboardMusyxSampleMemory memory;
static PartyboardMusyxVoiceScheduler scheduler;
static PartyboardMusyxBlockDriver driver;
static uint8_t pcm[2] = {0x40, 0};

static void reset(unsigned studios) {
    memset(voices, 0, sizeof voices); memset(pbs, 0, sizeof pbs); memset(spbs, 0, sizeof spbs);
    memset(dspStudio, 0, sizeof dspStudio); memset(main_buffers, 0, sizeof main_buffers);
    memset(aux_buffers, 0, sizeof aux_buffers); memset(surround, 0, sizeof surround);
    memset(scratch, 0, sizeof scratch); memset(&processor, 0, sizeof processor);
    dspVoice = voices; salNumVoices = 2; salMaxStudioNum = (u8)studios;
    salFrame = salAuxFrame = 0; dspSurround = surround;
    for(unsigned studio = 0; studio < studios; ++studio) {
        DSPstudioinfo *s = &dspStudio[studio];
        s->spb = &spbs[studio]; s->state = 1; s->type = SND_STUDIO_TYPE_STD;
        s->isMaster = studio + 1 == studios;
        for(unsigned slot = 0; slot < 2; ++slot) s->main[slot] = main_buffers[studio][slot];
        for(unsigned slot = 0; slot < 3; ++slot) {
            s->auxA[slot] = aux_buffers[studio][0][slot]; s->auxB[slot] = aux_buffers[studio][1][slot];
        }
    }
    assert(sizeof(_SPB) == 54 && sizeof(DSPhostDPop) == 36);
    assert(PartyboardMusyxStudioProcessorInit(&processor, studios));
    assert(!PartyboardMusyxStudioProcessorInit(&processor, studios));
}
static void rotate(void) { salHandleAuxProcessing(); salFrame ^= 1; salAuxFrame = (salAuxFrame + 1) % 3; }
static void no_aux(u8 reason, SND_AUX_INFO *info, void *context) { (void)reason; (void)info; (void)context; }
static void test_previous_inputs_and_inactive(void) {
    SND_STUDIO_INPUT desc = {0};
    reset(2); desc.srcStudio = 0; desc.vol = 64; desc.volA = 32; desc.volB = 16;
    assert(salAddStudioInput(&dspStudio[1], &desc));
    assert(dspStudio[1].in[0].vol == 16512 && dspStudio[1].in[0].volA == 8256 && dspStudio[1].in[0].volB == 4128);
    dspStudio[1].in[0].vol = 16384; dspStudio[1].in[0].volA = 8192; dspStudio[1].in[0].volB = 4096;
    dspStudio[1].auxAHandler = dspStudio[1].auxBHandler = no_aux;
    for(unsigned frame = 0; frame < 160; ++frame) {
        main_buffers[0][1][frame] = 100; main_buffers[0][1][160 + frame] = -100;
        main_buffers[0][1][320 + frame] = 8;
        scratch[frame * 9] = 1000; scratch[frame * 9 + 1] = -1001; scratch[frame * 9 + 2] = 5;
    }
    assert(PartyboardMusyxStudioProcess(&processor, scratch, 2));
    for(unsigned frame = 0; frame < 160; ++frame) {
        assert(processor.master_lrs[frame * 3] == 50 && processor.master_lrs[frame * 3 + 1] == -50);
        assert(processor.stereo[frame * 2] == 50 && processor.stereo[frame * 2 + 1] == -50);
        assert(surround[frame] == 4 && main_buffers[0][0][frame] == 1000);
        assert(aux_buffers[1][0][0][frame] == 25 && aux_buffers[1][0][0][160 + frame] == -25);
        assert(aux_buffers[1][1][0][frame] == 12 && aux_buffers[1][1][0][160 + frame] == -13);
    }
    rotate(); memset(scratch, 0, sizeof scratch);
    for(unsigned frame = 0; frame < 160; ++frame) scratch[frame * 9] = -2000;
    assert(PartyboardMusyxStudioProcess(&processor, scratch, 2));
    for(unsigned frame = 0; frame < 160; ++frame) {
        assert(processor.master_lrs[frame * 3] == 500 && processor.master_lrs[frame * 3 + 1] == -501);
        assert(processor.stereo[frame * 2] == 496 && processor.stereo[frame * 2 + 1] == -497);
        assert(surround[frame] == 2);
    }
    rotate(); dspStudio[0].state = 0; memset(scratch, 0, sizeof scratch);
    assert(PartyboardMusyxStudioProcess(&processor, scratch, 2));
    assert(main_buffers[0][0][0] == 1000 && main_buffers[0][1][0] == -2000);
    /* Inactive sources retain buffers; downstream reads the previous parity.
     * No traversal reorder or same-block feedback is invented for cycles. */
    assert(processor.master_lrs[0] == -963 && processor.master_lrs[1] == -38);
    assert(processor.stereo[0] == -965 && processor.stereo[1] == -36);
}
static void test_depop_and_matrix(void) {
    const int32_t values[] = {0, 159, -159, 160, -160, 3199, -3199, 3200, -3200, 8388607, -8388607};
    for(unsigned n = 0; n < sizeof values / sizeof *values; ++n) {
        int32_t host[9]; uint16_t words[27];
        reset(1);
        for(unsigned channel = 0; channel < 9; ++channel) host[channel] = values[(n + channel) % 11];
        memcpy(&dspStudio[0].hostDPopSum, host, sizeof host);
        assert(PartyboardMusyxStudioProcess(&processor, scratch, 1));
        memcpy(words, &spbs[0], sizeof words);
        for(unsigned channel = 0; channel < 9; ++channel) {
            int32_t step = -(host[channel] / 160);
            if(step > 20) step = 20; if(step < -20) step = -20;
            assert(words[channel * 3] == (uint16_t)((uint32_t)host[channel] >> 16));
            assert(words[channel * 3 + 1] == (uint16_t)host[channel]);
            assert(PartyboardMusyxDepopSigned(words[channel * 3 + 2]) == step);
            for(unsigned frame = 0; frame < 160; ++frame) assert(scratch[frame * 9 + channel] == host[channel] + (int32_t)frame * step);
            host[channel] += 160 * step;
        }
        assert(!memcmp(host, &dspStudio[0].hostDPopSum, sizeof host));
        rotate(); memset(scratch, 0, sizeof scratch);
        assert(PartyboardMusyxStudioProcess(&processor, scratch, 1));
        assert(scratch[0] == host[0]);
    }
    reset(2); dspStudio[0].isMaster = 1;
    for(unsigned frame = 0; frame < 160; ++frame) {
        scratch[frame * 9] = 40000; scratch[frame * 9 + 1] = -40000;
        scratch[160 * 9 + frame * 9] = -100; scratch[160 * 9 + frame * 9 + 1] = 100;
        scratch[160 * 9 + frame * 9 + 2] = 300;
        surround[frame] = 10;
    }
    assert(PartyboardMusyxStudioProcess(&processor, scratch, 2));
    assert(processor.master_lrs[0] == 39900 && processor.master_lrs[1] == -39900 && processor.master_lrs[2] == 300);
    assert(processor.stereo[0] == 32767 && processor.stereo[1] == -32768 && surround[0] == 300);
    assert(processor.speakers[0]==32767&&processor.speakers[1]==-32768);
    assert(!processor.speakers[2]&&!processor.speakers[3]&&processor.speakers[4]==10&&processor.speakers[5]==10);
    reset(1);
    for(unsigned frame=0;frame<160;++frame)surround[frame]=frame&1?INT32_MIN:INT32_MAX;
    assert(PartyboardMusyxStudioProcess(&processor,scratch,1));
    for(unsigned frame=0;frame<160;++frame){
        const int16_t rear=frame&1?-32768:32767;
        assert(!processor.speakers[frame*6]&&!processor.speakers[frame*6+1]);
        assert(!processor.speakers[frame*6+2]&&!processor.speakers[frame*6+3]);
        assert(processor.speakers[frame*6+4]==rear&&processor.speakers[frame*6+5]==rear);
    }
    reset(1); dspStudio[0].hostDPopSum.l = 1; scratch[0] = INT32_MAX;
    assert(PartyboardMusyxStudioProcess(&processor, scratch, 1)); assert(scratch[0] == INT32_MIN);
    assert(PartyboardMusyxStudioScale32(-1, 1) == -1);
    assert(PartyboardMusyxStudioScale32(INT32_MIN, 65535) == 65536);
    assert(PartyboardMusyxStudioScale32(INT32_MAX, 65535) == -65538);
}
static void test_feedback_and_seven_inputs(void) {
    reset(2);
    dspStudio[0].numInputs = 1; dspStudio[0].in[0].studio = 1; dspStudio[0].in[0].vol = 32768;
    dspStudio[1].numInputs = 7;
    for(unsigned input = 0; input < 7; ++input) {
        dspStudio[1].in[input].studio = input ? 0 : 1;
        dspStudio[1].in[input].vol = 32768;
    }
    for(unsigned frame = 0; frame < 160; ++frame) {
        main_buffers[0][1][frame] = 100; main_buffers[1][1][frame] = 10;
        scratch[frame * 9] = 1000; scratch[160 * 9 + frame * 9] = 2000;
    }
    assert(PartyboardMusyxStudioProcess(&processor, scratch, 2));
    assert(main_buffers[0][0][0] == 1010 && main_buffers[1][0][0] == 2610 && processor.stereo[0] == 2610);
    /* Inactive masters contribute no current bus. Prior surround still has
     * its original single-block matrix lifetime, then clears. */
    reset(1); dspStudio[0].state = 0; surround[0] = 9;
    assert(PartyboardMusyxStudioProcess(&processor, scratch, 1));
    assert(processor.stereo[0] == -9 && processor.stereo[1] == 9 && !surround[0]);
    rotate(); assert(PartyboardMusyxStudioProcess(&processor, scratch, 1));
    assert(!processor.stereo[0] && !processor.stereo[1]);
}
typedef struct { unsigned calls, group; } Aux;
static Aux aux[2];
static void multiply_aux(u8 reason, SND_AUX_INFO *info, void *context) {
    Aux *a = context; unsigned slot = (a->calls + 2) % 3;
    assert(reason == SND_AUX_REASON_BUFFERUPDATE && a->group < 2);
    assert(info->data.bufferUpdate.left == aux_buffers[0][a->group][slot]);
    assert(info->data.bufferUpdate.right == aux_buffers[0][a->group][slot] + 160);
    assert(info->data.bufferUpdate.surround == aux_buffers[0][a->group][slot] + 320);
    for(unsigned i = 0; i < 480; ++i) info->data.bufferUpdate.left[i] *= (int32_t)(a->group + 2);
    ++a->calls;
}
static void test_aux_rings_and_real_reverb(void) {
    SND_AUX_REVERBSTD reverb = {0};
    reset(1); memset(aux, 0, sizeof aux); aux[1].group = 1;
    dspStudio[0].auxAHandler = dspStudio[0].auxBHandler = multiply_aux;
    dspStudio[0].auxAUser = &aux[0]; dspStudio[0].auxBUser = &aux[1];
    for(unsigned block = 0; block < 9; ++block) {
        memset(scratch, 0, sizeof scratch);
        for(unsigned frame = 0; frame < 160; ++frame) for(unsigned channel = 0; channel < 3; ++channel) {
            scratch[frame * 9 + channel] = 10 + (int32_t)channel;
            scratch[frame * 9 + 3 + channel] = 100 + (int32_t)(block + channel + frame);
            scratch[frame * 9 + 6 + channel] = 200 + (int32_t)(block + channel + frame);
        }
        assert(PartyboardMusyxStudioProcess(&processor, scratch, 1));
        for(unsigned frame = 0; frame < 160; ++frame) for(unsigned channel = 0; channel < 3; ++channel) {
            int32_t expected = 10 + (int32_t)channel;
            if(block >= 2) expected += 2 * (100 + (int32_t)(block - 2 + channel + frame)) + 3 * (200 + (int32_t)(block - 2 + channel + frame));
            assert(processor.master_lrs[frame * 3 + channel] == expected);
            assert(main_buffers[0][salFrame][channel * 160 + frame] == expected);
        }
        assert(aux[0].calls == block && aux[1].calls == block);
        rotate(); assert(aux[0].calls == block + 1 && aux[1].calls == block + 1);
    }
    reset(1); salHooks.malloc = malloc; salHooks.free = free;
    reverb.coloration = reverb.mix = .5f; reverb.time = 1.f; reverb.damping = .2f; reverb.preDelay = .01f;
    assert(sndAuxCallbackPrepareReverbSTD(&reverb));
    dspStudio[0].auxAHandler = sndAuxCallbackReverbSTD; dspStudio[0].auxAUser = &reverb;
    for(unsigned block = 0; block < 3; ++block) {
        memset(scratch, 0, sizeof scratch);
        for(unsigned frame = 0; frame < 160; ++frame) {
            scratch[frame * 9 + 3] = 1000; scratch[frame * 9 + 4] = 2000; scratch[frame * 9 + 5] = -1000;
        }
        assert(PartyboardMusyxStudioProcess(&processor, scratch, 1));
        if(block < 2) assert(!processor.master_lrs[0]);
        else assert(processor.master_lrs[0] == 300 && processor.master_lrs[1] == 600 && processor.master_lrs[2] == -300);
        rotate();
    }
    assert(sndAuxCallbackShutdownReverbSTD(&reverb));
}
static void rejected_without_mutation(void) {
    PartyboardMusyxStudioProcessor before = processor;
    DSPstudioinfo studios_before[8]; int32_t scratch_before[3 * 160 * 9];
    _SPB spb_before[3]; int32_t main_before[3][2][480], aux_before[3][2][3][480], surround_before[160];
    memcpy(studios_before, dspStudio, sizeof studios_before); memcpy(scratch_before, scratch, sizeof scratch);
    memcpy(spb_before, spbs, sizeof spbs); memcpy(main_before, main_buffers, sizeof main_buffers);
    memcpy(aux_before, aux_buffers, sizeof aux_buffers); memcpy(surround_before, surround, sizeof surround);
    assert(!PartyboardMusyxStudioProcess(&processor, scratch, processor.studios));
    assert(!memcmp(&before, &processor, sizeof before) && !memcmp(studios_before, dspStudio, sizeof studios_before));
    assert(!memcmp(scratch_before, scratch, sizeof scratch) && !memcmp(spb_before, spbs, sizeof spbs));
    assert(!memcmp(main_before, main_buffers, sizeof main_before) && !memcmp(aux_before, aux_buffers, sizeof aux_before));
    assert(!memcmp(surround_before, surround, sizeof surround));
}
static void test_transactional_graph_rejection(void) {
    reset(2); dspStudio[1].hostDPopSum.l = 8388608; rejected_without_mutation();
    reset(2); dspStudio[1].type = SND_STUDIO_TYPE_DPL2; rejected_without_mutation();
    reset(2); dspStudio[1].numInputs = 8; rejected_without_mutation();
    reset(2); dspStudio[1].numInputs = 1; dspStudio[1].in[0].studio = 2; rejected_without_mutation();
    reset(2); dspStudio[0].auxAHandler = no_aux; dspStudio[0].auxA[2] = NULL; rejected_without_mutation();
    reset(2); dspStudio[0].main[1] = dspStudio[0].main[0]; rejected_without_mutation();
    reset(2); dspStudio[1].main[0] = dspStudio[0].main[0] + 1; rejected_without_mutation();
    reset(2); dspStudio[1].spb = NULL; rejected_without_mutation();
    reset(2); salAuxFrame = 3; rejected_without_mutation();
    reset(2); salFrame = 2; rejected_without_mutation();
    reset(2); processor.blocks = UINT64_MAX; rejected_without_mutation();
}
typedef struct { unsigned calls; uint32_t hash; int response; } Output;
static void start_driver(void) {
    reset(1); memset(&memory, 0, sizeof memory); memset(&scheduler, 0, sizeof scheduler); memset(&driver, 0, sizeof driver);
    memset(&synthInfo, 0, sizeof synthInfo); salHooks.malloc = malloc; salHooks.free = free;
    synthInfo.voiceNum = synthInfo.maxMusic = synthInfo.maxSFX = 2; synthInfo.studioNum = 1; synthInfo.numSamples = 32;
    synthInit(32000, 2); seqInit(); dataInitStack(); streamInit(); vsInit(); s3dInit(0); sndActive = 1;
    assert(PartyboardMusyxSampleMemoryAdd(&memory, pcm, sizeof pcm));
    assert(PartyboardMusyxVoiceSchedulerInit(&scheduler, 2, 1, &memory, NULL, 0));
    assert(PartyboardMusyxBlockDriverInit(&driver, &scheduler, 60));
    voices[0].pb = &pbs[0]; voices[0].smp_id = 1; voices[0].mesgCallBackUserValue = 7;
    voices[0].smp_info.addr = pcm; voices[0].smp_info.info = 32000;
    voices[0].smp_info.length = voices[0].smp_info.loopLength = 1; voices[0].smp_info.compType = 2;
    voices[0].srcTypeSelect = 2; voices[0].playInfo.pitch = 65536;
    voices[0].adsr.data.linear.sLevel = 32767; voices[0].volL = 16384;
    voices[0].virtualSampleID = UINT32_MAX; salActivateVoice(&voices[0], 0);
}
static int output(void *context, int32_t *buses, unsigned studios) {
    Output *out = context;
    assert(studios == 1 && buses == scratch && processor.blocks == out->calls + 1);
    assert(synthRealTime == (uint64_t)out->calls * 1280);
    for(unsigned i = 0; i < 320; ++i) out->hash = (out->hash ^ (uint16_t)processor.stereo[i]) * 16777619U;
    if(out->calls < 3) assert(processor.stereo[0] == 8191 && processor.stereo[1] == 0);
    if(out->calls == 3) {
        assert(processor.stereo[0] == 8191 && processor.stereo[318] == 5011);
        assert(dspStudio[0].hostDPopSum.l == 4991);
    }
    /* Scheduler diagnostics describe the current block, not a lifetime sum. */
    assert(scheduler.retired_voices == (out->calls == 3 ? 1U : 0U));
    ++out->calls; return out->response;
}
static uint32_t run_output(int response) {
    Output out = {0, 2166136261U, 0}; out.response = response;
    start_driver();
    assert(PartyboardMusyxBlockDriverTickProcessed(&driver, scratch, PartyboardMusyxStudioProcess, &processor, output, &out));
    salDeactivateVoice(&voices[0]);
    for(unsigned tick = 1; tick < 60; ++tick)
        assert(PartyboardMusyxBlockDriverTickProcessed(&driver, scratch, PartyboardMusyxStudioProcess, &processor, output, &out));
    assert(driver.completed_blocks == 200 && processor.blocks == 200 && out.calls == 200 && synthRealTime == 256000);
    assert(driver.accepted_blocks == (response > 0 ? 200 : 0) && driver.device_errors == (response < 0 ? 200 : 0));
    assert(!scheduler.voice[0].retained && !scheduler.voice[0].owned && !scheduler.retired_voices);
    assert(dspStudio[0].hostDPopSum.l == 31 && processor.stereo[0] == 31);
    s3dExit(); synthExit(); synthVoice = NULL; sndActive = 0;
    return out.hash;
}
static void test_processing_failure_latch(void) {
    Output out = {0}; start_driver(); dspStudio[0].numInputs = 8;
    assert(!PartyboardMusyxBlockDriverTickProcessed(&driver, scratch, PartyboardMusyxStudioProcess, &processor, output, &out));
    assert(driver.failed && !driver.busy && driver.mixed_blocks == 1 && !driver.completed_blocks);
    assert(!out.calls && !processor.blocks && !synthRealTime && !driver.dropped_blocks && !driver.device_errors);
    dspStudio[0].numInputs = 0;
    assert(!PartyboardMusyxBlockDriverTickProcessed(&driver, scratch, PartyboardMusyxStudioProcess, &processor, output, &out));
    assert(driver.mixed_blocks == 1 && !processor.blocks);
    s3dExit(); synthExit(); synthVoice = NULL; sndActive = 0;
}
static int effect_output(void *context, int32_t *buses, unsigned studios) {
    unsigned *calls = context;
    assert(buses == scratch && studios == 1 && *calls < 3);
    assert(processor.stereo[0] == (*calls < 2 ? 8191 : 10648));
    assert(processor.stereo[1] == 0); ++*calls; return 1;
}
static void test_driver_reverb_and_no_device(void) {
    SND_AUX_REVERBSTD reverb = {0}; unsigned calls = 0;
    start_driver(); voices[0].volLa = 16384;
    reverb.coloration = reverb.mix = .5f; reverb.time = 1.f; reverb.damping = .2f; reverb.preDelay = .01f;
    assert(sndAuxCallbackPrepareReverbSTD(&reverb));
    dspStudio[0].auxAHandler = sndAuxCallbackReverbSTD; dspStudio[0].auxAUser = &reverb;
    assert(PartyboardMusyxBlockDriverTickProcessed(&driver, scratch, PartyboardMusyxStudioProcess, &processor, effect_output, &calls));
    assert(calls == 3 && driver.accepted_blocks == 3);
    assert(PartyboardMusyxBlockDriverTickProcessed(&driver, scratch, PartyboardMusyxStudioProcess, &processor, NULL, NULL));
    assert(driver.completed_blocks == 6 && processor.blocks == 6 && driver.dropped_blocks == 3);
    assert(synthRealTime == 7680 && processor.stereo[0] > 8191);
    assert(sndAuxCallbackShutdownReverbSTD(&reverb));
    s3dExit(); synthExit(); synthVoice = NULL; sndActive = 0;
}
static int high_reverb_output(void *context, int32_t *buses, unsigned studios) {
    unsigned *calls = context;
    assert(buses == scratch && studios == 1 && *calls < 3);
    /* Authored mix .8: first returned dry-reverb contribution is982 from8191.
     * Original aux ring returns it after two blocks. */
    assert(processor.stereo[0] == (*calls < 2 ? 8191 : 9173));
    assert(processor.stereo[1] == 0); ++*calls; return 1;
}
static void test_driver_authored_high_reverb(void) {
    SND_AUX_REVERBHI reverb = {0}; unsigned calls = 0;
    start_driver(); voices[0].volLa = 16384;
    reverb.coloration = .5f; reverb.mix = .8f; reverb.time = 1.5f;
    reverb.damping = .5f; reverb.preDelay = .01f; reverb.crosstalk = 0.f;
    assert(sndAuxCallbackPrepareReverbHI(&reverb));
    dspStudio[0].auxAHandler = sndAuxCallbackReverbHI; dspStudio[0].auxAUser = &reverb;
    assert(PartyboardMusyxBlockDriverTickProcessed(&driver, scratch, PartyboardMusyxStudioProcess, &processor, high_reverb_output, &calls));
    assert(calls == 3 && processor.blocks == 3 && synthRealTime == 3840);
    assert(sndAuxCallbackShutdownReverbHI(&reverb));
    s3dExit(); synthExit(); synthVoice = NULL; sndActive = 0;
}
static int owned_delay_output(void *context, int32_t *buses, unsigned studios) {
    unsigned *calls = context;
    assert(buses == scratch && studios == 1 && *calls < 6);
    assert(processor.stereo[0] == (*calls < 3 ? 8191 : 16382));
    assert(processor.stereo[1] == 0); ++*calls; return 1;
}
static void test_driver_owned_delay(void) {
    uint8_t metadata[40] = {0}; unsigned calls = 0;
    PartyboardMsmContainer container = {0}; PartyboardMusyxAuxRuntime aux_runtime = {0};
    metadata[0] = 3;
    for(unsigned channel = 0; channel < 3; ++channel) {
        metadata[4 + channel * 4 + 3] = 6; metadata[28 + channel * 4 + 3] = 100;
    }
    container.data = metadata; container.bytes = sizeof metadata;
    container.section[PARTYBOARD_MSM_AUX].bytes = sizeof metadata;
    start_driver(); voices[0].volLb = 16384;
    assert(PartyboardMusyxAuxRuntimeInit(&aux_runtime, 0));
    assert(PartyboardMusyxAuxRuntimeSelect(&aux_runtime, &container, -2, 0));
    for(unsigned tick = 0; tick < 2; ++tick)
        assert(PartyboardMusyxBlockDriverTickProcessed(&driver, scratch, PartyboardMusyxStudioProcess, &processor, owned_delay_output, &calls));
    assert(calls == 6 && synthRealTime == 7680 && processor.blocks == 6);
    assert(PartyboardMusyxAuxRuntimeShutdown(&aux_runtime));
    assert(!dspStudio[0].auxAHandler && !dspStudio[0].auxBHandler && !dspStudio[0].auxBUser);
    assert(PartyboardMusyxBlockDriverTickProcessed(&driver, scratch, PartyboardMusyxStudioProcess, &processor, NULL, NULL));
    assert(processor.stereo[0] == 8191); /* Released owner is no longer called or returned. */
    s3dExit(); synthExit(); synthVoice = NULL; sndActive = 0;
}
int main(void) {
    uint32_t accepted, dropped, failed;
    test_previous_inputs_and_inactive(); test_depop_and_matrix(); test_feedback_and_seven_inputs(); test_aux_rings_and_real_reverb();
    test_transactional_graph_rejection();
    accepted = run_output(1); dropped = run_output(0); failed = run_output(-1);
    assert(accepted == dropped && dropped == failed); test_processing_failure_latch(); test_driver_reverb_and_no_device();
    test_driver_authored_high_reverb();
    test_driver_owned_delay();
    printf("PASS studio processor: previous inputs, nine depop ramps, aux rings/real reverb, master matrix/PCM, transactional rejection; accepted/dropped/error output hash=%u\n", accepted);
    return 0;
}
