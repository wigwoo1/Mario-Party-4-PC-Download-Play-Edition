#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/port/musyx_pcm_output.h"

static DSPvoice voices[2];
static _PB pbs[2];
static _SPB spb;
static int32_t main_buffers[2][480], aux_buffers[2][3][480], surround[160], scratch[160 * 9];
static PartyboardMusyxSampleMemory memory;
static PartyboardMusyxVoiceScheduler scheduler;
static PartyboardMusyxBlockDriver driver;
static PartyboardMusyxStudioProcessor processor;
static PartyboardMusyxPcmBridge bridge;
static PartyboardPcmOutput output;
static uint8_t pcm[2] = {0x40, 0};
static int16_t expected_pcm[32000 * 2], prefill[8000 * 6];
static int16_t expected_speakers[32000 * 6];
static unsigned replacement_src;

static void start_voice(void) {
    assert(!voices[0].state);
    memset(&voices[0], 0, sizeof voices[0]); memset(&pbs[0], 0, sizeof pbs[0]);
    voices[0].pb = &pbs[0]; voices[0].smp_id = 1; voices[0].mesgCallBackUserValue = 7;
    voices[0].smp_info.addr = pcm; voices[0].smp_info.info = 32000;
    voices[0].smp_info.length = voices[0].smp_info.loopLength = 1; voices[0].smp_info.compType = 2;
    voices[0].srcTypeSelect = replacement_src ? 0 : 2;
    voices[0].srcCoefSelect = replacement_src ? 2 : 0; voices[0].playInfo.pitch = 65536;
    voices[0].adsr.data.linear.sLevel = 32767;
    voices[0].volL = 16384; voices[0].volR = voices[0].volS = 8192;
    voices[0].virtualSampleID = UINT32_MAX; salActivateVoice(&voices[0], 0);
}
static void initialize(unsigned channels, int stream) {
    SDL_AudioSpec spec = {SDL_AUDIO_S16, (int)channels, 32000};
    assert(!output.stream);
    memset(voices, 0, sizeof voices); memset(pbs, 0, sizeof pbs); memset(&spb, 0, sizeof spb);
    memset(dspStudio, 0, sizeof dspStudio); memset(main_buffers, 0, sizeof main_buffers);
    memset(aux_buffers, 0, sizeof aux_buffers); memset(surround, 0, sizeof surround);
    memset(&memory, 0, sizeof memory); memset(&scheduler, 0, sizeof scheduler); memset(&driver, 0, sizeof driver);
    memset(&processor, 0, sizeof processor); memset(&bridge, 0, sizeof bridge); memset(&output, 0, sizeof output);
    dspVoice = voices; dspSurround = surround; salNumVoices = 2; salMaxStudioNum = 1; salFrame = salAuxFrame = 0;
    salHooks.malloc = malloc; salHooks.free = free;
    memset(&synthInfo, 0, sizeof synthInfo); synthInfo.voiceNum = synthInfo.maxMusic = synthInfo.maxSFX = 2;
    synthInfo.studioNum = 1; synthInfo.numSamples = 32;
    synthInit(32000, 2); seqInit(); dataInitStack(); streamInit(); vsInit(); s3dInit(0); sndActive = 1;
    dspStudio[0].state = dspStudio[0].isMaster = 1; dspStudio[0].type = SND_STUDIO_TYPE_STD; dspStudio[0].spb = &spb;
    for(unsigned slot = 0; slot < 2; ++slot) dspStudio[0].main[slot] = main_buffers[slot];
    for(unsigned slot = 0; slot < 3; ++slot) { dspStudio[0].auxA[slot] = aux_buffers[0][slot]; dspStudio[0].auxB[slot] = aux_buffers[1][slot]; }
    assert(PartyboardMusyxSampleMemoryAdd(&memory, pcm, sizeof pcm));
    if(replacement_src) {
        assert(PartyboardMusyxVoiceSchedulerInitDefault(&scheduler, 2, 1, &memory));
        assert(scheduler.banks == PartyboardSrcDefaultCoefficients(NULL) && scheduler.bank_words == 1536);
    } else assert(PartyboardMusyxVoiceSchedulerInit(&scheduler, 2, 1, &memory, NULL, 0));
    assert(PartyboardMusyxBlockDriverInit(&driver, &scheduler, 60));
    assert(PartyboardMusyxStudioProcessorInit(&processor, 1));
    if(stream) {
        output.stream = SDL_CreateAudioStream(&spec, &spec); assert(output.stream);
        output.frequency = 32000; output.channels = channels;
    }
    assert(PartyboardMusyxPcmBridgeInit(&bridge, &driver, &processor, stream ? &output : NULL));
    assert(!PartyboardMusyxPcmBridgeInit(&bridge, &driver, &processor, &output));
    start_voice();
}
static void finish(void) {
    PartyboardPcmOutputClose(&output);
    salDeactivateVoice(&voices[0]); s3dExit(); synthExit(); synthVoice = NULL; sndActive = 0;
}
/* Independently calculate the known constant PCM fixture, its original host
 * depop formula and previous-surround matrix. No renderer output is used as
 * expected data. Stop at block40, restart at150, at exact60Hz tick boundaries. */
