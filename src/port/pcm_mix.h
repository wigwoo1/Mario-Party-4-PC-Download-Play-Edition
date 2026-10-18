#ifndef PARTYBOARD_PCM_MIX_H
#define PARTYBOARD_PCM_MIX_H
#include <stddef.h>
#include <stdint.h>

/* Accumulate before clipping so voice order cannot alter cancellation.
 * Caller supplies original resolved Q15 channel gains, not raw pan values.
 * 64-bit sums also leave room for auxiliary/studio bus contributions. */
static inline void PartyboardPcmMixAdd(int64_t *bus, const int16_t *mono,
        size_t frames, const int16_t *gains, unsigned channels) {
    size_t frame;
    unsigned channel;
    for (frame = 0; frame < frames; ++frame)
        for (channel = 0; channel < channels; ++channel)
            bus[frame * channels + channel] += (int64_t)mono[frame] * gains[channel];
}

static inline void PartyboardPcmMixFinish(const int64_t *bus, int16_t *output,
        size_t samples) {
    size_t i;
    for (i = 0; i < samples; ++i) {
        int64_t value = bus[i] >= 0 ? bus[i] / 32768 : -((-bus[i] + 32767) / 32768);
        if (value > 32767) value = 32767;
        if (value < -32768) value = -32768;
        output[i] = (int16_t)value;
    }
}
#endif
