#ifndef PARTYBOARD_MSM_SAMPLE_RESAMPLER_H
#define PARTYBOARD_MSM_SAMPLE_RESAMPLER_H
#include "msm_sample.h"
#include "src_resampler.h"

static inline int PartyboardMsmResamplerReadSample(void *context,int16_t *output) {
    return PartyboardMsmSampleRead((PartyboardMsmSample*)context,output,1)==1;
}
static inline int PartyboardMsmResamplerAtEnd(void *context) {
    PartyboardMsmSample *sample=(PartyboardMsmSample*)context;
    if(sample->format==0) return !sample->source.dsp.looping
        && sample->source.dsp.position>=sample->source.dsp.end;
    if(sample->format==2) return !sample->source.pcm.looping
        && sample->source.pcm.position>=sample->source.pcm.end;
    return 0;
}
/* The decoded source must remain at a stable address until playback ends.
 * Existing source readers retain format-specific loop predictor histories.
 * Caller supplies the selected original coefficient bank, never a fallback. */
static inline int PartyboardMsmSampleResamplerInit(PartyboardSrcResampler *destination,
        PartyboardMsmSample *sample,uint32_t ratio,const int16_t *coefficients,
        size_t coefficient_count) {
    if(!sample || (sample->format!=0 && sample->format!=2)) return 0;
    if(!PartyboardSrcResamplerInit(destination,ratio,coefficients,coefficient_count,
        PartyboardMsmResamplerReadSample,sample)) return 0;
    destination->at_end=PartyboardMsmResamplerAtEnd;
    return 1;
}
#endif
