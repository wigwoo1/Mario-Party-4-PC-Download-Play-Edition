/* Real private stream/device pipeline probe; reads PDT only from stdin. */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <io.h>
#include "../src/port/pdt_stream.h"
#include "../src/port/pcm_output.h"

int main(void) {
    uint8_t *data;
    size_t size, frames = 0, nonzero = 0;
    PartyboardPdtStream source;
    PartyboardPcmOutput output = {0};
    int16_t pcm[1024];
    Uint64 deadline;
    int result = 1;
    _setmode(_fileno(stdin), _O_BINARY);
    data = (uint8_t *)malloc(64 * 1024 * 1024);
    if (!data) return 2;
    size = fread(data, 1, 64 * 1024 * 1024, stdin);
    if (!feof(stdin) || ferror(stdin) || !PartyboardPdtStreamOpen(&source, data, size, 1)) goto cleanup;
    if (!PartyboardPcmOutputOpen(&output, source.frequency)) goto cleanup;
    deadline = SDL_GetTicks() + 4000;
    while (frames < source.frequency && SDL_GetTicks() < deadline) {
        size_t wanted = source.frequency - frames, produced, i;
        int capacity;
        if (wanted > 512) wanted = 512;
        capacity = PartyboardPcmOutputCapacity(&output, wanted);
        if (capacity < 0) goto cleanup;
        if (!capacity) { SDL_Delay(5); continue; }
        produced = PartyboardPdtStreamReadStereo(&source, pcm, wanted);
        if (!produced) goto cleanup;
        for (i = 0; i < produced * 2; ++i) {
            /* Conservative probe volume; no production gain semantics. */
            if (pcm[i]) ++nonzero;
            pcm[i] = (int16_t)(pcm[i] / 4);
        }
        if (PartyboardPcmOutputSubmit(&output, pcm, produced) != 1) goto cleanup;
        frames += produced;
    }
    if (frames != source.frequency || !nonzero || !PartyboardPcmOutputFinish(&output)) goto cleanup;
    while (SDL_GetAudioStreamQueued(output.stream) > 0 && SDL_GetTicks() < deadline) SDL_Delay(5);
    if (SDL_GetAudioStreamQueued(output.stream) != 0) goto cleanup;
    printf("Real PDT stream1: frames=%zu nonzero=%zu source queue drained; audible fidelity unverified\n", frames, nonzero);
    result = 0;
cleanup:
    if (result) fprintf(stderr, "Playback probe failed: %s\n", SDL_GetError());
    PartyboardPcmOutputClose(&output);
    free(data);
    return result;
}
