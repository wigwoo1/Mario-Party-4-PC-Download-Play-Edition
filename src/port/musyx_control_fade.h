#ifndef PARTYBOARD_MUSYX_CONTROL_FADE_H
#define PARTYBOARD_MUSYX_CONTROL_FADE_H
#include <stdint.h>
/* MSM's server advances every third5ms callback, not once per video frame.
 * Keep its15ms truncation and integer gain; output queues never drive this. */
typedef struct { int32_t total, time; unsigned volume; } PartyboardMusyxControlFade;
static inline int32_t PartyboardMusyxControlPeriods(int32_t milliseconds) {
    return milliseconds>0 ? milliseconds/15 : 0;
}
static inline void PartyboardMusyxControlFadeStart(PartyboardMusyxControlFade *r,int32_t periods,int out) {
    r->total=out ? periods : -periods; r->time=out ? periods : 0; r->volume=out ? 127 : 0;
}
/* Returns1 exactly when a descending ramp reaches its target. */
static inline int PartyboardMusyxControlFadeStep(PartyboardMusyxControlFade *r) {
    if(r->total>0) {
        if(--r->time<=0) { r->total=0;r->time=0;r->volume=0;return 1; }
        r->volume=(unsigned)((int64_t)r->time*127/r->total);
    } else if(r->total<0) {
        if(++r->time>=-r->total) { r->total=0;r->volume=127; }
        else r->volume=(unsigned)((int64_t)r->time*127/-r->total);
    }
    return 0;
}
static inline unsigned PartyboardMusyxControlVolume(unsigned base,unsigned volume,unsigned fade,unsigned pause) {
    return (unsigned)((uint64_t)base*volume*fade*pause/(127u*127u*127u));
}
#endif
