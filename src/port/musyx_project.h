#ifndef PARTYBOARD_MUSYX_PROJECT_H
#define PARTYBOARD_MUSYX_PROJECT_H
#include "musyx_pool.h"
static inline int PartyboardMusyxProjectRefs(uint8_t *out, const uint8_t *input,
        size_t bytes, size_t cursor) {
    if (cursor % 2) return 0;
    for (;;) {
        unsigned id;
        if (cursor > bytes || bytes - cursor < 2) return 0;
        id = PartyboardMsmU16(input + cursor);
        PartyboardMusyxPoolU16(out + cursor, input + cursor);
        cursor += 2;
        if (id == 65535) return 1;
        if (id & 0x8000) {
            unsigned end;
            if (bytes - cursor < 2) return 0;
            end = PartyboardMsmU16(input + cursor);
            if (end < (id & 0x3fff) || end >= 0x8000) return 0;
            PartyboardMusyxPoolU16(out + cursor, input + cursor);
            cursor += 2;
        }
    }
}
/* Owned host-endian project copy, including effects and song setup tables.
 * Arrangement/song event payload conversion is a separate requirement. */
static inline uint8_t *PartyboardMusyxProjectConvert(const uint8_t *input, size_t bytes) {
    uint8_t *out;
    size_t cursor = 0;
    if (!input || bytes < 4) return NULL;
    out = (uint8_t *)malloc(bytes);
    if (!out) return NULL;
    memcpy(out, input, bytes);
    for (;;) {
        size_t next, i, table;
        unsigned type;
        if (cursor % 4 || cursor > bytes || bytes - cursor < 4) goto failure;
        next = PartyboardMsmU32(input + cursor);
        PartyboardMusyxPoolU32(out + cursor, input + cursor);
        if (next == UINT32_MAX) return out;
        if (bytes - cursor < 40 || next < cursor + 40 || next > bytes - 4) goto failure;
        type = PartyboardMsmU16(input + cursor + 6);
        if (type > 1) goto failure;
        PartyboardMusyxPoolU16(out + cursor + 4, input + cursor + 4);
        PartyboardMusyxPoolU16(out + cursor + 6, input + cursor + 6);
        for (i = 8; i < 40; i += 4)
            PartyboardMusyxPoolU32(out + cursor + i, input + cursor + i);
        for (i = 8; i <= 24; i += 4)
            if (!PartyboardMusyxProjectRefs(out, input, bytes, PartyboardMsmU32(input + cursor + i))) goto failure;
        table = PartyboardMsmU32(input + cursor + 28);
        if (type == 1) {
            size_t count;
            if (table % 2 || table > bytes || bytes - table < 4) goto failure;
            count = PartyboardMsmU16(input + table);
            if (count > (bytes - table - 4) / 10) goto failure;
            PartyboardMusyxPoolU16(out + table, input + table);
            PartyboardMusyxPoolU16(out + table + 2, input + table + 2);
            for (i = 0; i < count; ++i) {
                size_t entry = table + 4 + i * 10;
                PartyboardMusyxPoolU16(out + entry, input + entry);
                PartyboardMusyxPoolU16(out + entry + 2, input + entry + 2);
            }
        } else {
            unsigned page;
            for (page = 0; page < 2; ++page) {
                size_t position = PartyboardMsmU32(input + cursor + 28 + page * 4), count = 0;
                for (;;) {
                    if (position % 2 || position > bytes || bytes - position < 6) goto failure;
                    PartyboardMusyxPoolU16(out + position, input + position);
                    if (input[position + 4] == 255) break;
                    if (input[position + 4] >= 128 || ++count > 128) goto failure;
                    position += 6;
                }
            }
            table = PartyboardMsmU32(input + cursor + 36);
            for (;;) {
                if (table % 2 || table > bytes || bytes - table < 2) goto failure;
                PartyboardMusyxPoolU16(out + table, input + table);
                if (PartyboardMsmU16(input + table) == 65535) break;
                if (bytes - table < 84) goto failure;
                PartyboardMusyxPoolU16(out + table + 2, input + table + 2);
                table += 84;
            }
        }
        cursor = next;
    }
failure:
    free(out);
    return NULL;
}
#endif
