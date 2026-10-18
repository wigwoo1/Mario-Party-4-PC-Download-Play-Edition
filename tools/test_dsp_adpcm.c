#include <assert.h>
#include <string.h>
#include "../src/port/dsp_adpcm.h"
#include "../src/port/dsp_adpcm_sample.h"
#include "../src/port/msm_dsp_sample.h"
#include "../src/port/pcm16_sample.h"
#include "../src/port/msm_sample.h"
#include "../src/port/pcm_mix.h"
#include "../src/port/src_phase.h"
#include "../src/port/src_filter.h"
#include "../src/port/dsp_adpcm_stream.h"
#include "../src/port/pdt_stream.h"
#include "../src/port/pdt_player.h"

int main(void) {
    {
        int16_t coefficients[512] = {0}, history[4] = {1000, -1000, 32767, -32768};
        int16_t output = 123;
        coefficients[0] = 16384;
        coefficients[509] = 16384;
        assert(PartyboardSrcFilter(history, 0, coefficients, 512, &output) && output == 500);
        assert(PartyboardSrcFilter(history, 65535, coefficients, 512, &output) && output == -500);
        coefficients[0] = coefficients[2] = 32767;
        assert(PartyboardSrcFilter(history, 0, coefficients, 512, &output) && output == 32767);
        output = 123;
        assert(!PartyboardSrcFilter(history, 0, coefficients, 511, &output) && output == 123);
        assert(!PartyboardSrcFilter(history, 65536, coefficients, 512, &output));
    }
    {
        PartyboardSrcPhase phase = {0, 32768};
        uint32_t consumed = 0;
        unsigned i;
        assert(PartyboardSrcPhaseAdvance(&phase) == 0 && phase.fraction == 32768);
        phase.ratio = 65536;
        assert(PartyboardSrcPhaseAdvance(&phase) == 1 && phase.fraction == 32768);
        phase.ratio = 0x3fff0; /* original hardware maximum pitch */
        for (i = 0; i < 32000; ++i) consumed += PartyboardSrcPhaseAdvance(&phase);
        assert(consumed == 127992 && phase.fraction == 45056);
    }
    {
        int64_t bus[6] = {0};
        int16_t source[1] = {30000}, negative[1] = {-30000};
        int16_t gains[6] = {32767, 16384, 0, -16384, 0, 0};
        int16_t output[6];
        PartyboardPcmMixAdd(bus, source, 1, gains, 6);
        PartyboardPcmMixAdd(bus, source, 1, gains, 6);
        PartyboardPcmMixFinish(bus, output, 6);
        assert(output[0] == 32767 && output[1] == 30000 && output[3] == -30000);
        PartyboardPcmMixAdd(bus, negative, 1, gains, 6);
        PartyboardPcmMixFinish(bus, output, 6);
        assert(output[0] == 29999 && output[1] == 15000 && output[3] == -15000);
        assert(output[2] == 0 && output[4] == 0 && output[5] == 0);
    }
    {
        uint8_t group[96] = {0}, data[8] = {0x80, 0, 0, 1, 0, 2, 0x7f, 0xff};
        PartyboardMsmSample sample, saved;
        int16_t pcm[8];
        group[11] = 16;
        group[17] = 7;
        group[28] = 60; group[30] = 0x7d; /* root60/rate32000 */
        group[32] = 2; group[35] = 4;
        group[48] = group[49] = 255;
        assert(PartyboardMsmSampleOpen(&sample, group, 96, data, 8, 7, 1));
        assert(sample.frequency == 32000 && sample.root_key == 60 && sample.format == 2);
        assert(PartyboardMsmSampleRead(&sample, pcm, 8) == 3 && pcm[0] == 1 && pcm[2] == 32767);
        saved = sample;
        group[32] = 3;
        assert(!PartyboardMsmSampleOpen(&sample, group, 96, data, 8, 7, 0));
        assert(memcmp(&sample, &saved, sizeof(sample)) == 0);
    }
    {
        const uint8_t bytes[8] = {0x80, 0, 0xff, 0xff, 0, 1, 0x7f, 0xff};
        PartyboardPcm16Sample sample, saved;
        int16_t pcm[8];
        assert(PartyboardPcm16SampleInit(&sample, bytes, 8, 4, 0, 0, 0));
        assert(PartyboardPcm16SampleRead(&sample, pcm, 8) == 4);
        assert(pcm[0] == -32768 && pcm[1] == -1 && pcm[2] == 1 && pcm[3] == 32767);
        assert(PartyboardPcm16SampleInit(&sample, bytes, 8, 4, 1, 1, 2));
        assert(PartyboardPcm16SampleRead(&sample, pcm, 5) == 5);
        assert(pcm[0] == -1 && pcm[1] == 1 && pcm[2] == -1 && pcm[4] == -1);
        saved = sample;
        assert(!PartyboardPcm16SampleInit(&sample, bytes, 7, 4, 0, 0, 0));
        assert(memcmp(&saved, &sample, sizeof(sample)) == 0);
        assert(!PartyboardPcm16SampleInit(&sample, bytes, 8, 4, 0, 3, 2));
    }
    {
        uint8_t group[96] = {0}, data[8] = {0, 0x12};
        PartyboardDspAdpcmSample sample = {0}, saved;
        int16_t pcm[14];
        group[11] = 16; /* directory */
        group[17] = 7; /* sample ID */
        group[35] = 14; /* sample count */
        group[47] = 40; /* parameters relative to directory */
        group[48] = group[49] = 255; /* terminator */
        assert(PartyboardMsmDspSampleOpen(&sample, group, sizeof(group), data, sizeof(data), 7));
        assert(PartyboardDspAdpcmSampleRead(&sample, pcm, 14) == 14 && pcm[0] == 1 && pcm[1] == 2);
        saved = sample;
        assert(!PartyboardMsmDspSampleOpen(&sample, group, sizeof(group), data, 7, 7));
        assert(memcmp(&sample, &saved, sizeof(sample)) == 0);
        group[47] = 80;
        assert(!PartyboardMsmDspSampleOpen(&sample, group, sizeof(group), data, 8, 7));
        group[47] = 40;
        group[32] = 1;
        assert(!PartyboardMsmDspSampleOpen(&sample, group, sizeof(group), data, 8, 7));
        assert(!PartyboardMsmDspSampleOpen(&sample, group, sizeof(group), data, 8, 8));
    }
    {
        PartyboardDspAdpcm decoder = {0};
        PartyboardDspAdpcmSample sample;
        uint8_t data[16] = {0, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
                            1, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
        int16_t pcm[32];
        decoder.coefficients[0][0] = 2048;
        assert(PartyboardDspAdpcmSampleInit(&sample, &decoder, data, 16, 20, 0, 3, 2, 0, 100, 0));
        assert(PartyboardDspAdpcmSampleRead(&sample, pcm, 8) == 8);
        assert(pcm[4] == 5 && pcm[5] == 101 && pcm[6] == 102 && pcm[7] == 101);
        assert(PartyboardDspAdpcmSampleInit(&sample, &decoder, data, 16, 17, 0, 0, 0, 0, 0, 0));
        assert(PartyboardDspAdpcmSampleRead(&sample, pcm, 32) == 17);
        assert(pcm[13] == 14 && pcm[14] == 16 && pcm[16] == 20);
        assert(!PartyboardDspAdpcmSampleInit(&sample, &decoder, data, 8, 17, 0, 0, 0, 0, 0, 0));
    }
    {
        PartyboardPdtPlayer player = {0};
        PartyboardDspAdpcm decoder = {0};
        uint8_t encoded[8] = {0, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77};
        int16_t pcm[1068];
        size_t total = 0;
        unsigned tick;
        player.source.channel_count = 1;
        player.source.frequency = 32000;
        player.source.volume = 127;
        player.active = 1;
        assert(PartyboardDspAdpcmStreamInit(&player.source.channels[0], &decoder, encoded, 8, 0, 1));
        for (tick = 0; tick < 60; ++tick) total += PartyboardPdtPlayerTick(&player, 60, pcm, 534);
        assert(total == 32000 && player.remainder == 0 && player.active);
        PartyboardPdtPlayerFade(&player, 2);
        assert(PartyboardPdtPlayerTick(&player, 60, pcm, 534) == 533 && pcm[0] == 7);
        assert(PartyboardPdtPlayerTick(&player, 60, pcm, 534) == 533 && pcm[0] == 3);
        assert(!player.active && PartyboardPdtPlayerTick(&player, 60, pcm, 534) == 0);
        player.active = 1;
        player.fade_frames = player.fade_remaining = player.remainder = 0;
        player.source.frequency = 840;
        assert(PartyboardDspAdpcmStreamInit(&player.source.channels[0], &decoder, encoded, 8, 0, 0));
        assert(PartyboardPdtPlayerTick(&player, 60, pcm, 534) == 14);
        assert(!player.active); /* Complete on final exact-sized tick. */
    }
    {
        PartyboardPdtStream stereo = {0};
        PartyboardDspAdpcm decoder = {0};
        uint8_t left[8] = {0, 0x12};
        uint8_t right[8] = {0, 0xfe};
        int16_t pcm[32] = {0};
        stereo.channel_count = 2;
        assert(PartyboardDspAdpcmStreamInit(&stereo.channels[0], &decoder, left, 8, 0, 0));
        assert(PartyboardDspAdpcmStreamInit(&stereo.channels[1], &decoder, right, 8, 0, 0));
        assert(PartyboardPdtStreamReadStereo(&stereo, pcm, 1) == 1);
        assert(pcm[0] == 1 && pcm[1] == -1);
        assert(PartyboardPdtStreamReadStereo(&stereo, pcm + 2, 15) == 13);
        assert(pcm[2] == 2 && pcm[3] == -2);
        stereo.channel_count = 1;
        assert(PartyboardDspAdpcmStreamInit(&stereo.channels[0], &decoder, left, 8, 0, 0));
        assert(PartyboardPdtStreamReadStereo(&stereo, pcm, 1) == 1);
        assert(pcm[0] == 1 && pcm[1] == 1);
    }
    {
        uint8_t pdt[164] = {0};
        PartyboardPdtStream stream = {0}, saved;
        int16_t samples[56];
        pdt[1] = 1; pdt[3] = 1;
        pdt[19] = 32; pdt[23] = 36; pdt[27] = 68; pdt[31] = 100;
        pdt[35] = 68;
        pdt[69] = 127; pdt[74] = 125; /* 32000Hz */
        pdt[79] = 128; pdt[83] = 2; pdt[87] = 100;
        pdt[101] = 0x71;
        assert(PartyboardPdtStreamOpen(&stream, pdt, sizeof(pdt), 0));
        assert(stream.channel_count == 1 && stream.frequency == 32000);
        assert(PartyboardDspAdpcmStreamRead(&stream.channels[0], samples, 56) == 56);
        assert(samples[0] == 7 && samples[1] == 1);
        saved = stream;
        pdt[89] = 1; /* Out-of-range coefficient index. */
        assert(!PartyboardPdtStreamOpen(&stream, pdt, sizeof(pdt), 0));
        assert(!memcmp(&saved, &stream, sizeof(stream)));
        pdt[89] = 0;
        assert(!PartyboardPdtStreamOpen(&stream, pdt, 120, 0));
        assert(!PartyboardPdtStreamOpen(&stream, pdt, sizeof(pdt), 1));
    }
    {
        PartyboardDspAdpcm decoder = {0};
        PartyboardDspAdpcmStream stream;
        uint8_t encoded[16] = {0x10, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
                               0x10, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
        int16_t decoded[42];
        unsigned index;
        decoder.coefficients[1][0] = 2048;
        assert(PartyboardDspAdpcmStreamInit(&stream, &decoder, encoded, 16, 8, 1));
        assert(PartyboardDspAdpcmStreamRead(&stream, decoded, 5) == 5);
        assert(PartyboardDspAdpcmStreamRead(&stream, decoded + 5, 37) == 37);
        for (index = 0; index < 28; ++index) assert(decoded[index] == (int16_t)(index + 1));
        for (index = 28; index < 42; ++index) assert(decoded[index] == decoded[index - 14]);
        assert(PartyboardDspAdpcmStreamInit(&stream, &decoder, encoded, 16, 0, 0));
        assert(PartyboardDspAdpcmStreamRead(&stream, decoded, 42) == 28);
        assert(!PartyboardDspAdpcmStreamInit(&stream, &decoder, encoded, 15, 0, 0));
    }
    PartyboardDspAdpcm state = {0};
    uint8_t frame[8] = {0, 0x78, 0x1f};
    int16_t output[14] = {0};
    PartyboardDspAdpcm saved;
    assert(PartyboardDspAdpcmFrame(&state, frame, output));
    assert(output[0] == 7 && output[1] == -8);
    assert(output[2] == 1 && output[3] == -1);
    state.coefficients[1][0] = 2048;
    state.previous = 100;
    frame[0] = 0x10;
    memset(frame + 1, 0, 7);
    assert(PartyboardDspAdpcmFrame(&state, frame, output));
    assert(output[0] == 100 && output[13] == 100);
    frame[0] = 0x0f;
    frame[1] = 0x78;
    assert(PartyboardDspAdpcmFrame(&state, frame, output));
    assert(output[0] == 32767 && output[1] == -32768);
    saved = state;
    frame[0] = 0x80;
    output[0] = 123;
    assert(!PartyboardDspAdpcmFrame(&state, frame, output));
    assert(!memcmp(&saved, &state, sizeof(state)) && output[0] == 123);
    return 0;
}
