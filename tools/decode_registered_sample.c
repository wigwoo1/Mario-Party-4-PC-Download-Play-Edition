/* Private stdin validation of real SDIR registration/lookup. Hardware sample
 * storage is observed at its boundary; no synthesis/device claims. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
#include "../extern/musyx/src/musyx/runtime/synthdata.c"
#include "../extern/musyx/src/musyx/runtime/snd_service.c"
#include "../src/port/musyx_sample_directory.h"
#include "../src/port/musyx_group.h"
static void *registered_address;
static void *removed_address;
static unsigned removals;
u32 synthGetTicksPerSecond(SYNTH_VOICE *voice) { (void)voice; abort(); }
void hwDisableIrq(void) {}
void hwEnableIrq(void) {}
void hwInitSampleMem(u32 base, u32 length) { (void)base; (void)length; abort(); }
void hwExitSampleMem(void) { abort(); }
void hwRemoveSample(void *header, void *data) {
    (void)header;
    removed_address = data;
    ++removals;
}
void hwSaveSample(void *header, void *data) {
    (void)header;
    registered_address = *(void **)data;
}
int main(int argc, char **argv) {
    uint8_t header[16], *group, *samples;
    size_t group_bytes, sample_bytes, remaining, i;
    unsigned id;
    SDIR_DATA *directory;
    SAMPLE_INFO resolved = {0};
    PartyboardMsmSample source;
    PartyboardMusyxGroup converted = {0};
    (void)argc; (void)argv;
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    if (fread(header, 1, sizeof(header), stdin) != sizeof(header)) return 2;
    group_bytes = PartyboardMsmU32(header);
    sample_bytes = PartyboardMsmU32(header + 4);
    id = PartyboardMsmU32(header + 8);
    remaining = PartyboardMsmU32(header + 12);
    if (!group_bytes || group_bytes > 64 * 1024 * 1024 || !sample_bytes
            || sample_bytes > 64 * 1024 * 1024 || id >= 65535) return 2;
    group = (uint8_t *)malloc(group_bytes);
    samples = (uint8_t *)malloc(sample_bytes);
    if (!group || !samples) return 3;
    if (fread(group, 1, group_bytes, stdin) != group_bytes
            || fread(samples, 1, sample_bytes, stdin) != sample_bytes) return 4;
    if (!PartyboardMusyxGroupConvert(&converted, group, group_bytes, samples, sample_bytes)) return 5;
    /* Refuse replacing an owned conversion and preserve its allocation. */
    if (PartyboardMusyxGroupConvert(&converted, group, group_bytes, samples, sample_bytes)) return 12;
    directory = converted.directory;
    if (!directory || !dataInsertSDir(directory, samples)
            || !dataAddSampleReference((u16)id) || dataGetSample((u16)id, &resolved)) return 5;
    for (i = 0; directory[i].id != 65535 && directory[i].id != id; ++i) {}
    if (directory[i].id != id || resolved.addr != samples + directory[i].offset
            || registered_address != resolved.addr
            || !PartyboardMusyxSampleSourceOpen(&source, &resolved, sample_bytes - directory[i].offset)) return 6;
    /* A live reference must prevent freeing its directory. */
    if (dataRemoveSDir(directory) || removals) return 10;
    while (remaining) {
        int16_t pcm[257];
        uint8_t output[514];
        size_t got = PartyboardMsmSampleRead(&source, pcm, remaining > 257 ? 257 : remaining);
        if (!got) return 7;
        for (i = 0; i < got; ++i) {
            output[i * 2] = (uint8_t)pcm[i];
            output[i * 2 + 1] = (uint8_t)((uint16_t)pcm[i] >> 8);
        }
        if (fwrite(output, 2, got, stdout) != got) return 8;
        remaining -= got;
    }
    if (!dataRemoveSampleReference((u16)id) || removals != 1
            || removed_address != registered_address || !dataRemoveSDir(directory)
            || dataRemoveSDir(directory) || dataRemoveSampleReference((u16)id)) return 11;
    PartyboardMusyxGroupRelease(&converted);
    PartyboardMusyxGroupRelease(&converted); /* empty cleanup is idempotent */
    free(samples);
    free(group);
    return fflush(stdout) ? 9 : 0;
}
