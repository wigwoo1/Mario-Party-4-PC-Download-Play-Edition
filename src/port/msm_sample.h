#ifndef PARTYBOARD_MSM_SAMPLE_H
#define PARTYBOARD_MSM_SAMPLE_H
#include "msm_dsp_sample.h"
#include "pcm16_sample.h"

typedef struct {
    unsigned format, frequency, root_key;
    union {
        PartyboardDspAdpcmSample dsp;
        PartyboardPcm16Sample pcm;
    } source;
} PartyboardMsmSample;

/* Macro-selected sample ID; caller retains group/sample memory. Original
 * format0 ignores playback offset; format2 applies it in sample units. */
static inline int PartyboardMsmSampleOpen(PartyboardMsmSample *destination,
        const uint8_t *group, size_t group_bytes, const uint8_t *samples,
        size_t sample_bytes, unsigned id, size_t playback_offset) {
    PartyboardMsmSample result = {0};
    size_t cursor;
    if (!group || !samples || group_bytes < 16 || id >= 65535) return 0;
    cursor = PartyboardMsmU32(group + 8);
    if (cursor > group_bytes) return 0;
    while (group_bytes - cursor >= 2) {
        const uint8_t *entry = group + cursor;
        unsigned entry_id = PartyboardMsmU16(entry);
        if (entry_id == 65535 || group_bytes - cursor < 32) return 0;
        if (entry_id == id) {
            uint32_t info = PartyboardMsmU32(entry + 12);
            uint32_t packed_length = PartyboardMsmU32(entry + 16);
            result.format = packed_length >> 24;
            result.frequency = info & 0xffffff;
            result.root_key = info >> 24;
            if (!result.frequency) return 0;
            if (result.format == 0) {
                if (!PartyboardMsmDspSampleOpen(&result.source.dsp, group, group_bytes,
                        samples, sample_bytes, id)) return 0;
            } else if (result.format == 2) {
                size_t offset = PartyboardMsmU32(entry + 4);
                if (offset > sample_bytes || !PartyboardPcm16SampleInit(&result.source.pcm,
                        samples + offset, sample_bytes - offset, packed_length & 0xffffff,
                        playback_offset, PartyboardMsmU32(entry + 20),
                        PartyboardMsmU32(entry + 24))) return 0;
            } else return 0;
            *destination = result;
            return 1;
        }
        cursor += 32;
    }
    return 0;
}

static inline size_t PartyboardMsmSampleRead(PartyboardMsmSample *sample,
        int16_t *output, size_t count) {
    if (sample->format == 0) return PartyboardDspAdpcmSampleRead(&sample->source.dsp, output, count);
    if (sample->format == 2) return PartyboardPcm16SampleRead(&sample->source.pcm, output, count);
    return 0;
}
#endif
