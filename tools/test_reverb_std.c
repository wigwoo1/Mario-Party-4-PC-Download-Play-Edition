#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "musyx/musyx.h"
#include "musyx/sal.h"

bool ReverbSTDCreate(_SND_REVSTD_WORK*, f32, f32, f32, f32, f32);
bool ReverbSTDModify(_SND_REVSTD_WORK*, f32, f32, f32, f32, f32);
void ReverbSTDCallback(s32*, s32*, s32*, _SND_REVSTD_WORK*);
void ReverbSTDFree(_SND_REVSTD_WORK*);

typedef struct { uint8_t *base; size_t bytes; } Allocation;
static Allocation allocations[64];
static int allocation_attempt, fail_at = -1;
static unsigned live;
static void guards(void) {
    for(unsigned i = 0; i < 64; ++i) if(allocations[i].base)
        for(unsigned j = 0; j < 16; ++j) {
            assert(allocations[i].base[j] == 0xa5);
            assert(allocations[i].base[16 + allocations[i].bytes + j] == 0xa5);
        }
}
static void *allocate(size_t bytes) {
    unsigned slot;
    if(allocation_attempt++ == fail_at) return NULL;
    for(slot = 0; slot < 64 && allocations[slot].base; ++slot) {}
    assert(slot < 64);
    allocations[slot].base = malloc(bytes + 32); assert(allocations[slot].base);
    allocations[slot].bytes = bytes;
    memset(allocations[slot].base, 0xa5, bytes + 32); ++live;
    return allocations[slot].base + 16;
}
static void release(void *pointer) {
    unsigned slot;
    guards();
    for(slot = 0; slot < 64; ++slot)
        if(allocations[slot].base && allocations[slot].base + 16 == pointer) break;
    assert(slot < 64); free(allocations[slot].base);
    memset(&allocations[slot], 0, sizeof allocations[slot]); --live;
}
static uint32_t read_word(FILE *input) {
    uint8_t b[4]; assert(fread(b, 1, 4, input) == 4);
    return (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
}
static uint32_t bits(f32 value) { uint32_t word; memcpy(&word, &value, 4); return word; }
static f32 as_float(uint32_t word) { f32 value; memcpy(&value, &word, 4); return value; }
static s32 as_signed(uint32_t word) {
    return word <= 0x7fffffffU ? (s32)word : (s32)((int64_t)word - 4294967296LL);
}
static void float_field(FILE *input, f32 *field, int initialize) {
    uint32_t word = read_word(input);
    if(initialize) *field = as_float(word);
    else assert(bits(*field) == word);
}
static void state(FILE *input, _SND_REVSTD_WORK *rv, int initialize) {
    float_field(input, &rv->allPassCoeff, initialize); float_field(input, &rv->level, initialize);
    float_field(input, &rv->damping, initialize);
    for(unsigned i = 0; i < 6; ++i) float_field(input, &rv->combCoef[i], initialize);
    for(unsigned i = 0; i < 3; ++i) float_field(input, &rv->lpLastout[i], initialize);
    {
        uint32_t delay = read_word(input); assert(delay <= 32);
        if(initialize) rv->preDelayTime = (s32)delay; else assert(rv->preDelayTime == (s32)delay);
    }
    for(unsigned i = 0; i < 12; ++i) {
        _SND_REVSTD_DELAYLINE *line = i < 6 ? &rv->AP[i] : &rv->C[i - 6];
        uint32_t in = read_word(input), out = read_word(input), length = read_word(input);
        assert(length && length < 128 && !(length & 3));
        if(initialize) {
            line->inPoint = (s32)in; line->outPoint = (s32)out; line->length = (s32)length;
            line->inputs = allocate(length); assert(line->inputs);
        } else assert(line->inPoint == (s32)in && line->outPoint == (s32)out && line->length == (s32)length);
        float_field(input, &line->lastOutput, initialize);
        for(unsigned n = 0; n < length / 4; ++n) float_field(input, &line->inputs[n], initialize);
    }
    for(unsigned i = 0; i < 3; ++i) {
        uint32_t position = read_word(input);
        if(initialize && rv->preDelayTime) {
            rv->preDelayLine[i] = allocate((size_t)rv->preDelayTime * 4);
            rv->preDelayPtr[i] = rv->preDelayLine[i] + position;
        } else if(!initialize) assert(position == (rv->preDelayTime ?
            (uint32_t)(rv->preDelayPtr[i] - rv->preDelayLine[i]) : 0));
        for(int n = 0; n < rv->preDelayTime; ++n) float_field(input, &rv->preDelayLine[i][n], initialize);
    }
}
static void test_assembly_vectors(const char *path) {
    FILE *input = fopen(path, "rb"); unsigned compared = 0;
    assert(input); assert(read_word(input) == 0x52565331); assert(read_word(input) == 3);
    for(unsigned example = 0; example < 3; ++example) {
        _SND_REVSTD_WORK rv = {0};
        s32 samples[4][480], expected[4][480];
        uint32_t blocks = read_word(input), words = read_word(input); long start;
        assert(blocks == 4); start = ftell(input); state(input, &rv, 1);
        assert(ftell(input) - start == (long)words * 4);
        for(unsigned b = 0; b < blocks; ++b) for(unsigned i = 0; i < 480; ++i) samples[b][i] = as_signed(read_word(input));
        for(unsigned b = 0; b < blocks; ++b) for(unsigned i = 0; i < 480; ++i) expected[b][i] = as_signed(read_word(input));
        for(unsigned b = 0; b < blocks; ++b) {
            ReverbSTDCallback(samples[b], samples[b] + 160, samples[b] + 320, &rv);
            for(unsigned i = 0; i < 480; ++i) {
                if(samples[b][i] != expected[b][i]) fprintf(stderr, "reverb mismatch case%u block%u sample%u actual%d expected%d\n", example, b, i, samples[b][i], expected[b][i]);
                assert(samples[b][i] == expected[b][i]); ++compared;
            }
            guards();
        }
        state(input, &rv, 0); ReverbSTDFree(&rv); assert(!live);
    }
    assert(fgetc(input) == EOF); fclose(input);
    assert(compared == 5760);
    puts("PASS standard reverb: 5760 assembly-reference outputs and complete persistent state match");
}
static void test_lifecycle(void) {
    for(int failure = 0; failure < 15; ++failure) {
        _SND_REVSTD_WORK rv, saved;
        memset(&rv, 0x5a, sizeof rv); saved = rv;
        allocation_attempt = 0; fail_at = failure;
        assert(!ReverbSTDCreate(&rv, .5f, 1.f, .5f, .2f, .01f));
        assert(!memcmp(&rv, &saved, sizeof rv) && !live);
    }
    fail_at = -1;
    {
        _SND_REVSTD_WORK rv = {0}, saved;
        assert(ReverbSTDCreate(&rv, .5f, 1.f, .5f, .2f, .01f)); assert(live == 15);
        saved = rv;
        for(int failure = 0; failure < 15; ++failure) {
            fail_at = allocation_attempt + failure;
            assert(!ReverbSTDModify(&rv, .3f, 2.f, .6f, .4f, .02f));
            assert(!memcmp(&rv, &saved, sizeof rv) && live == 15); fail_at = -1;
        }
        assert(!ReverbSTDModify(&rv, .5f, 1.f, .5f, .2f, 1.f));
        assert(!ReverbSTDModify(&rv, NAN, 1.f, .5f, .2f, .01f));
        assert(!memcmp(&rv, &saved, sizeof rv) && live == 15);
        assert(ReverbSTDModify(&rv, .3f, 2.f, .6f, .4f, 0)); assert(live == 12);
        ReverbSTDFree(&rv); ReverbSTDFree(&rv); assert(!live);
    }
    {
        SND_AUX_REVERBSTD rev = {0}; _SND_REVSTD_WORK saved;
        rev.coloration = rev.mix = .5f; rev.time = 1.f; rev.damping = .2f; rev.preDelay = .01f;
        assert(sndAuxCallbackPrepareReverbSTD(&rev)); saved = rev.rv;
        rev.preDelay = 1.f;
        assert(!sndAuxCallbackUpdateSettingsReverbSTD(&rev));
        assert(!rev.tempDisableFX && !memcmp(&rev.rv, &saved, sizeof saved));
        assert(sndAuxCallbackShutdownReverbSTD(&rev)); assert(!live);
    }
    puts("PASS standard reverb lifecycle: 30 allocation failures, transactional modify, invalid parameters and repeated shutdown");
}
static void test_one_sample_predelay(void) {
    _SND_REVSTD_WORK rv = {0};
    struct { s32 left[160]; uint32_t guard[8]; s32 right[160]; uint32_t guard2[8]; s32 surround[160]; } buffers;
    memset(&buffers, 0x5a, sizeof buffers);
    for(unsigned i = 0; i < 160; ++i) { buffers.left[i] = 1000; buffers.right[i] = -2000; buffers.surround[i] = 3000; }
    assert(ReverbSTDCreate(&rv, .5f, 1.f, 0, .2f, 1.f / 32000.f)); assert(rv.preDelayTime == 1);
    ReverbSTDCallback(buffers.left, buffers.right, buffers.surround, &rv);
    for(unsigned i = 0; i < 160; ++i) assert(buffers.left[i] == 600 && buffers.right[i] == -1200 && buffers.surround[i] == 1800);
    for(unsigned i = 0; i < 8; ++i) assert(buffers.guard[i] == 0x5a5a5a5a && buffers.guard2[i] == 0x5a5a5a5a);
    guards(); ReverbSTDFree(&rv); assert(!live);
    puts("PASS standard reverb: bounded one-sample predelay and separate L/R/S buffers");
}
static void configure(SND_AUX_REVERBSTD *rev) {
    rev->coloration = rev->mix = .5f; rev->time = 1.f; rev->damping = .2f; rev->preDelay = .01f;
}
static void disabled_callback(SND_AUX_REVERBSTD *rev) {
    SND_AUX_INFO info = {0}; s32 samples[480], saved[480];
    assert(rev->tempDisableFX);
    for(unsigned i = 0; i < 480; ++i) samples[i] = (s32)i - 240;
    memcpy(saved, samples, sizeof samples);
    info.data.bufferUpdate.left = samples; info.data.bufferUpdate.right = samples + 160;
    info.data.bufferUpdate.surround = samples + 320;
    sndAuxCallbackReverbSTD(SND_AUX_REASON_BUFFERUPDATE, &info, rev);
    assert(!memcmp(samples, saved, sizeof samples)); guards();
}
static void test_owner_callbacks(void) {
    for(int failure = 0; failure < 15; ++failure) {
        SND_AUX_REVERBSTD rev;
        memset(&rev, 0x5a, sizeof rev); configure(&rev);
        fail_at = allocation_attempt + failure;
        assert(!sndAuxCallbackPrepareReverbSTD(&rev)); fail_at = -1;
        assert(rev.tempDisableFX && !live); disabled_callback(&rev);
        assert(sndAuxCallbackShutdownReverbSTD(&rev)); disabled_callback(&rev);
        assert(sndAuxCallbackShutdownReverbSTD(&rev)); assert(!live);
    }
    {
        SND_AUX_REVERBSTD rev = {0}; _SND_REVSTD_WORK saved;
        configure(&rev); assert(sndAuxCallbackPrepareReverbSTD(&rev)); assert(!rev.tempDisableFX);
        rev.tempDisableFX = TRUE; saved = rev.rv; rev.preDelay = 1.f;
        assert(!sndAuxCallbackUpdateSettingsReverbSTD(&rev));
        assert(rev.tempDisableFX && !memcmp(&rev.rv, &saved, sizeof saved)); disabled_callback(&rev);
        rev.preDelay = .02f; assert(sndAuxCallbackUpdateSettingsReverbSTD(&rev));
        assert(rev.tempDisableFX && live == 15); disabled_callback(&rev);
        assert(sndAuxCallbackShutdownReverbSTD(&rev)); assert(!live); disabled_callback(&rev);
        assert(sndAuxCallbackPrepareReverbSTD(&rev)); assert(!rev.tempDisableFX && live == 15);
        {
            s32 samples[480]; SND_AUX_INFO info = {0};
            for(unsigned i = 0; i < 480; ++i) samples[i] = 1000;
            info.data.bufferUpdate.left = samples; info.data.bufferUpdate.right = samples + 160;
            info.data.bufferUpdate.surround = samples + 320;
            sndAuxCallbackReverbSTD(SND_AUX_REASON_BUFFERUPDATE, &info, &rev);
            assert(samples[0] == 300 && samples[160] == 300 && samples[320] == 300);
        }
        assert(sndAuxCallbackShutdownReverbSTD(&rev)); assert(!live); disabled_callback(&rev);
    }
    puts("PASS standard reverb owner: 15 setup failures, disabled updates, callbacks after failure/shutdown and fresh restart");
}
int main(int argc, char **argv) {
    assert(argc == 2); salHooks.malloc = allocate; salHooks.free = release;
    test_assembly_vectors(argv[1]); test_lifecycle(); test_one_sample_predelay(); test_owner_callbacks();
    return 0;
}
