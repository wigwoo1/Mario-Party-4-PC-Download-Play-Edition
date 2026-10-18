#ifndef PARTYBOARD_MUSYX_SAMPLE_SOURCE_H
#define PARTYBOARD_MUSYX_SAMPLE_SOURCE_H
#include "musyx/synthdata.h"
#include "musyx/stream.h"
#include "msm_sample.h"

/* Host-endian, original macro-resolved SAMPLE_INFO. Encoded sample memory
 * stays caller-owned; bytes must be the actual available payload extent.
 * Group conversion must provide host-endian SNDADPCMinfo before this call. */
static inline int PartyboardMusyxSampleSourceOpen(PartyboardMsmSample *destination,
        const SAMPLE_INFO *info, size_t bytes) {
    PartyboardMsmSample result = {0};
    result.frequency = info->info & 0xffffff;
    result.root_key = info->info >> 24;
    result.format = info->compType;
    if (!result.frequency || !info->addr) return 0;
    if (result.format == 0) {
        PartyboardDspAdpcm decoder = {0};
        const SNDADPCMinfo *parameters = (const SNDADPCMinfo *)info->extraData;
        unsigned i;
        if (!parameters) return 0;
        for (i = 0; i < 8; ++i) {
            decoder.coefficients[i][0] = parameters->coefTab[i][0];
            decoder.coefficients[i][1] = parameters->coefTab[i][1];
        }
        if (!PartyboardDspAdpcmSampleInit(&result.source.dsp, &decoder,
                (const uint8_t *)info->addr, bytes, info->length, parameters->initialPS,
                info->loop, info->loopLength, parameters->loopPS,
                parameters->loopY1, parameters->loopY0)) return 0;
    } else if (result.format == 2) {
        if (!PartyboardPcm16SampleInit(&result.source.pcm, (const uint8_t *)info->addr,
                bytes, info->length, info->offset, info->loop, info->loopLength)) return 0;
    } else return 0;
    *destination = result;
    return 1;
}
#endif
