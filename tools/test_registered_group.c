/* Reuse private sample registration fixtures; its decoder main is not called. */
#define main unused_sample_decoder_main
#include "decode_registered_sample.c"
#undef main
#include "../extern/musyx/src/musyx/runtime/s_data.c"
#include "../src/port/musyx_sample_memory.h"
static PartyboardMusyxSampleMemory sample_memory;
u8 sndActive = 1;
void *hwTransAddr(void *samples) { return samples; }
void hwSyncSampleMem(void) {}
void hwSetSaveSampleCallback(ARAMUploadCallback callback, unsigned long size) { (void)callback; (void)size; abort(); }
static unsigned killed;
void synthKillVoicesByMacroReferences(u16 *ref) { (void)ref; ++killed; }
void s3dKillEmitterByFXID(FX_TAB *fx, u32 count) { (void)fx; (void)count; }
void seqKillInstancesByGroupID(SND_GROUPID id) { (void)id; }
u32 seqStartPlay(PAGE *norm, PAGE *drum, MIDISETUP *setup, u32 *song,
        SND_PLAYPARA *parameters, u8 studio, u16 id) {
    (void)norm; (void)drum; (void)setup; (void)song;
    (void)parameters; (void)studio; (void)id; abort();
}
static unsigned verify_registered_lookups(void) {
    unsigned i, j, total = 0;
    for (i = 0; i < dataSmpSDirNum; ++i) {
        SDIR_DATA *entry;
        for (entry = dataSmpSDirs[i].data; entry->id != 65535; ++entry) {
            SAMPLE_INFO info = {0};
            PartyboardMsmSample source;
            size_t available;
            if (!entry->ref_cnt || entry->ref_cnt == 65535) continue;
            if (dataGetSample(entry->id, &info)) abort();
            available = PartyboardMusyxSampleMemoryAvailable(&sample_memory, info.addr);
            if (!available || !PartyboardMusyxSampleSourceOpen(&source, &info, available)) abort();
            ++total;
        }
    }
    for (i = 0; i < dataMacTotal; ++i) {
        if (dataGetMacro(dataMacSubTabmem[i].id) != dataMacSubTabmem[i].data) {
            fprintf(stderr, "macro lookup mismatch id=%u\n", dataMacSubTabmem[i].id); abort();
        }
        ++total;
    }
    for (i = 0; i < dataCurveNum; ++i) {
        if (dataGetCurve(dataCurveTab[i].id) != dataCurveTab[i].data) {
            fprintf(stderr, "curve lookup mismatch id=%u\n", dataCurveTab[i].id); abort();
        }
        ++total;
    }
    for (i = 0; i < dataKeymapNum; ++i) {
        if (dataGetKeymap(dataKeymapTab[i].id) != dataKeymapTab[i].data) {
            fprintf(stderr, "keymap lookup mismatch id=%u\n", dataKeymapTab[i].id); abort();
        }
        ++total;
    }
    for (i = 0; i < dataLayerNum; ++i) {
        u16 length = 0;
        if (dataGetLayer(dataLayerTab[i].id, &length) != dataLayerTab[i].data
                || length != dataLayerTab[i].num) {
            fprintf(stderr, "layer lookup mismatch id=%u\n", dataLayerTab[i].id); abort();
        }
        ++total;
    }
    for (i = 0; i < dataFXGroupNum; ++i) {
        for (j = 0; j < dataFXGroups[i].fxNum; ++j) {
            unsigned g, entry;
            FX_TAB *expected = NULL;
            u16 id = dataFXGroups[i].fxTab[j].id;
            /* Independent linear reference preserves original group priority. */
            for (g = 0; g <= i && !expected; ++g)
                for (entry = 0; entry < dataFXGroups[g].fxNum; ++entry)
                    if (dataFXGroups[g].fxTab[entry].id == id) {
                        expected = &dataFXGroups[g].fxTab[entry]; break;
                    }
            if (dataGetFX(id) != expected) { fprintf(stderr, "effect lookup mismatch id=%u\n", id); abort(); }
            ++total;
        }
    }
    return total;
}
int main(void) {
    uint8_t header[8], *raw, *samples[16] = {0};
    size_t bytes, sample_bytes;
    PartyboardMusyxGroup converted[16] = {0};
    GROUP_DATA *project;
    unsigned macro_count, sample_count, effect_count, count, index, lookups;
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _set_error_mode(_OUT_TO_STDERR);
#endif
    if (fread(header, 1, 4, stdin) != 4) return 2;
    count = PartyboardMsmU32(header);
    if (!count || count > 16) return 2;
    dataInitStack();
    for (index = 0; index < count; ++index) {
    if (fread(header, 1, 8, stdin) != 8) return 2;
    bytes = PartyboardMsmU32(header); sample_bytes = PartyboardMsmU32(header + 4);
    if (!bytes || bytes > 64 * 1024 * 1024 || !sample_bytes || sample_bytes > 64 * 1024 * 1024) return 2;
    raw = (uint8_t *)malloc(bytes); samples[index] = (uint8_t *)malloc(sample_bytes);
    if (!raw || !samples[index]) return 3;
    if (fread(raw, 1, bytes, stdin) != bytes || fread(samples[index], 1, sample_bytes, stdin) != sample_bytes) return 4;
    if (!PartyboardMusyxGroupConvert(&converted[index], raw, bytes, samples[index], sample_bytes)) return 5;
    if (!PartyboardMusyxSampleMemoryAdd(&sample_memory, samples[index], sample_bytes)) return 9;
    free(raw);
    project = (GROUP_DATA *)converted[index].project;
    fprintf(stderr, "group=%u push\n", project->id); fflush(stderr);
    if (!sndPushGroup(project, project->id, samples[index], converted[index].directory, converted[index].pool)) return 6;
    }
    macro_count = dataMacTotal; sample_count = dataSmpSDirNum; effect_count = dataFXGroupNum;
    fprintf(stderr, "group=%u pushed macros=%u samples=%u pop\n", project->id, macro_count, sample_count); fflush(stderr);
    if (!macro_count || sample_count != count || SP_CURRENT != count) return 7;
    lookups = verify_registered_lookups();
    for (index = count; index; --index) {
        if (!sndPopGroup() || SP_CURRENT != index - 1) return 8;
        if (!PartyboardMusyxSampleMemoryRemove(&sample_memory, samples[index - 1])
                || PartyboardMusyxSampleMemoryAvailable(&sample_memory, samples[index - 1])) return 9;
        PartyboardMusyxGroupRelease(&converted[index - 1]); free(samples[index - 1]);
    }
    if (killed != count || SP_CURRENT || dataMacTotal || dataSmpSDirNum
            || dataFXGroupNum || dataCurveNum || dataKeymapNum || dataLayerNum) return 8;
    printf("%u %u %u %u %u\n", macro_count, sample_count, effect_count, removals, lookups);
    return 0;
}
