#ifndef PARTYBOARD_MUSYX_DEPOP_VOICE_H
#define PARTYBOARD_MUSYX_DEPOP_VOICE_H
#include "musyx_depop.h"
#include "musyx/dspvoice.h"
#include "musyx/version.h"

static inline int16_t PartyboardMusyxDepopSigned(uint16_t word) {
    return (int16_t)(word<=32767 ? (int32_t)word : (int32_t)word-65536);
}
/* Original 2.0 HandleDepopVoice routing. Residuals must already have been
 * produced by the DSP-equivalent voice renderer; this does not invent them. */
static inline int PartyboardMusyxDepopVoice200(DSPhostDPop *sum,DSPvoice *voice) {
    _PB *pb;
    if(!sum || !voice || !voice->pb || MUSY_VERSION!=MUSY_VERSION_CHECK(2,0,0)) return 0;
    pb=voice->pb;
    voice->postBreak=0;pb->state=0;
#define PARTYBOARD_ADD_DEPOP(field,residual) sum->field=PartyboardMusyxDepopAdd(sum->field,PartyboardMusyxDepopSigned(pb->dpop.residual))
    PARTYBOARD_ADD_DEPOP(l,aL);PARTYBOARD_ADD_DEPOP(r,aR);
    if(pb->mixerCtrl&4) { PARTYBOARD_ADD_DEPOP(s,aS); }
    if(pb->mixerCtrl&1) {
        PARTYBOARD_ADD_DEPOP(lA,aAuxAL);PARTYBOARD_ADD_DEPOP(rA,aAuxAR);
        if(pb->mixerCtrl&0x14) { PARTYBOARD_ADD_DEPOP(sA,aAuxAS); }
    }
    if(pb->mixerCtrl&0x12) {
        PARTYBOARD_ADD_DEPOP(lB,aAuxBL);PARTYBOARD_ADD_DEPOP(rB,aAuxBR);
        if(pb->mixerCtrl&4) { PARTYBOARD_ADD_DEPOP(sB,aAuxBS); }
    }
#undef PARTYBOARD_ADD_DEPOP
    return 1;
}
/* Write console high/low words explicitly: the original long* store assumes
 * big-endian memory and cannot be reused on a little-endian host. */
static inline int PartyboardMusyxDepopStudio200(DSPhostDPop *sum,_SPB *pb) {
    DSPhostDPop next;
    _SPB prepared;
    int32_t start,remaining;
    int16_t delta;
    if(!sum || !pb) return 0;
    next=*sum;prepared=*pb;
#define PARTYBOARD_FADE_DEPOP(field,prefix) \
    remaining=(int32_t)next.field; \
    if(!PartyboardMusyxDepopFade(&remaining,&start,&delta)) return 0; \
    next.field=remaining; \
    prepared.prefix##Hi=(uint16_t)((uint32_t)start>>16); \
    prepared.prefix##Lo=(uint16_t)start; \
    prepared.prefix##Delta=(uint16_t)delta
    PARTYBOARD_FADE_DEPOP(l,dpopL);PARTYBOARD_FADE_DEPOP(r,dpopR);
    PARTYBOARD_FADE_DEPOP(s,dpopS);PARTYBOARD_FADE_DEPOP(lA,dpopAL);
    PARTYBOARD_FADE_DEPOP(rA,dpopAR);PARTYBOARD_FADE_DEPOP(sA,dpopAS);
    PARTYBOARD_FADE_DEPOP(lB,dpopBL);PARTYBOARD_FADE_DEPOP(rB,dpopBR);
    PARTYBOARD_FADE_DEPOP(sB,dpopBS);
#undef PARTYBOARD_FADE_DEPOP
    *sum=next;*pb=prepared;
    return 1;
}
#endif
