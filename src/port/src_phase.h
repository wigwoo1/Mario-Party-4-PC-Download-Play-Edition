#ifndef PARTYBOARD_SRC_PHASE_H
#define PARTYBOARD_SRC_PHASE_H
#include <stdint.h>

/* Original hardware ratio is unsigned16.16 (hwSetPitch stores speed*16).
 * Keep fractional phase across output chunks and live pitch changes.
 * Caller advances decoder by the returned whole-source-sample count before
 * applying the selected original filter at the new fractional phase. */
typedef struct { uint32_t fraction, ratio; } PartyboardSrcPhase;

static inline uint32_t PartyboardSrcPhaseAdvance(PartyboardSrcPhase *phase) {
    uint64_t position = (uint64_t)phase->fraction + phase->ratio;
    phase->fraction = (uint32_t)(position & 65535);
    return (uint32_t)(position >> 16);
}
#endif
