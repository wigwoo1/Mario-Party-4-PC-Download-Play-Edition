#ifndef PARTYBOARD_MUSYX_PCM_CONTROLS_H
#define PARTYBOARD_MUSYX_PCM_CONTROLS_H
#include "musyx/dspvoice.h"
#include "musyx_pcm_envelope.h"
#include "src_phase.h"

/* Apply original scheduled key-off and pitch changes at a32-sample boundary.
 * Caller owns changed flags and other events (startup, break, SRC, volumes).
 * Reject a partial envelope block instead of shifting an event in time. */
static inline int PartyboardMusyxPcmControlsApply(
        PartyboardMusyxPcmEnvelope *envelope, PartyboardSrcPhase *phase,
        const DSPvoice *voice, unsigned offset) {
    uint32_t changes;
    if (!envelope || !phase || !voice || offset >= 5 || envelope->remaining)
        return 0;
    changes = voice->changed[offset];
    if (changes & 0x40) adsrRelease(&envelope->adsr);
    if (changes & 8) phase->ratio = voice->pitch[offset];
    return 1;
}
/* Original scheduler treats the first processed period separately. Later
 * break events start a ten-period release; postBreak suppresses keyoff/pitch.
 * Caller must check previous envelope completion before scheduling a period. */
static inline int PartyboardMusyxPcmControlsScheduled(
        PartyboardMusyxPcmEnvelope *envelope,PartyboardSrcPhase *phase,
        DSPvoice *voice,unsigned offset,int first_period) {
    uint32_t changes;
    int apply_regular;
    if(!envelope || !phase || !voice || offset>=5 || envelope->remaining) return 0;
    changes=voice->changed[offset];
    apply_regular=first_period || (!(changes&0x20) && !voice->postBreak);
    if(apply_regular && (changes&8) && voice->pitch[offset]>0x3fffU*16U) return 0;
    if(!first_period && (changes&0x20)) {
        adsrStartRelease(&envelope->adsr,10);
        voice->postBreak=1;
    } else if(apply_regular) {
        if(changes&0x40) adsrRelease(&envelope->adsr);
        if(changes&8) {
            phase->ratio=voice->pitch[offset];
            voice->playInfo.pitch=voice->pitch[offset];
        }
    }
    return 1;
}
#endif
