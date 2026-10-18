#ifndef PARTYBOARD_MUSYX_POOL_H
#define PARTYBOARD_MUSYX_POOL_H
#include <stdlib.h>
#include <string.h>
#include "msm_dsp_sample.h"
static inline void PartyboardMusyxPoolU32(uint8_t *destination, const uint8_t *source) {
    uint32_t value = PartyboardMsmU32(source);
    memcpy(destination, &value, sizeof(value));
}
static inline void PartyboardMusyxPoolU16(uint8_t *destination, const uint8_t *source) {
    uint16_t value = (uint16_t)PartyboardMsmU16(source);
    memcpy(destination, &value, sizeof(value));
}
/* Owned host-endian pool copy. Offsets retain their original relative bases.
 * Macro pairs are two u32 words; byte curves and byte keymap/layer fields
 * remain untouched. Validate all chains and typed payloads before use. */
static inline uint8_t *PartyboardMusyxPoolConvert(const uint8_t *pool, size_t bytes) {
    uint8_t *result;
    unsigned kind;
    if (!pool || bytes < 16) return NULL;
    result = (uint8_t *)malloc(bytes);
    if (!result) return NULL;
    memcpy(result, pool, bytes);
    for (kind = 0; kind < 4; ++kind) {
        size_t cursor = PartyboardMsmU32(pool + kind * 4);
        PartyboardMusyxPoolU32(result + kind * 4, pool + kind * 4);
        if (!cursor) continue; /* Absent pool type: no reference list requests it. */
        for (;;) {
            size_t next, span, i;
            if (cursor < 16 || cursor % 4 || cursor > bytes || bytes - cursor < 4) goto failure;
            next = PartyboardMsmU32(pool + cursor);
            PartyboardMusyxPoolU32(result + cursor, pool + cursor);
            if (next == UINT32_MAX) break;
            if (next < 8 || next > bytes - cursor || bytes - cursor - next < 4) goto failure;
            span = next - 8;
            PartyboardMusyxPoolU16(result + cursor + 4, pool + cursor + 4);
            PartyboardMusyxPoolU16(result + cursor + 6, pool + cursor + 6);
            if (kind == 0) {
                if (span % 8) goto failure;
                for (i = 0; i < span; i += 4)
                    PartyboardMusyxPoolU32(result + cursor + 8 + i, pool + cursor + 8 + i);
            } else if (kind == 2) {
                if (span < 128 * 8) goto failure;
                for (i = 0; i < 128; ++i) {
                    size_t position = cursor + 8 + i * 8;
                    PartyboardMusyxPoolU16(result + position, pool + position);
                    PartyboardMusyxPoolU16(result + position + 4, pool + position + 4);
                }
            } else if (kind == 3) {
                size_t count;
                if (span < 4) goto failure;
                count = PartyboardMsmU32(pool + cursor + 8);
                if (count > (span - 4) / 12) goto failure;
                PartyboardMusyxPoolU32(result + cursor + 8, pool + cursor + 8);
                for (i = 0; i < count; ++i) {
                    size_t position = cursor + 12 + i * 12;
                    PartyboardMusyxPoolU16(result + position, pool + position);
                    PartyboardMusyxPoolU16(result + position + 6, pool + position + 6);
                }
            }
            cursor += next;
        }
    }
    return result;
failure:
    free(result);
    return NULL;
}
#endif
