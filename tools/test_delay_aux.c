#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../src/port/musyx_aux_runtime.h"
extern u8 salAuxFrame;

typedef struct { unsigned char *base; size_t bytes; } Allocation;
static Allocation allocations[128];
static int attempt, fail_at = -1, binding_checks;
static unsigned live;
static void guards(void) {
    for(unsigned i = 0; i < 128; ++i) if(allocations[i].base)
        for(unsigned j = 0; j < 16; ++j) {
            assert(allocations[i].base[j] == 0xa5);
            assert(allocations[i].base[16 + allocations[i].bytes + j] == 0xa5);
        }
}
static void *allocate(size_t bytes) {
    unsigned slot;
    if(attempt++ == fail_at) return NULL;
    for(slot = 0; slot < 128 && allocations[slot].base; ++slot) {}
    assert(slot < 128 && bytes <= 1024 * 1024);
    allocations[slot].base = malloc(bytes + 32); assert(allocations[slot].base);
    allocations[slot].bytes = bytes; memset(allocations[slot].base, 0xa5, bytes + 32); ++live;
    return allocations[slot].base + 16;
}
static void release(void *pointer) {
    unsigned slot;
    guards();
    for(slot = 0; slot < 128; ++slot) if(allocations[slot].base && allocations[slot].base + 16 == pointer) break;
    assert(slot < 128);
    if(binding_checks) {
        uintptr_t first = (uintptr_t)pointer, end = first + allocations[slot].bytes;
        uintptr_t a = (uintptr_t)dspStudio[0].auxAUser, b = (uintptr_t)dspStudio[0].auxBUser;
        assert(a < first || a >= end); assert(b < first || b >= end);
    }
    free(allocations[slot].base); memset(&allocations[slot], 0, sizeof allocations[slot]); --live;
}
static uint32_t word(FILE *file) {
    uint8_t raw[4]; assert(fread(raw, 1, 4, file) == 4); return PartyboardMsmU32(raw);
}
static int32_t signed_word(uint32_t bits) {
    return bits <= INT32_MAX ? (int32_t)bits : (int32_t)((int64_t)bits - 4294967296LL);
}
static int32_t input_sample(unsigned block, unsigned channel, unsigned frame) {
    unsigned index = channel * 160 + frame;
    if(index % 127 == 0) return (index + block) % 2 ? INT32_MIN : INT32_MAX;
    return (int32_t)((index * 97 + block * 1709) % 65535) - 32767;
}
static void configure(SND_AUX_DELAY *delay) {
    for(unsigned channel = 0; channel < 3; ++channel) {
        delay->delay[channel] = 150 + channel * 25;
        delay->feedback[channel] = 60; delay->output[channel] = 85;
    }
}
static void callback(SND_AUX_DELAY *delay, int32_t *samples) {
    SND_AUX_INFO info = {0};
    info.data.bufferUpdate.left = samples; info.data.bufferUpdate.right = samples + 160;
    info.data.bufferUpdate.surround = samples + 320;
    sndAuxCallbackDelay(SND_AUX_REASON_BUFFERUPDATE, &info, delay);
}
static void noop_callback(SND_AUX_DELAY *delay) {
    int32_t samples[480], before[480];
    for(unsigned i = 0; i < 480; ++i) samples[i] = (int32_t)i - 240;
    memcpy(before, samples, sizeof samples); callback(delay, samples);
    assert(!memcmp(before, samples, sizeof samples)); guards();
}
static void test_vectors(const char *path) {
    FILE *file = fopen(path, "rb"); unsigned compared = 0;
    assert(file && word(file) == 0x444c5931 && word(file) == 17);
    for(unsigned example = 0; example < 17; ++example) {
        SND_AUX_DELAY delay = {0}; int32_t samples[480]; uint32_t blocks;
        for(unsigned channel = 0; channel < 3; ++channel) delay.delay[channel] = word(file);
        for(unsigned channel = 0; channel < 3; ++channel) delay.feedback[channel] = word(file);
        for(unsigned channel = 0; channel < 3; ++channel) delay.output[channel] = word(file);
        blocks = word(file); assert(blocks == 80); assert(sndAuxCallbackPrepareDelay(&delay));
        for(unsigned block = 0; block < blocks; ++block) {
            for(unsigned channel = 0; channel < 3; ++channel) for(unsigned frame = 0; frame < 160; ++frame)
                samples[channel * 160 + frame] = input_sample(block, channel, frame);
            callback(&delay, samples);
            for(unsigned i = 0; i < 480; ++i) {
                int32_t expected = signed_word(word(file));
                if(samples[i] != expected) fprintf(stderr, "delay mismatch case%u block%u sample%u actual%d expected%d\n", example, block, i, samples[i], expected);
                assert(samples[i] == expected); ++compared;
            }
            guards();
        }
        for(unsigned channel = 0; channel < 3; ++channel) assert(delay.currentPos[channel] == word(file));
        for(unsigned channel = 0; channel < 3; ++channel) {
            const int32_t *buffer = channel == 0 ? delay.left : channel == 1 ? delay.right : delay.sur;
            for(unsigned i = 0; i < delay.currentSize[channel] * 160; ++i) assert(buffer[i] == signed_word(word(file)));
        }
        assert(sndAuxCallbackShutdownDelay(&delay)); assert(!live); noop_callback(&delay);
        assert(sndAuxCallbackShutdownDelay(&delay));
    }
    assert(compared == 652800 && fgetc(file) == EOF); fclose(file);
    puts("PASS delay: 652800 independent outputs and final payload state, all15 authored presets and word-boundary cases");
}
static void test_delay_lifecycle(void) {
    for(unsigned failure = 0; failure < 3; ++failure) {
        SND_AUX_DELAY delay; memset(&delay, 0x5a, sizeof delay); configure(&delay);
        fail_at = attempt + (int)failure;
        assert(!sndAuxCallbackPrepareDelay(&delay)); fail_at = -1;
        assert(!live && !delay.left && !delay.right && !delay.sur); noop_callback(&delay);
        assert(sndAuxCallbackShutdownDelay(&delay)); assert(sndAuxCallbackShutdownDelay(&delay));
    }
    {
        SND_AUX_DELAY delay = {0}, saved;
        configure(&delay); assert(sndAuxCallbackPrepareDelay(&delay)); assert(live == 3);
        delay.delay[0] = 250;
        for(unsigned failure = 0; failure < 3; ++failure) {
            saved = delay; fail_at = attempt + (int)failure;
            assert(!sndAuxCallbackUpdateSettingsDelay(&delay)); fail_at = -1;
            assert(!memcmp(&delay, &saved, sizeof delay) && live == 3);
        }
        delay.delay[0] = 5; saved = delay; assert(!sndAuxCallbackUpdateSettingsDelay(&delay)); assert(!memcmp(&delay, &saved, sizeof delay));
        delay.delay[0] = UINT32_MAX; assert(!sndAuxCallbackUpdateSettingsDelay(&delay));
        delay.delay[0] = 150; delay.feedback[1] = 101; assert(!sndAuxCallbackUpdateSettingsDelay(&delay));
        delay.feedback[1] = 60; delay.output[2] = 101; assert(!sndAuxCallbackUpdateSettingsDelay(&delay));
        delay.output[2] = 85; assert(sndAuxCallbackUpdateSettingsDelay(&delay)); assert(live == 3);
        delay.currentPos[1] = delay.currentSize[1]; noop_callback(&delay); delay.currentPos[1] = 0;
        assert(sndAuxCallbackShutdownDelay(&delay)); assert(!live); noop_callback(&delay);
        assert(sndAuxCallbackPrepareDelay(&delay)); assert(live == 3);
        assert(sndAuxCallbackShutdownDelay(&delay)); assert(!live);
    }
    {
        SND_AUX_DELAY delay = {0}; SND_AUX_INFO info = {0};
        struct { int32_t left[160]; uint32_t guard[8]; int32_t right[160]; uint32_t guard2[8]; int32_t surround[160]; } buffers;
        memset(&buffers, 0x5a, sizeof buffers);
        delay.delay[0] = 6; delay.delay[1] = 10; delay.delay[2] = 11;
        for(unsigned channel = 0; channel < 3; ++channel) { delay.feedback[channel] = 50; delay.output[channel] = 100; }
        assert(sndAuxCallbackPrepareDelay(&delay));
        assert(delay.currentSize[0] == 1 && delay.currentSize[1] == 1 && delay.currentSize[2] == 2);
        info.data.bufferUpdate.left = buffers.left; info.data.bufferUpdate.right = buffers.right;
        info.data.bufferUpdate.surround = buffers.surround;
        for(unsigned block = 0; block < 5; ++block) {
            for(unsigned frame = 0; frame < 160; ++frame) {
                buffers.left[frame] = block ? 0 : 1000; buffers.right[frame] = block ? 0 : -2000;
                buffers.surround[frame] = block ? 0 : 3000;
            }
            sndAuxCallbackDelay(SND_AUX_REASON_BUFFERUPDATE, &info, &delay);
            assert(buffers.left[0] == (block ? 1000 >> (block - 1) : 0));
            assert(buffers.right[0] == (block ? -(2000 >> (block - 1)) : 0));
            assert(buffers.surround[0] == (block == 2 ? 3000 : block == 4 ? 1500 : 0));
            for(unsigned i = 0; i < 8; ++i) assert(buffers.guard[i] == 0x5a5a5a5a && buffers.guard2[i] == 0x5a5a5a5a);
        }
        assert(sndAuxCallbackShutdownDelay(&delay)); assert(!live);
    }
    puts("PASS delay lifecycle: six allocation failures, transactional update, invalid settings and safe callbacks after shutdown/restart");
}
static int32_t aux_buffers[2][3][480];
static void foreign_callback(u8 reason, SND_AUX_INFO *info, void *user) { (void)reason; (void)info; (void)user; }
static void test_aux_ownership(const char *path) {
    uint8_t metadata[640], changed[640]; FILE *file = fopen(path, "rb");
    PartyboardMsmContainer container = {0}; PartyboardMusyxAuxRuntime runtime = {0}, saved;
    assert(file && fread(metadata, 1, sizeof metadata, file) == sizeof metadata && fgetc(file) == EOF); fclose(file);
    container.data = metadata; container.bytes = sizeof metadata;
    container.section[PARTYBOARD_MSM_AUX].bytes = sizeof metadata;
    memset(dspStudio, 0, sizeof dspStudio); memset(aux_buffers, 0, sizeof aux_buffers);
    salMaxStudioNum = 1; dspStudio[0].state = 1; dspStudio[0].type = SND_STUDIO_TYPE_STD;
    for(unsigned slot = 0; slot < 3; ++slot) { dspStudio[0].auxA[slot] = aux_buffers[0][slot]; dspStudio[0].auxB[slot] = aux_buffers[1][slot]; }
    sndActive = 0; assert(!PartyboardMusyxAuxRuntimeInit(&runtime, 0)); sndActive = 1;
    dspStudio[0].auxB[2] = NULL; assert(!PartyboardMusyxAuxRuntimeInit(&runtime, 0));
    dspStudio[0].auxB[2] = aux_buffers[1][2];
    assert(!PartyboardMusyxAuxRuntimeInit(&runtime, 1)); assert(PartyboardMusyxAuxRuntimeInit(&runtime, 0));
    assert(!PartyboardMusyxAuxRuntimeInit(&runtime, 0)); binding_checks = 1;
    for(int preset = 1; preset < 16; ++preset) {
        assert(PartyboardMusyxAuxRuntimeSelect(&runtime, &container, 0, preset));
        assert(live == 26 && runtime.selected[0] == 0 && runtime.selected[1] == preset);
        assert(dspStudio[0].auxAUser == PartyboardMusyxAuxUser(runtime.instance[0]));
        assert(dspStudio[0].auxBUser == PartyboardMusyxAuxUser(runtime.instance[1]));
        assert(synthAuxAMIDI[0] == 0xff && synthAuxBMIDI[0] == 0xff);
        assert(runtime.instance[0]->work.high.mix == .8f);
        for(unsigned channel = 0; channel < 3; ++channel) {
            assert(runtime.instance[1]->work.delay.delay[channel] == PartyboardMsmU32(metadata + preset * 40 + 4 + channel * 4));
            for(unsigned frame = 0; frame < 160; ++frame) {
                aux_buffers[0][2][channel * 160 + frame] = 1000;
                aux_buffers[1][2][channel * 160 + frame] = 2000;
            }
        }
        salAuxFrame = 0; salHandleAuxProcessing();
        assert(aux_buffers[0][2][0] == 120 && aux_buffers[1][2][0] == 0);
        for(unsigned channel = 0; channel < 3; ++channel)
            assert(runtime.instance[1]->work.delay.currentPos[channel] == 1 % runtime.instance[1]->work.delay.currentSize[channel]);
        guards();
    }
    saved = runtime;
    for(int failure = 0; failure < 26; ++failure) {
        SND_AUX_REVERBHI old_high = runtime.instance[0]->work.high;
        SND_AUX_DELAY old_delay = runtime.instance[1]->work.delay;
        fail_at = attempt + failure;
        assert(!PartyboardMusyxAuxRuntimeSelect(&runtime, &container, 0, 1)); fail_at = -1;
        assert(!memcmp(&runtime, &saved, sizeof runtime) && live == 26 && PartyboardMusyxAuxOwnsBindings(&runtime));
        assert(!memcmp(&old_high, &runtime.instance[0]->work.high, sizeof old_high));
        assert(!memcmp(&old_delay, &runtime.instance[1]->work.delay, sizeof old_delay));
    }
    assert(!PartyboardMusyxAuxRuntimeSelect(&runtime, &container, 0, 16));
    assert(!PartyboardMusyxAuxRuntimeSelect(&runtime, &container, -3, -1)); assert(live == 26);
    memcpy(changed, metadata, sizeof changed); changed[0] = 2; container.data = changed;
    assert(!PartyboardMusyxAuxRuntimeSelect(&runtime, &container, 0, 1)); assert(live == 26);
    memcpy(changed, metadata, sizeof changed); changed[8] = 0x7f; changed[9] = 0xc0; changed[10] = changed[11] = 0;
    assert(!PartyboardMusyxAuxRuntimeSelect(&runtime, &container, 0, 1)); assert(live == 26); container.data = metadata;
    assert(PartyboardMusyxAuxRuntimeSelect(&runtime, &container, -1, -1));
    assert(runtime.selected[1] == saved.selected[1] && runtime.instance[0] != saved.instance[0] && runtime.instance[1] != saved.instance[1]);
    assert(!runtime.instance[1]->work.delay.currentPos[0]);
    dspStudio[0].auxAHandler = foreign_callback;
    assert(!PartyboardMusyxAuxRuntimeSelect(&runtime, &container, 0, 1));
    assert(!PartyboardMusyxAuxRuntimeShutdown(&runtime) && live == 26);
    dspStudio[0].auxAHandler = sndAuxCallbackReverbHI;
    assert(PartyboardMusyxAuxRuntimeShutdown(&runtime)); assert(!live && !dspStudio[0].auxAHandler && !dspStudio[0].auxBHandler);
    assert(PartyboardMusyxAuxRuntimeShutdown(&runtime));
    assert(PartyboardMusyxAuxRuntimeInit(&runtime, 0));
    memcpy(changed, metadata, sizeof changed); changed[0] = 1; container.data = changed;
    assert(PartyboardMusyxAuxRuntimeSelect(&runtime, &container, 0, -2)); assert(live == 16);
    assert(dspStudio[0].auxAHandler == sndAuxCallbackReverbSTD && !dspStudio[0].auxBHandler);
    assert(PartyboardMusyxAuxRuntimeSelect(&runtime, NULL, -2, -2)); assert(!live);
    assert(PartyboardMusyxAuxRuntimeShutdown(&runtime)); binding_checks = 0; sndActive = 0;
    puts("PASS aux ownership: all16 authored presets, 26 pair-allocation failures preserve live callbacks, stable users, retained/disabled selections and foreign-owner rejection");
}
int main(int argc, char **argv) {
    assert(argc == 3); salHooks.malloc = allocate; salHooks.free = release;
    test_vectors(argv[1]); test_delay_lifecycle(); test_aux_ownership(argv[2]);
    assert(!live); return 0;
}
