/* Private stdin validation only: 40-byte SNDADPCMinfo, four BE u32 values
 * (length, loop start, loop length, requested count), then encoded frames. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
#include "../src/port/dsp_adpcm_sample.h"
#include "../src/port/msm_dsp_sample.h"
#include "../src/port/pcm16_sample.h"
#include "../src/port/msm_sample.h"
#include "../src/port/musyx_sample_directory.h"
static int converted_open(PartyboardMsmSample *source, const uint8_t *group,
        size_t group_bytes, uint8_t *samples, size_t sample_bytes, unsigned id) {
    SDIR_DATA *directory = PartyboardMusyxSampleDirectoryConvert(group, group_bytes, samples, sample_bytes);
    size_t i;
    int opened = 0;
    if (!directory) return 0;
    for (i = 0; directory[i].id != 65535; ++i) {
        SAMPLE_INFO info = {0};
        if (directory[i].id != id) continue;
        info.info = directory[i].header.info;
        info.addr = samples + directory[i].offset;
        info.extraData = directory[i].extraData ? (uint8_t *)directory + directory[i].extraData : NULL;
        info.length = directory[i].header.length & 0xffffff;
        info.compType = (u8)(directory[i].header.length >> 24);
        info.loop = directory[i].header.loopOffset;
        info.loopLength = directory[i].header.loopLength;
        opened = PartyboardMusyxSampleSourceOpen(source, &info, sample_bytes - directory[i].offset);
        break;
    }
    free(directory);
    return opened;
}
static unsigned be32(const unsigned char *p) {
    return (unsigned)p[0] << 24 | (unsigned)p[1] << 16 | (unsigned)p[2] << 8 | p[3];
}
static int16_t be16(const unsigned char *p) {
    unsigned n = (unsigned)p[0] << 8 | p[1];
    return (int16_t)(n < 32768 ? (int)n : (int)n - 65536);
}
int main(int argc, char **argv) {
    unsigned char header[56], *data;
    PartyboardDspAdpcm decoder = {0};
    PartyboardDspAdpcmSample source = {0};
    PartyboardPcm16Sample pcm_source;
    PartyboardMsmSample common_source;
    int pcm_mode = argc == 2 && !strcmp(argv[1], "--pcm16");
    int converted_mode = argc == 2 && !strcmp(argv[1], "--converted-container");
    int common_mode = converted_mode || (argc == 2 && !strcmp(argv[1], "--common-container"));
    size_t length, bytes, remaining;
    unsigned i;
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    if (pcm_mode) {
        if (fread(header, 1, 20, stdin) != 20) return 2;
        length = be32(header);
        if (!length || length > 0xffffff) return 2;
        bytes = length * 2;
        data = (uint8_t *)malloc(bytes);
        if (!data) return 3;
        if (fread(data, 1, bytes, stdin) != bytes || !PartyboardPcm16SampleInit(&pcm_source,
                data, bytes, length, be32(header + 4), be32(header + 8), be32(header + 12))) {
            free(data); return 4;
        }
        remaining = be32(header + 16);
    } else if (common_mode || (argc == 2 && !strcmp(argv[1], "--container"))) {
        uint8_t *group;
        size_t group_bytes, sample_bytes;
        if (fread(header, 1, 16, stdin) != 16) return 2;
        group_bytes = be32(header);
        sample_bytes = be32(header + 4);
        if (!group_bytes || group_bytes > 64 * 1024 * 1024 || !sample_bytes || sample_bytes > 64 * 1024 * 1024) return 2;
        group = (uint8_t *)malloc(group_bytes);
        data = (uint8_t *)malloc(sample_bytes);
        if (!group || !data) { free(group); free(data); return 3; }
        if (fread(group, 1, group_bytes, stdin) != group_bytes || fread(data, 1, sample_bytes, stdin) != sample_bytes
            || (converted_mode ? !converted_open(&common_source, group, group_bytes, data, sample_bytes, be32(header + 8))
                : common_mode ? !PartyboardMsmSampleOpen(&common_source, group, group_bytes, data, sample_bytes, be32(header + 8), 0)
                : !PartyboardMsmDspSampleOpen(&source, group, group_bytes, data, sample_bytes, be32(header + 8)))) {
            free(group); free(data); return 4;
        }
        free(group); /* Decoder copied parameters; source retains sample buffer. */
        remaining = be32(header + 12);
    } else {
    if (fread(header, 1, 56, stdin) != 56) return 2;
    length = be32(header + 40);
    if (!length || length > 0xffffff) return 2;
    bytes = ((length + 13) / 14) * 8;
    data = (unsigned char *)malloc(bytes);
    if (!data) return 3;
    if (fread(data, 1, bytes, stdin) != bytes) { free(data); return 2; }
    for (i = 0; i < 16; ++i) decoder.coefficients[i / 2][i % 2] = be16(header + 8 + i * 2);
    if (!PartyboardDspAdpcmSampleInit(&source, &decoder, data, bytes, length,
            header[2], be32(header + 44), be32(header + 48), header[3],
            be16(header + 6), be16(header + 4))) { free(data); return 4; }
    remaining = be32(header + 52);
    }
    while (remaining) {
        int16_t pcm[257];
        unsigned char output[514];
        size_t got = common_mode ? PartyboardMsmSampleRead(&common_source, pcm, remaining > 257 ? 257 : remaining)
            : pcm_mode ? PartyboardPcm16SampleRead(&pcm_source, pcm, remaining > 257 ? 257 : remaining)
            : PartyboardDspAdpcmSampleRead(&source, pcm, remaining > 257 ? 257 : remaining);
        if (!got) { free(data); return 5; }
        for (i = 0; i < got; ++i) {
            output[i * 2] = (unsigned char)pcm[i];
            output[i * 2 + 1] = (unsigned char)((uint16_t)pcm[i] >> 8);
        }
        if (fwrite(output, 2, got, stdout) != got) { free(data); return 6; }
        remaining -= got;
    }
    free(data);
    return (!pcm_mode && !common_mode && source.failed) || fflush(stdout) ? 7 : 0;
}
