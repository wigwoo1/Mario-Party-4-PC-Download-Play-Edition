#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
#include "../src/port/pdt_stream.h"

int main(int argc, char **argv) {
    uint8_t *data;
    size_t size, produced;
    unsigned channel, i;
    PartyboardPdtStream stream;
    int16_t samples[257];
    uint8_t pcm[514];
    if (argc != 3) return 2;
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    data = (uint8_t *)malloc(64 * 1024 * 1024);
    if (!data) return 3;
    size = fread(data, 1, 64 * 1024 * 1024, stdin);
    if (ferror(stdin) || !feof(stdin)) { free(data); return 4; }
    channel = (unsigned)strtoul(argv[2], NULL, 10);
    if (!PartyboardPdtStreamOpen(&stream, data, size, (unsigned)strtoul(argv[1], NULL, 10))
        || channel >= stream.channel_count) { free(data); return 5; }
    /* One original pass only; preserve flags in loader but avoid infinite
     * validation output for containers declaring looping streams. */
    stream.channels[channel].looping = 0;
    while ((produced = PartyboardDspAdpcmStreamRead(&stream.channels[channel], samples, 257)) != 0) {
        for (i = 0; i < produced; ++i) {
            uint16_t value = (uint16_t)samples[i];
            pcm[i * 2] = (uint8_t)value;
            pcm[i * 2 + 1] = (uint8_t)(value >> 8);
        }
        if (fwrite(pcm, 2, produced, stdout) != produced) { free(data); return 6; }
    }
    free(data);
    return stream.channels[channel].failed || fflush(stdout) ? 7 : 0;
}
