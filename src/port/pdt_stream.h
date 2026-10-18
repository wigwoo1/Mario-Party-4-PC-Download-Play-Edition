#ifndef PARTYBOARD_PDT_STREAM_H
#define PARTYBOARD_PDT_STREAM_H
#include "dsp_adpcm_stream.h"

typedef struct {
    PartyboardDspAdpcmStream channels[2];
    unsigned channel_count, frequency, volume;
} PartyboardPdtStream;

static inline uint32_t PartyboardPdtU32(const uint8_t *data) {
    return (uint32_t)data[0] * 16777216u + (uint32_t)data[1] * 65536u
           + (uint32_t)data[2] * 256u + data[3];
}
static inline unsigned PartyboardPdtU16(const uint8_t *data) {
    return (unsigned)data[0] * 256u + data[1];
}

/* Container remains caller-owned. Failure leaves destination unchanged. */
static inline int PartyboardPdtStreamOpen(PartyboardPdtStream *destination,
        const uint8_t *data, size_t size, unsigned stream_id) {
    PartyboardPdtStream result = {0};
    uint32_t list, parameters, packs, samples, pack, length, loop_start;
    unsigned count, channel, i;
    const uint8_t *entry;
    if (!data || size < 32 || PartyboardPdtU16(data) != 1) return 0;
    count = PartyboardPdtU16(data + 2);
    list = PartyboardPdtU32(data + 16);
    parameters = PartyboardPdtU32(data + 20);
    packs = PartyboardPdtU32(data + 24);
    samples = PartyboardPdtU32(data + 28);
    if (list < 32 || list > parameters || parameters > packs || packs > samples
        || samples > size || count > (parameters - list) / 4 || stream_id >= count) return 0;
    pack = PartyboardPdtU32(data + list + stream_id * 4);
    if (!pack || pack < packs || pack > samples || samples - pack < 32) return 0;
    entry = data + pack;
    result.channel_count = (entry[0] & 1) ? 2 : 1;
    result.frequency = PartyboardPdtU16(entry + 6);
    result.volume = entry[1];
    if (!result.frequency || result.volume > 127) return 0;
    length = (PartyboardPdtU32(entry + 8) >> 1) & ~31u;
    loop_start = (PartyboardPdtU32(entry + 12) >> 1) & ~7u;
    for (channel = 0; channel < result.channel_count; ++channel) {
        PartyboardDspAdpcm decoder = {0};
        uint32_t offset = PartyboardPdtU32(entry + 16 + channel * 8);
        unsigned index = PartyboardPdtU16(entry + 20 + channel * 8);
        if (offset < samples || offset > size || length > size - offset
            || index >= (packs - parameters) / 32) return 0;
        for (i = 0; i < 16; ++i) {
            unsigned value = PartyboardPdtU16(data + parameters + index * 32 + i * 2);
            decoder.coefficients[i / 2][i % 2] = (int16_t)(value < 32768 ? (int)value : (int)value - 65536);
        }
        if (!PartyboardDspAdpcmStreamInit(&result.channels[channel], &decoder,
                data + offset, length, loop_start, (entry[0] & 2) != 0)) return 0;
    }
    *destination = result;
    return 1;
}

/* Raw interleaved stereo PCM, before gain/pan/resampling/mixing. A mono
 * stream occupies both channels; stereo keeps distinct channel samples. */
static inline size_t PartyboardPdtStreamReadStereo(PartyboardPdtStream *stream,
                                                  int16_t *output, size_t frames) {
    size_t produced = 0;
    if (stream->channel_count != 1 && stream->channel_count != 2) return 0;
    while (produced < frames) {
        int16_t left, right;
        if (PartyboardDspAdpcmStreamRead(&stream->channels[0], &left, 1) != 1) break;
        right = left;
        if (stream->channel_count == 2
            && PartyboardDspAdpcmStreamRead(&stream->channels[1], &right, 1) != 1) {
            stream->channels[0].failed = stream->channels[1].failed = 1;
            break;
        }
        output[produced * 2] = left;
        output[produced * 2 + 1] = right;
        ++produced;
    }
    return produced;
}
#endif
