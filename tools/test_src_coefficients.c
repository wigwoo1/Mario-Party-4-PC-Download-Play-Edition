#include <assert.h>
#include <stdio.h>
#include "../src/port/src_coefficients.h"
#include "../src/port/src_resampler.h"

typedef struct { uint64_t position; } Source;
static int16_t sample(uint64_t index) {
    /* Bounded integer saw with a different period from the 128 phases. */
    return (int16_t)((int)((index * 7919u + 12345u) % 60001u) - 30000);
}
static int read_sample(void *context, int16_t *out) {
    Source *source = context; *out = sample(source->position++); return 1;
}
static uint32_t hash_word(uint32_t hash, int16_t word) {
    hash = (hash ^ ((uint16_t)word & 255u)) * 16777619u;
    return (hash ^ ((uint16_t)word >> 8)) * 16777619u;
}
/* Absolute source index and a scalar dot product, independent of the runtime's
 * history shifts/phase advance. Also exercises negative rounding and clipping. */
static int16_t reference(uint64_t consumed, uint32_t fraction, const int16_t *bank) {
    int64_t sum = 0;
    for(unsigned tap = 0; tap < 4; ++tap) {
        int16_t value = consumed + tap < 4 ? 0 : sample(consumed + tap - 4);
        sum += (int64_t)value * bank[(fraction >> 9) * 4 + tap];
    }
    /* C truncates division toward zero; explicitly implement arithmetic floor. */
    int64_t value = sum / 32768;
    if(sum < 0 && sum % 32768) --value;
    if(value < -32768) value = -32768;
    if(value > 32767) value = 32767;
    return (int16_t)value;
}
int main(void) {
    static const uint32_t ratios[] = {1, 16384, 32768, 65536, 98304, 131072, 196608, 262128};
    const int16_t *banks; size_t words = 0; uint32_t hash = 2166136261u, cases = 0;
    banks = PartyboardSrcDefaultCoefficients(&words);
    assert(words == 1536 && banks == PartyboardSrcDefaultCoefficients(NULL));
    for(unsigned bank = 0; bank < 3; ++bank) {
        const int16_t *coefs = banks + bank * 512;
        for(unsigned phase = 0; phase < 128; ++phase) {
            int total = 0;
            for(unsigned tap = 0; tap < 4; ++tap) {
                total += coefs[phase * 4 + tap]; hash = hash_word(hash, coefs[phase * 4 + tap]);
            }
            assert(total == 32767);
            for(int extreme = -32768; extreme <= 32767; extreme += 65535) {
                int16_t history[4] = {(int16_t)extreme, (int16_t)extreme, (int16_t)extreme, (int16_t)extreme}, out;
                assert(PartyboardSrcFilter(history, phase * 512, coefs, 512, &out));
                assert(out == (extreme < 0 ? -32767 : 32766));
            }
        }
        for(unsigned rate = 0; rate < sizeof ratios / sizeof ratios[0]; ++rate) {
            Source source = {0}; PartyboardSrcResampler renderer;
            int16_t output[4096], whole[4096]; uint64_t position = 0;
            assert(PartyboardSrcResamplerInit(&renderer, ratios[rate], coefs, 512, read_sample, &source));
            assert(PartyboardSrcResamplerRead(&renderer, whole, 4096) == 4096);
            source.position = 0;
            assert(PartyboardSrcResamplerInit(&renderer, ratios[rate], coefs, 512, read_sample, &source));
            for(unsigned start = 0; start < 4096;) {
                size_t count = (start % 37) + 1;
                if(count > 4096 - start) count = 4096 - start;
                assert(PartyboardSrcResamplerRead(&renderer, output + start, count) == count); start += (unsigned)count;
            }
            for(unsigned frame = 0; frame < 4096; ++frame) {
                position += ratios[rate];
                assert(output[frame] == whole[frame]);
                assert(output[frame] == reference(position >> 16, (uint32_t)position & 65535, coefs));
                hash = hash_word(hash, output[frame]);
            }
            assert(source.position == position >> 16 && renderer.phase.fraction == (position & 65535));
            ++cases;
        }
    }
    printf("PASS replacement SRC: provider=%s banks=3 phases=128 cases=%u frames=98304 hash=%u\n",
           PARTYBOARD_SRC_COEFFICIENT_PROVIDER, cases, hash);
    return 0;
}
