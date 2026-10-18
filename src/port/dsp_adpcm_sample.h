#ifndef PARTYBOARD_DSP_ADPCM_SAMPLE_H
#define PARTYBOARD_DSP_ADPCM_SAMPLE_H
#include <stddef.h>
#include "dsp_adpcm.h"

typedef struct {
    PartyboardDspAdpcm decoder;
    const uint8_t *data;
    size_t position, end, loop_start;
    int16_t loop_previous, loop_previous2;
    uint8_t ps, loop_ps;
    int looping, failed;
} PartyboardDspAdpcmSample;

/* Sample positions exclude each frame's two header nibbles. Histories are
 * supplied by SNDADPCMinfo, including loops inside a frame. */
static inline int PartyboardDspAdpcmSampleInit(PartyboardDspAdpcmSample *sample,
        const PartyboardDspAdpcm *decoder, const uint8_t *data, size_t bytes,
        size_t length, uint8_t initial_ps, size_t loop_start, size_t loop_length,
        uint8_t loop_ps, int16_t loop_previous, int16_t loop_previous2) {
    if (!data || !length || length / 14 + (length % 14 != 0) > bytes / 8
        || (initial_ps >> 4) >= 8 || loop_start > length
        || loop_length > length - loop_start || (loop_length && (loop_ps >> 4) >= 8)) return 0;
    sample->decoder = *decoder;
    sample->data = data;
    sample->position = 0;
    sample->end = loop_length ? loop_start + loop_length : length;
    sample->loop_start = loop_start;
    sample->ps = initial_ps;
    sample->loop_ps = loop_ps;
    sample->loop_previous = loop_previous;
    sample->loop_previous2 = loop_previous2;
    sample->looping = loop_length != 0;
    sample->failed = 0;
    return 1;
}

static inline size_t PartyboardDspAdpcmSampleRead(PartyboardDspAdpcmSample *sample,
        int16_t *output, size_t count) {
    size_t produced = 0;
    while (produced < count && !sample->failed) {
        size_t within, frame;
        unsigned byte;
        int restored = 0;
        if (sample->position == sample->end) {
            if (!sample->looping) break;
            sample->position = sample->loop_start;
            sample->decoder.previous = sample->loop_previous;
            sample->decoder.previous2 = sample->loop_previous2;
            sample->ps = sample->loop_ps;
            restored = 1;
        }
        within = sample->position % 14;
        frame = (sample->position / 14) * 8;
        if (!within && sample->position && !restored) sample->ps = sample->data[frame];
        byte = sample->data[frame + 1 + within / 2];
        if (!PartyboardDspAdpcmNibble(&sample->decoder, sample->ps,
                (within & 1) ? byte & 15 : byte >> 4, &output[produced])) {
            sample->failed = 1;
            break;
        }
        ++sample->position;
        ++produced;
    }
    return produced;
}
#endif
