#ifndef PARTYBOARD_SRC_RESAMPLER_H
#define PARTYBOARD_SRC_RESAMPLER_H
#include <string.h>
#include "src_phase.h"
#include "src_filter.h"

/* Read one decoded source sample. Zero signals finite source exhaustion.
 * Caller owns verified coefficients and decoder/loop state for this lifetime.
 * This adapter implements the polyphase mode; other SRC modes are separate. */
typedef int (*PartyboardSrcReadSample)(void *context,int16_t *sample);
typedef int (*PartyboardSrcAtEnd)(void *context);
typedef struct {
    PartyboardSrcPhase phase;
    int16_t history[4];
    const int16_t *coefficients;
    size_t coefficient_count;
    PartyboardSrcReadSample read;
    PartyboardSrcAtEnd at_end;
    void *context;
    int ended;
    unsigned mode;
} PartyboardSrcResampler;

static inline int PartyboardSrcResamplerInit(PartyboardSrcResampler *destination,
        uint32_t ratio,const int16_t *coefficients,size_t coefficient_count,
        PartyboardSrcReadSample read,void *context) {
    PartyboardSrcResampler result={0};
    if(!destination || !coefficients || coefficient_count<512 || !read
            || ratio>0x3fffU*16U) return 0;
    result.phase.ratio=ratio;
    result.coefficients=coefficients;result.coefficient_count=coefficient_count;
    result.read=read;result.context=context;
    *destination=result;
    return 1;
}

static inline size_t PartyboardSrcResamplerReadBypassPeriod(PartyboardSrcResampler *source,
        int16_t *output,size_t count);
static inline size_t PartyboardSrcResamplerReadMode(PartyboardSrcResampler *source,
        int16_t *output,size_t count,int stopped_input) {
    size_t produced=0;
    if(source && source->mode==2)
        return PartyboardSrcResamplerReadBypassPeriod(source,output,count);
    if(source && source->mode!=0) return 0;
    if(!source || !output || !source->read || !source->coefficients
            || source->coefficient_count<512 || source->phase.fraction>65535
            || source->phase.ratio>0x3fffU*16U || source->ended) return 0;
    while(produced<count) {
        uint32_t advance=PartyboardSrcPhaseAdvance(&source->phase);
        while(advance--) {
            int16_t sample=0;
            if(source->ended || !source->read(source->context,&sample)) {
                source->ended=1;
                if(!stopped_input) return produced;
                sample=0;
            }
            memmove(source->history,source->history+1,3*sizeof(int16_t));
            source->history[3]=sample;
        }
        if(!PartyboardSrcFilter(source->history,source->phase.fraction,
                source->coefficients,source->coefficient_count,output+produced)) return produced;
        ++produced;
    }
    /* The accelerator signals end on the last valid read. A period that
     * consumes exactly the finite source must not start another period. */
    if(stopped_input && source->at_end && source->at_end(source->context))
        source->ended=1;
    return produced;
}
static inline size_t PartyboardSrcResamplerRead(PartyboardSrcResampler *source,
        int16_t *output,size_t count) {
    return PartyboardSrcResamplerReadMode(source,output,count,0);
}
/* Accelerator stopped input is zero, but surviving filter history can still
 * produce output. Bound this experimental path to the current32-frame period;
 * callers must honor the original PB state gate before another period. */
static inline size_t PartyboardSrcResamplerReadStoppedPeriod(PartyboardSrcResampler *source,
        int16_t *output,size_t count) {
    if(count>32) return 0;
    return PartyboardSrcResamplerReadMode(source,output,count,1);
}
/* Selected original mode2 copies source samples directly, independent of
 * pitch and coefficient banks, then stores trailing history and zero phase.
 * Stopped accelerator reads supply zero for the rest of this period only. */
static inline size_t PartyboardSrcResamplerReadBypassPeriod(PartyboardSrcResampler *source,
        int16_t *output,size_t count) {
    size_t produced;
    if(!source || !output || !source->read || source->ended || count>32) return 0;
    for(produced=0;produced<count;++produced) {
        int16_t sample=0;
        if(source->ended || !source->read(source->context,&sample)) {
            source->ended=1;sample=0;
        }
        output[produced]=sample;
        memmove(source->history,source->history+1,3*sizeof(int16_t));
        source->history[3]=sample;
    }
    if(count) source->phase.fraction=0;
    if(count && source->at_end && source->at_end(source->context)) source->ended=1;
    return count;
}
#endif
