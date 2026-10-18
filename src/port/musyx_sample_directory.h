#ifndef PARTYBOARD_MUSYX_SAMPLE_DIRECTORY_H
#define PARTYBOARD_MUSYX_SAMPLE_DIRECTORY_H
#include <stdlib.h>
#include "musyx_sample_source.h"

/* One owned native directory allocation, including relocated host-endian
 * ADPCM parameters. Original group and encoded samples are never modified
 * or freed. Unregister from MusyX before freeing this directory. */
static inline SDIR_DATA *PartyboardMusyxSampleDirectoryConvert(
        const uint8_t *group, size_t group_bytes, const uint8_t *samples,
        size_t sample_bytes) {
    size_t base, cursor, count = 0, entries_bytes, allocation_bytes, i;
    unsigned previous = 0;
    SDIR_DATA *directory;
    if (!group || !samples || group_bytes < 16) return NULL;
    base = PartyboardMsmU32(group + 8);
    if (base > group_bytes) return NULL;
    cursor = base;
    for (;;) {
        unsigned id;
        if (group_bytes - cursor < 2) return NULL;
        id = PartyboardMsmU16(group + cursor);
        if (id == 65535) break;
        if (group_bytes - cursor < 32 || (count && id <= previous)) return NULL;
        previous = id;
        ++count;
        cursor += 32;
    }
    if (count > (SIZE_MAX / (sizeof(SDIR_DATA) + sizeof(SNDADPCMinfo))) - 1) return NULL;
    entries_bytes = (count + 1) * sizeof(SDIR_DATA);
    allocation_bytes = entries_bytes + count * sizeof(SNDADPCMinfo);
    /* Original extraData offsets remain u32 even on native64. */
    if (allocation_bytes > UINT32_MAX) return NULL;
    directory = (SDIR_DATA *)calloc(1, allocation_bytes);
    if (!directory) return NULL;
    for (i = 0; i < count; ++i) {
        const uint8_t *entry = group + base + i * 32;
        SAMPLE_INFO info = {0};
        PartyboardMsmSample validated;
        size_t offset = PartyboardMsmU32(entry + 4);
        uint32_t length = PartyboardMsmU32(entry + 16);
        info.info = PartyboardMsmU32(entry + 12);
        info.length = length & 0xffffff;
        info.compType = (u8)(length >> 24);
        info.loop = PartyboardMsmU32(entry + 20);
        info.loopLength = PartyboardMsmU32(entry + 24);
        if (offset > sample_bytes) goto failure;
        info.addr = (void *)(samples + offset);
        if (info.compType == 0) {
            size_t extra = PartyboardMsmU32(entry + 28);
            const uint8_t *encoded;
            SNDADPCMinfo *params = (SNDADPCMinfo *)((uint8_t *)directory + entries_bytes) + i;
            unsigned j;
            if (!extra || extra > group_bytes - base || group_bytes - base - extra < 40)
                goto failure;
            encoded = group + base + extra;
            params->numCoef = (u16)PartyboardMsmU16(encoded);
            params->initialPS = encoded[2];
            params->loopPS = encoded[3];
            params->loopY0 = PartyboardMsmS16(encoded + 4);
            params->loopY1 = PartyboardMsmS16(encoded + 6);
            for (j = 0; j < 16; ++j)
                params->coefTab[j / 2][j % 2] = PartyboardMsmS16(encoded + 8 + j * 2);
            info.extraData = params;
            directory[i].extraData = (u32)((uint8_t *)params - (uint8_t *)directory);
        }
        if (!PartyboardMusyxSampleSourceOpen(&validated, &info, sample_bytes - offset))
            goto failure;
        directory[i].id = (u16)PartyboardMsmU16(entry);
        directory[i].offset = (u32)offset;
        directory[i].header.info = info.info;
        directory[i].header.length = length;
        directory[i].header.loopOffset = info.loop;
        directory[i].header.loopLength = info.loopLength;
    }
    directory[count].id = 65535;
    return directory;
failure:
    free(directory);
    return NULL;
}
#endif
