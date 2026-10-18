#include <assert.h>
#include <string.h>
#include <stdint.h>
#include "../extern/musyx/src/musyx/runtime/synthdata.c"
#include "../extern/musyx/src/musyx/runtime/snd_service.c"
#include "../src/port/musyx_sample_source.h"
#include "../src/port/musyx_sample_directory.h"
static void *saved_address;
u32 synthGetTicksPerSecond(SYNTH_VOICE *voice) { (void)voice; assert(0); return 0; }
void hwDisableIrq(void) {}
void hwEnableIrq(void) {}
void hwInitSampleMem(u32 base, u32 length) { (void)base; (void)length; assert(0); }
void hwExitSampleMem(void) { assert(0); }
void hwRemoveSample(void *header, void *data
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
    , ARAMInfo *aram
#endif
) {
    (void)header; (void)data;
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
    (void)aram;
#endif
    assert(0);
}
void hwSaveSample(void *header, void *data
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
    , ARAMInfo *aram
#endif
) {
    (void)header;
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
    (void)aram;
#endif
    saved_address = *(void **)data;
}
int main(void) {
    SDIR_DATA directory[2] = {0};
    uint8_t buffer[128];
    bool added;
    SAMPLE_INFO info = {0};
    directory[0].id = 7;
    directory[0].offset = 33;
    directory[0].header.info = (60u << 24) | 32000;
    directory[0].header.length = 14;
    directory[0].extraData = 64;
    directory[1].id = 65535;
#if UINTPTR_MAX > UINT32_MAX
    assert((uintptr_t)buffer > UINT32_MAX);
#endif
    assert(dataInsertSDir(directory, buffer));
    added = dataAddSampleReference(7
#if MUSY_VERSION >= MUSY_VERSION_CHECK(2, 0, 1)
        , NULL
#endif
    );
    assert(added);
    assert(saved_address == buffer + 33);
    assert(directory[0].addr == buffer + 33);
    assert(dataGetSample(7, &info) == 0);
    assert(info.addr == buffer + 33 && info.length == 14 && info.compType == 0);
    assert(info.extraData == (uint8_t *)directory + 64);
    directory[0].extraData = 0;
    assert(dataGetSample(7, &info) == 0 && info.extraData == NULL);
    {
        PartyboardMsmSample source;
        SNDADPCMinfo parameters = {0};
        int16_t pcm[16];
        buffer[33] = 0; buffer[34] = 0x12;
        info.extraData = &parameters;
        assert(PartyboardMusyxSampleSourceOpen(&source, &info, 8));
        assert(PartyboardMsmSampleRead(&source, pcm, 2) == 2 && pcm[0] == 1 && pcm[1] == 2);
        info.compType = 2; info.length = 2; info.offset = 1;
        buffer[33] = 0xff; buffer[34] = 0xff; buffer[35] = 0; buffer[36] = 7;
        assert(PartyboardMusyxSampleSourceOpen(&source, &info, 4));
        assert(PartyboardMsmSampleRead(&source, pcm, 16) == 1 && pcm[0] == 7);
        {
            PartyboardMsmSample previous = source;
            assert(!PartyboardMusyxSampleSourceOpen(&source, &info, 3));
            assert(memcmp(&source, &previous, sizeof(source)) == 0);
            info.compType = 1;
            assert(!PartyboardMusyxSampleSourceOpen(&source, &info, 4));
            assert(memcmp(&source, &previous, sizeof(source)) == 0);
        }
    }
    {
        uint8_t group[120] = {0}, encoded[8] = {0, 0x12};
        SDIR_DATA *converted;
        SAMPLE_INFO resolved = {0};
        PartyboardMsmSample source;
        int16_t pcm[2];
        group[11] = 16;
        group[17] = 8;
        group[30] = 0x7d; /* 32000 Hz */
        group[35] = 14;
        group[47] = 64; /* original directory-relative parameters */
        group[48] = 0xff; group[49] = 0xff;
        group[81] = 8;
        converted = PartyboardMusyxSampleDirectoryConvert(group, sizeof(group), encoded, sizeof(encoded));
        assert(converted && converted[0].id == 8 && converted[1].id == 65535);
        assert(dataInsertSDir(converted, encoded));
        assert(dataAddSampleReference(8));
        assert(dataGetSample(8, &resolved) == 0 && resolved.addr == encoded);
        assert(resolved.extraData == (uint8_t *)converted + converted[0].extraData);
        assert(((SNDADPCMinfo *)resolved.extraData)->numCoef == 8);
        assert(PartyboardMusyxSampleSourceOpen(&source, &resolved, sizeof(encoded)));
        assert(PartyboardMsmSampleRead(&source, pcm, 2) == 2 && pcm[0] == 1 && pcm[1] == 2);
        /* Keep registered directory alive until process teardown. */
        assert(!PartyboardMusyxSampleDirectoryConvert(group, 119, encoded, sizeof(encoded)));
        assert(!PartyboardMusyxSampleDirectoryConvert(group, sizeof(group), encoded, 7));
        group[48] = 0; group[49] = 8; /* duplicate ID invalidates binary-search ordering */
        assert(!PartyboardMusyxSampleDirectoryConvert(group, sizeof(group), encoded, sizeof(encoded)));
    }
    return 0;
}
