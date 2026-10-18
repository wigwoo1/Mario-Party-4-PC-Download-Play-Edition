#ifndef PARTYBOARD_PCM16_SAMPLE_H
#define PARTYBOARD_PCM16_SAMPLE_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t *data;
    size_t position, end, loop_start;
    int looping;
} PartyboardPcm16Sample;

/* Original console PCM16 payload is big-endian. Positions count samples;
 * offset corresponds to the original hardware PCM playback start offset. */
static inline int PartyboardPcm16SampleInit(PartyboardPcm16Sample *destination,
        const uint8_t *data, size_t bytes, size_t length, size_t offset,
        size_t loop_start, size_t loop_length) {
    PartyboardPcm16Sample result;
    if (!data || !length || length > bytes / 2 || offset >= length
        || loop_start > length || loop_length > length - loop_start) return 0;
    result.data = data;
    result.position = offset;
    result.end = loop_length ? loop_start + loop_length : length;
    if (offset >= result.end) return 0;
    result.loop_start = loop_start;
    result.looping = loop_length != 0;
    *destination = result;
    return 1;
}

static inline size_t PartyboardPcm16SampleRead(PartyboardPcm16Sample *sample,
        int16_t *output, size_t count) {
    size_t produced = 0;
    while (produced < count) {
        unsigned value;
        const uint8_t *p;
        if (sample->position == sample->end) {
            if (!sample->looping) break;
            sample->position = sample->loop_start;
        }
        p = sample->data + sample->position * 2;
        value = (unsigned)p[0] * 256 + p[1];
        output[produced++] = (int16_t)(value < 32768 ? (int)value : (int)value - 65536);
        ++sample->position;
    }
    return produced;
}
#endif
