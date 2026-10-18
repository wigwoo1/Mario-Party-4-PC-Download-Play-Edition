#ifndef PARTYBOARD_DSP_ADPCM_STREAM_H
#define PARTYBOARD_DSP_ADPCM_STREAM_H
#include <stddef.h>
#include "dsp_adpcm.h"

typedef struct {
    PartyboardDspAdpcm decoder;
    const uint8_t *data;
    size_t bytes, position, loop_start;
    int16_t pending[14];
    unsigned pending_position;
    int16_t loop_previous, loop_previous2;
    int looping, loop_saved, failed;
} PartyboardDspAdpcmStream;

/* Encoded memory must remain valid. Initial histories are explicit; a loop
 * snapshot is captured at its first frame before consuming its samples. */
static inline int PartyboardDspAdpcmStreamInit(PartyboardDspAdpcmStream *stream,
        const PartyboardDspAdpcm *decoder, const uint8_t *data, size_t bytes,
        size_t loop_start, int looping) {
    if (!data || !bytes || bytes % 8 || loop_start % 8 || loop_start >= bytes) return 0;
    stream->decoder = *decoder;
    stream->data = data;
    stream->bytes = bytes;
    stream->position = 0;
    stream->loop_start = loop_start;
    stream->pending_position = 14;
    stream->loop_previous = stream->loop_previous2 = 0;
    stream->looping = looping;
    stream->loop_saved = stream->failed = 0;
    return 1;
}

/* Returns actual samples produced; finite end/error returns a short read. */
static inline size_t PartyboardDspAdpcmStreamRead(PartyboardDspAdpcmStream *stream,
                                                 int16_t *output, size_t samples) {
    size_t produced = 0;
    while (produced < samples && !stream->failed) {
        if (stream->pending_position == 14) {
            if (stream->position == stream->bytes) {
                if (!stream->looping || !stream->loop_saved) break;
                stream->position = stream->loop_start;
                stream->decoder.previous = stream->loop_previous;
                stream->decoder.previous2 = stream->loop_previous2;
            }
            if (stream->position == stream->loop_start && !stream->loop_saved) {
                stream->loop_previous = stream->decoder.previous;
                stream->loop_previous2 = stream->decoder.previous2;
                stream->loop_saved = 1;
            }
            if (!PartyboardDspAdpcmFrame(&stream->decoder,
                    stream->data + stream->position, stream->pending)) {
                stream->failed = 1;
                break;
            }
            stream->position += 8;
            stream->pending_position = 0;
        }
        output[produced++] = stream->pending[stream->pending_position++];
    }
    return produced;
}
#endif
