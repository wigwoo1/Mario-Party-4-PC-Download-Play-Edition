#ifndef PARTYBOARD_MUSYX_PCM_GAIN_H
#define PARTYBOARD_MUSYX_PCM_GAIN_H
#include "musyx/sal.h"
#include "pcm_mix.h"

/* Original console buses: dry L/R/S, auxA L/R/S, auxB L/R/S.
 * These are mixer buses, not a claim of discrete surround device routing. */
static inline void PartyboardMusyxPcmGain(const SAL_VOLINFO *volume, int16_t gains[9]) {
    const float values[9] = {volume->volL, volume->volR, volume->volS,
        volume->volAuxAL, volume->volAuxAR, volume->volAuxAS,
        volume->volAuxBL, volume->volAuxBR, volume->volAuxBS};
    unsigned i;
    for (i = 0; i < 9; ++i) {
        float value = values[i];
        /* Match original32767*gain conversion; reject nonfinite/negative
         * contributions as silence instead of undefined float→int casts. */
        if (!(value >= 0.f)) value = 0.f;
        if (value > 1.f) value = 1.f;
        gains[i] = (int16_t)(32767.f * value);
    }
}
#endif
