#ifndef PARTYBOARD_MUSYX_PCM_ENVELOPE_H
#define PARTYBOARD_MUSYX_PCM_ENVELOPE_H
#include <stddef.h>
#include <stdint.h>
#include "musyx/adsr.h"
#include "musyx/synth.h"

typedef struct {
    ADSR_VARS adsr;
    int32_t volume, delta;
    unsigned remaining;
    int done_after_block, finished;
} PartyboardMusyxPcmEnvelope;

/* Original DSP startup runs adsrSetup on the voice's configured envelope.
 * Keep a private copy: rendering must not mutate macro configuration. */
static inline int PartyboardMusyxPcmEnvelopeStart(PartyboardMusyxPcmEnvelope *destination,
        const ADSR_VARS *configured) {
    PartyboardMusyxPcmEnvelope result = {0};
    if (!destination || !configured) return 0;
    result.adsr = *configured;
    if (adsrSetup(&result.adsr)) return 0;
    *destination = result;
    return 1;
}

/* Schedule independently of decoder output. A pending partial period must
 * finish before another original ADSR control step can run. */
static inline int PartyboardMusyxPcmEnvelopeBeginPeriod(PartyboardMusyxPcmEnvelope *state) {
    u16 start, delta;
    if (!state || state->remaining || state->finished) return 0;
    if (state->done_after_block) { state->finished = 1; return 0; }
    state->done_after_block = adsrHandle(&state->adsr, &start, &delta) != 0;
    state->volume = start;
    state->delta = (int16_t)delta;
    state->remaining = 32;
    return 1;
}

/* Preserve original one-millisecond/32-sample envelope control periods.
 * Completion applies after the final returned control block is rendered.
 * Source is already at the original32kHz output rate before this stage. */
static inline size_t PartyboardMusyxPcmEnvelopeApply(PartyboardMusyxPcmEnvelope *state,
        int16_t *pcm, size_t count) {
    size_t processed = 0;
    while (processed < count && !state->finished) {
        int64_t value;
        if (!state->remaining) {
            if (!PartyboardMusyxPcmEnvelopeBeginPeriod(state)) break;
        }
        if (state->volume < 0) state->volume = 0;
        if (state->volume > 32767) state->volume = 32767;
        value = (int64_t)pcm[processed] * state->volume;
        pcm[processed++] = (int16_t)(value >= 0 ? value / 32768 : -((-value + 32767) / 32768));
        state->volume += state->delta;
        --state->remaining;
        if (!state->remaining && state->done_after_block) state->finished = 1;
    }
    return processed;
}
#endif
