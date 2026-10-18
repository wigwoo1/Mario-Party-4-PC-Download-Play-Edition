#ifndef PARTYBOARD_DSP_ADPCM_H
#define PARTYBOARD_DSP_ADPCM_H

#include <stdint.h>

typedef struct {
    int16_t coefficients[8][2];
    int16_t previous;
    int16_t previous2;
} PartyboardDspAdpcm;

static inline int PartyboardDspAdpcmNibble(PartyboardDspAdpcm *state,
        uint8_t ps, unsigned bits, int16_t *output) {
    unsigned predictor = ps >> 4;
    int32_t nibble = (int32_t)(bits & 15);
    int64_t total, sample;
    if (predictor >= 8) return 0;
    if (nibble >= 8) nibble -= 16;
    total = (int64_t)nibble * ((int64_t)1 << (ps & 15)) * 2048 + 1024
          + (int64_t)state->previous * state->coefficients[predictor][0]
          + (int64_t)state->previous2 * state->coefficients[predictor][1];
    sample = total >= 0 ? total / 2048 : -((-total + 2047) / 2048);
    if (sample > 32767) sample = 32767;
    if (sample < -32768) sample = -32768;
    state->previous2 = state->previous;
    state->previous = *output = (int16_t)sample;
    return 1;
}

/* One complete GameCube DSP frame: predictor/scale byte, fourteen signed
 * nibbles. Caller owns stream position and initial/loop history snapshots.
 * Returns zero for an invalid predictor without changing state or output. */
static inline int PartyboardDspAdpcmFrame(PartyboardDspAdpcm *state,
                                        const uint8_t frame[8], int16_t output[14]) {
    unsigned predictor = frame[0] >> 4;
    unsigned i;
    if (predictor >= 8) return 0;
    for (i = 0; i < 14; ++i) {
        unsigned byte = frame[1 + i / 2];
        PartyboardDspAdpcmNibble(state, frame[0], (i & 1) ? (byte & 15) : (byte >> 4), &output[i]);
    }
    return 1;
}

#endif