static uint32_t expected(void) {
    int32_t host[3] = {0}, previous[160] = {0}; uint32_t hash = 2166136261U;
    for(unsigned block = 0; block < 200; ++block) {
        int live = block < 40 || block >= 150;
        int32_t delta[3] = {0};
        if(block == 40) { host[0] = 8191; host[1] = host[2] = 4095; }
        if(!live) for(unsigned channel = 0; channel < 3; ++channel) {
            delta[channel] = -(host[channel] / 160);
            if(delta[channel] < -20) delta[channel] = -20;
        }
        for(unsigned frame = 0; frame < 160; ++frame) {
            /* Bank2 phase0 is [0,32767,0,0]: two silent startup
             * samples, then the same rounded gains as the bypass fixture. */
            int audible = live && !(replacement_src && (block == 0 || block == 150) && frame < 2);
            int32_t l = (audible ? 8191 : 0) + host[0] + (int32_t)frame * delta[0];
            int32_t r = (audible ? 4095 : 0) + host[1] + (int32_t)frame * delta[1];
            int32_t s = (audible ? 4095 : 0) + host[2] + (int32_t)frame * delta[2];
            int16_t left = (int16_t)(l - previous[frame]), right = (int16_t)(r + previous[frame]);
            unsigned offset = (block * 160 + frame) * 2;
            expected_pcm[offset] = left; expected_pcm[offset + 1] = right; previous[frame] = s;
            const unsigned six=(block*160+frame)*6;
            expected_speakers[six]=(int16_t)l;expected_speakers[six+1]=(int16_t)r;
            expected_speakers[six+2]=expected_speakers[six+3]=0;
            /* Original reference matrix: right-r == previous mono surround. */
            expected_speakers[six+4]=expected_speakers[six+5]=(int16_t)(right-r);
            hash = (hash ^ (uint16_t)left) * 16777619U; hash = (hash ^ (uint16_t)right) * 16777619U;
        }
        if(!live) for(unsigned channel = 0; channel < 3; ++channel) host[channel] += 160 * delta[channel];
    }
    return hash;
}
static void drain_and_compare(unsigned first_block, unsigned blocks, unsigned channels) {
    int16_t recovered[640 * 6]; size_t frames = blocks * 160;
    int bytes = (int)(frames * channels * sizeof(int16_t));
    assert(blocks <= 4 && PartyboardPcmOutputFinish(&output));
    assert(SDL_GetAudioStreamData(output.stream, recovered, bytes) == bytes);
    for(size_t frame = 0; frame < frames; ++frame) {
        size_t expected_frame = (size_t)first_block * 160 + frame;
        const int16_t *expected_frame_pcm=channels==6?expected_speakers+expected_frame*6:expected_pcm+expected_frame*2;
        if(recovered[frame * channels] != expected_frame_pcm[0])
            fprintf(stderr, "PCM mismatch block%u frame%zu left%d expected%d\n", first_block, frame,
                recovered[frame * channels], expected_frame_pcm[0]);
        for(unsigned channel=0;channel<channels;++channel)
            assert(recovered[frame*channels+channel]==expected_frame_pcm[channel]);
    }
    assert(SDL_GetAudioStreamQueued(output.stream) == 0);
}
static uint32_t state_hash(void) {
    uint32_t hash = 2166136261U;
    for(unsigned frame = 0; frame < 160; ++frame) {
        hash = (hash ^ (uint32_t)surround[frame]) * 16777619U;
        for(unsigned channel = 0; channel < 3; ++channel)
            hash = (hash ^ (uint32_t)processor.master_lrs[frame * 3 + channel]) * 16777619U;
    }
    hash = (hash ^ voices[0].playInfo.posHi) * 16777619U;
    hash = (hash ^ voices[0].playInfo.posLo) * 16777619U;
    hash = (hash ^ voices[0].state) * 16777619U;
    hash = (hash ^ (uint32_t)synthRealTime) * 16777619U;
    return hash;
}
/*0=drained stream;1=real full SDL queue;2=no output;3=local config error;
 *4=full queue then recovery;5=detach/reattach output at tick boundaries. */
