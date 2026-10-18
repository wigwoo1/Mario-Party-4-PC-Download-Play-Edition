#ifndef PARTYBOARD_MSM_DSP_SAMPLE_H
#define PARTYBOARD_MSM_DSP_SAMPLE_H
#include "dsp_adpcm_sample.h"

static inline uint32_t PartyboardMsmU32(const uint8_t *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}
static inline unsigned PartyboardMsmU16(const uint8_t *p) {
    return (unsigned)p[0] * 256 + p[1];
}
static inline int16_t PartyboardMsmS16(const uint8_t *p) {
    unsigned value = PartyboardMsmU16(p);
    return (int16_t)(value < 32768 ? (int)value : (int)value - 65536);
}

/* Original 32-bit SDIR entries; group/sample buffers remain caller-owned.
 * Sample IDs are resolved after MusyX macro selection, not effect IDs.
 * Only format0 is supported; unsupported compression fails explicitly. */
static inline int PartyboardMsmDspSampleOpen(PartyboardDspAdpcmSample *destination,
        const uint8_t *group, size_t group_bytes, const uint8_t *samples,
        size_t sample_bytes, unsigned sample_id) {
    size_t directory, cursor;
    if (!group || !samples || group_bytes < 16 || sample_id >= 65535) return 0;
    directory = PartyboardMsmU32(group + 8);
    if (directory > group_bytes) return 0;
    cursor = directory;
    while (group_bytes - cursor >= 2) {
        const uint8_t *entry = group + cursor;
        unsigned id = PartyboardMsmU16(entry);
        if (id == 65535) return 0;
        if (group_bytes - cursor < 32) return 0;
        if (id == sample_id) {
            PartyboardDspAdpcm decoder = {0};
            PartyboardDspAdpcmSample result;
            size_t offset = PartyboardMsmU32(entry + 4);
            size_t extra = PartyboardMsmU32(entry + 28);
            uint32_t length = PartyboardMsmU32(entry + 16);
            const uint8_t *params;
            unsigned i;
            if (length >> 24 || offset > sample_bytes || extra > group_bytes - directory
                || group_bytes - directory - extra < 40) return 0;
            params = group + directory + extra;
            for (i = 0; i < 16; ++i)
                decoder.coefficients[i / 2][i % 2] = PartyboardMsmS16(params + 8 + i * 2);
            if (!PartyboardDspAdpcmSampleInit(&result, &decoder, samples + offset,
                    sample_bytes - offset, length, params[2], PartyboardMsmU32(entry + 20),
                    PartyboardMsmU32(entry + 24), params[3],
                    PartyboardMsmS16(params + 6), PartyboardMsmS16(params + 4))) return 0;
            *destination = result;
            return 1;
        }
        cursor += 32;
    }
    return 0;
}
#endif
