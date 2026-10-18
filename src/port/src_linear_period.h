#ifndef PARTYBOARD_SRC_LINEAR_PERIOD_H
#define PARTYBOARD_SRC_LINEAR_PERIOD_H
#include "src_resampler.h"
#include "dsp_product_rounding.h"
/* Caller supplies original DMEM e42/e43 at source-call entry. This keeps
 * low-consumption four-word history writeback faithful to shared scratch.
 * Experimental numerical routine; full DSP exception/pipeline not verified. */
static inline size_t PartyboardSrcReadLinearPeriod(PartyboardSrcResampler *source,
        int16_t output[32],uint16_t scratch_e42,uint16_t scratch_e43) {
    int16_t samples[132];
    uint32_t position,total;
    unsigned i;
    if(!source || !output || !source->read || source->ended
            || source->phase.fraction>65535 || source->phase.ratio>0x3fffU*16U) return 0;
    samples[0]=(int16_t)scratch_e42;samples[1]=(int16_t)scratch_e43;
    samples[2]=source->history[2];samples[3]=source->history[3];
    total=(source->phase.fraction+32*source->phase.ratio)>>16;
    for(i=0;i<total;++i) {
        int16_t sample=0;
        if(source->ended || !source->read(source->context,&sample)) { source->ended=1;sample=0; }
        samples[4+i]=sample;
    }
    position=source->phase.fraction;
    for(i=0;i<32;++i) {
        uint32_t whole;
        position+=source->phase.ratio;whole=position>>16;
        output[i]=PartyboardDspInterpolateOperands(samples[2+whole],samples[3+whole],
            (uint16_t)position);
    }
    for(i=0;i<4;++i) source->history[i]=samples[total+i];
    source->phase.fraction=position&65535;
    if(source->at_end && source->at_end(source->context)) source->ended=1;
    return 32;
}
#endif
