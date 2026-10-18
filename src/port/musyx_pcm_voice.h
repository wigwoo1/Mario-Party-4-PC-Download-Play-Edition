#ifndef PARTYBOARD_MUSYX_PCM_VOICE_H
#define PARTYBOARD_MUSYX_PCM_VOICE_H
#include "src_resampler.h"
#include "musyx_pcm_envelope.h"
#include "musyx_pcm_gain.h"
#include "musyx_pcm_controls.h"
#include "musyx_pcm_ramp.h"
/* Render one voice into the original nine unclipped mixer buses. Caller
 * retains source/envelope lifetime and performs final studio/aux processing.
 * Bound decoding at envelope periods to avoid consuming samples after release.
 */
static inline size_t PartyboardMusyxPcmVoiceRender(PartyboardSrcResampler *source,
        PartyboardMusyxPcmEnvelope *envelope,const SAL_VOLINFO *volume,
        int64_t *bus,size_t frames) {
    int16_t gains[9],pcm[32];
    size_t rendered=0;
    if(!source || !envelope || !volume || !bus) return 0;
    PartyboardMusyxPcmGain(volume,gains);
    while(rendered<frames && !envelope->finished && !source->ended) {
        size_t request=frames-rendered,decoded,processed;
        unsigned period=envelope->remaining ? envelope->remaining : 32;
        if(request>period) request=period;
        decoded=PartyboardSrcResamplerRead(source,pcm,request);
        if(!decoded) break;
        processed=PartyboardMusyxPcmEnvelopeApply(envelope,pcm,decoded);
        PartyboardPcmMixAdd(bus+rendered*9,pcm,processed,gains,9);
        rendered+=processed;
        if(processed!=decoded || decoded!=request) break;
    }
    return rendered;
}
/* A caller schedules exactly five periods per 160-frame DSP block. Preserve
 * original changed[] ownership; this handles pitch/key-off only, with other
 * events and gain ramps still belonging to the production scheduler. */
static inline size_t PartyboardMusyxPcmVoiceRenderPeriod(PartyboardSrcResampler *source,
        PartyboardMusyxPcmEnvelope *envelope,const DSPvoice *voice,unsigned offset,
        const SAL_VOLINFO *volume,int64_t *bus) {
    if(!source || !volume || !bus || !voice || offset>=5
            || ((voice->changed[offset]&8) && voice->pitch[offset]>0x3fffU*16U)) return 0;
    if(!PartyboardMusyxPcmControlsApply(envelope,&source->phase,voice,offset)) return 0;
    return PartyboardMusyxPcmVoiceRender(source,envelope,volume,bus,32);
}
/* Caller supplies actual PB gain/delta state and this period's reset flags.
 * ROM multiply/update ordering still needs independent fidelity validation. */
static inline size_t PartyboardMusyxPcmVoiceRenderRampPrepared(PartyboardSrcResampler *source,
        PartyboardMusyxPcmEnvelope *envelope,
        PartyboardMusyxPcmRamp *ramp,uint16_t reset_flags,int64_t *bus) {
    int16_t pcm[32];
    size_t decoded,processed;
    if(!source || !envelope || !ramp || !bus || source->ended || envelope->finished
            || (envelope->remaining && envelope->remaining!=32)) return 0;
    decoded=PartyboardSrcResamplerRead(source,pcm,32);
    processed=PartyboardMusyxPcmEnvelopeApply(envelope,pcm,decoded);
    PartyboardMusyxPcmRampReset(ramp,reset_flags);
    PartyboardMusyxPcmRampMix(ramp,bus,pcm,processed);
    return processed;
}
static inline size_t PartyboardMusyxPcmVoiceRenderRampPeriod(PartyboardSrcResampler *source,
        PartyboardMusyxPcmEnvelope *envelope,const DSPvoice *voice,unsigned offset,
        PartyboardMusyxPcmRamp *ramp,uint16_t reset_flags,int64_t *bus) {
    if(!source || !envelope || !ramp || !bus || !voice || offset>=5
            || source->ended || envelope->finished
            || ((voice->changed[offset]&8) && voice->pitch[offset]>0x3fffU*16U)) return 0;
    if(!PartyboardMusyxPcmControlsApply(envelope,&source->phase,voice,offset)) return 0;
    return PartyboardMusyxPcmVoiceRenderRampPrepared(source,envelope,ramp,reset_flags,bus);
}
#endif
