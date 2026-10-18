/* Private validation pipe: big-endian coefficient table then complete frames.
 * Emits little-endian PCM16. No filesystem or device access. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
#include "../src/port/dsp_adpcm.h"
#include "../src/port/dsp_adpcm_stream.h"

int main(int argc, char **argv) {
    PartyboardDspAdpcm state = {0};
    uint8_t coefficients[32], frame[8], pcm[28];
    int16_t samples[14];
    size_t size;
    unsigned i;
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    if (fread(coefficients, 1, 32, stdin) != 32) return 2;
    for (i = 0; i < 16; ++i) {
        unsigned value = ((unsigned)coefficients[i * 2] << 8) | coefficients[i * 2 + 1];
        state.coefficients[i / 2][i % 2] = (int16_t)(value < 32768 ? (int)value : (int)value - 65536);
    }
    if (argc == 2 && !strcmp(argv[1], "--stream")) {
        PartyboardDspAdpcmStream stream;
        uint8_t *encoded = NULL;
        size_t length = 0, capacity = 0;
        int16_t chunk[257];
        uint8_t bytes[514];
        while ((size = fread(frame, 1, 8, stdin)) != 0) {
            if (size != 8) { free(encoded); return 3; }
            if (length == capacity) {
                size_t next = capacity ? capacity * 2 : 4096;
                uint8_t *grown;
                if (next > 64 * 1024 * 1024) { free(encoded); return 6; }
                grown = (uint8_t *)realloc(encoded, next);
                if (!grown) { free(encoded); return 6; }
                encoded = grown;
                capacity = next;
            }
            memcpy(encoded + length, frame, 8);
            length += 8;
        }
        if (ferror(stdin) || !PartyboardDspAdpcmStreamInit(&stream, &state, encoded, length, 0, 0)) {
            free(encoded); return 3;
        }
        while ((size = PartyboardDspAdpcmStreamRead(&stream, chunk, 257)) != 0) {
            for (i = 0; i < size; ++i) {
                uint16_t value = (uint16_t)chunk[i];
                bytes[i * 2] = (uint8_t)value;
                bytes[i * 2 + 1] = (uint8_t)(value >> 8);
            }
            if (fwrite(bytes, 2, size, stdout) != size) { free(encoded); return 4; }
        }
        free(encoded);
        return stream.failed || fflush(stdout) ? 5 : 0;
    }
    while ((size = fread(frame, 1, 8, stdin)) != 0) {
        if (size != 8 || !PartyboardDspAdpcmFrame(&state, frame, samples)) return 3;
        for (i = 0; i < 14; ++i) {
            uint16_t value = (uint16_t)samples[i];
            pcm[i * 2] = (uint8_t)value;
            pcm[i * 2 + 1] = (uint8_t)(value >> 8);
        }
        if (fwrite(pcm, 1, 28, stdout) != 28) return 4;
    }
    return ferror(stdin) || fflush(stdout) ? 5 : 0;
}