static uint32_t run_mode(unsigned channels, unsigned mode) {
    initialize(channels, mode != 2);
    if(mode == 1 || mode == 4) assert(PartyboardPcmOutputSubmit(&output, prefill, 8000) == 1);
    if(mode == 3) output.frequency = 44100;
    for(unsigned tick = 0; tick < 60; ++tick) {
        unsigned before = (unsigned)driver.completed_blocks, after;
        if(tick == 12) salDeactivateVoice(&voices[0]);
        if(tick == 45) start_voice();
        if(mode == 4 && tick == 30) assert(SDL_ClearAudioStream(output.stream));
        if(mode == 5 && tick == 15) assert(PartyboardMusyxPcmBridgeSetOutput(&bridge, NULL));
        if(mode == 5 && tick == 30) assert(PartyboardMusyxPcmBridgeSetOutput(&bridge, &output));
        assert(PartyboardMusyxPcmBridgeTick(&bridge, scratch));
        after = (unsigned)driver.completed_blocks;
        assert(after == (tick + 1) * 200 / 60 && processor.blocks == after && bridge.last_output_block == after);
        assert(synthRealTime == (uint64_t)after * 1280 && !driver.failed && !driver.busy);
        if(mode == 0 || (mode == 4 && tick >= 30) || (mode == 5 && (tick < 15 || tick >= 30)))
            drain_and_compare(before, after - before, channels);
    }
    if(mode == 0) assert(driver.accepted_blocks == 200 && !driver.dropped_blocks && !driver.device_errors);
    if(mode == 1 || mode == 2) assert(!driver.accepted_blocks && driver.dropped_blocks == 200 && !driver.device_errors);
    if(mode == 3) assert(!driver.accepted_blocks && driver.device_errors == 200 && driver.dropped_blocks == 200);
    if(mode == 4) assert(driver.accepted_blocks == 100 && driver.dropped_blocks == 100 && !driver.device_errors);
    if(mode == 5) assert(driver.accepted_blocks == 150 && driver.dropped_blocks == 50 && !driver.device_errors);
    if(mode == 1) assert(SDL_GetAudioStreamQueued(output.stream) == (int)(8000 * channels * sizeof(int16_t)));
    assert(driver.completed_blocks == 200 && driver.control_periods == 1000 && synthRealTime == 256000);
    {
        uint64_t before = bridge.last_output_block;
        assert(PartyboardMusyxPcmBridgeSubmit(&bridge, scratch, 1) == -1);
        assert(bridge.last_output_block == before); /* No duplicate submission outside a running block. */
    }
    {
        uint32_t hash = state_hash(); finish(); return hash;
    }
}
static void test_guards_and_graph_failure(void) {
    initialize(2, 1);
    bridge.last_output_block = 1;
    assert(!PartyboardMusyxPcmBridgeTick(&bridge, scratch) && !driver.mixed_blocks);
    assert(!PartyboardMusyxPcmBridgeSetOutput(&bridge, NULL)); bridge.last_output_block = 0;
    driver.busy = 1; assert(!PartyboardMusyxPcmBridgeSetOutput(&bridge, NULL)); driver.busy = 0;
    dspStudio[0].numInputs = 8;
    assert(!PartyboardMusyxPcmBridgeTick(&bridge, scratch));
    assert(driver.failed && driver.mixed_blocks == 1 && !driver.completed_blocks && !processor.blocks);
    assert(!bridge.last_output_block && !synthRealTime && SDL_GetAudioStreamQueued(output.stream) == 0);
    dspStudio[0].numInputs = 0; assert(!PartyboardMusyxPcmBridgeTick(&bridge, scratch));
    assert(!PartyboardMusyxPcmBridgeSetOutput(&bridge, NULL)); finish();
}
static int device_probe(unsigned channels) {
    unsigned waits = 0; int before;
    initialize(channels, 0);
    if(!PartyboardPcmOutputOpenChannels(&output, 32000, channels)) { fprintf(stderr, "SDL device open: %s\n", SDL_GetError()); finish(); return 1; }
    assert(PartyboardMusyxPcmBridgeSetOutput(&bridge, &output));
    for(unsigned tick = 0; tick < 6; ++tick) assert(PartyboardMusyxPcmBridgeTick(&bridge, scratch));
    assert(driver.accepted_blocks == 20 && driver.completed_blocks == 20 && synthRealTime == 25600);
    assert(PartyboardPcmOutputFinish(&output)); before = SDL_GetAudioStreamQueued(output.stream);
    while(SDL_GetAudioStreamQueued(output.stream) > 0 && waits++ < 100) SDL_Delay(10);
    assert(SDL_GetAudioStreamQueued(output.stream) == 0);
    printf("PASS PCM bridge device: source_channels=%u destination_channels=%u driver=%s accepted_blocks=20 queue_before=%d queue_drained=1; muted probe, no audibility/speaker acceptance\n",
        channels, PartyboardPcmOutputDeviceChannels(&output), SDL_GetCurrentAudioDriver(), before);
    finish(); return 0;
}
int main(int argc, char **argv) {
    replacement_src = argc == 2 && !strcmp(argv[1], "--replacement-src");
    uint32_t pcm_hash = expected(), state = 0;
    for(unsigned channels = 2; channels <= 6; channels += 4) for(unsigned mode = 0; mode < 6; ++mode) {
        uint32_t current = run_mode(channels, mode);
        if(!state) state = current; else assert(state == current);
    }
    test_guards_and_graph_failure();
    printf("PASS PCM bridge: exact SDL2/6-channel bytes, real queue backpressure/recovery, detach/reattach, output errors and graph guards; expected_pcm_hash=%u synthesis_hash=%u\n", pcm_hash, state);
    if(replacement_src) printf("PASS PCM replacement provider: %s; mode0 bank2, twelve SDL output cases\n", PARTYBOARD_SRC_COEFFICIENT_PROVIDER);
    if(argc == 2 && !strcmp(argv[1], "--device")) {
        if(device_probe(2) || device_probe(6)) return 1;
    }
    return 0;
}
