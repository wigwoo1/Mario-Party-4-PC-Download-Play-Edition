#include <assert.h>
#include "../src/port/musyx_pool.h"
static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}
static uint32_t host32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
int main(void) {
    uint8_t pool[1080] = {0}, *converted;
    uint16_t value;
    put32(pool, 16); put32(pool + 4, 32); put32(pool + 8, 36); put32(pool + 12, 1072);
    put32(pool + 16, 16); pool[21] = 3;
    put32(pool + 24, 0x12345678); put32(pool + 28, 0xabcdef01);
    put32(pool + 32, UINT32_MAX);
    put32(pool + 36, 1032);
    pool[44] = 0x12; pool[45] = 0x34;
    pool[48] = 0xfe; pool[49] = 0xdc; /* keymap signed priority */
    pool[50] = 0x56; pool[51] = 0x78; /* reserved bytes unchanged */
    put32(pool + 1068, UINT32_MAX); put32(pool + 1072, UINT32_MAX);
    converted = PartyboardMusyxPoolConvert(pool, sizeof(pool));
    assert(converted && host32(converted + 24) == 0x12345678 && host32(converted + 28) == 0xabcdef01);
    memcpy(&value, converted + 44, 2); assert(value == 0x1234);
    memcpy(&value, converted + 48, 2); assert(value == 0xfedc);
    assert(converted[50] == 0x56 && converted[51] == 0x78 && pool[24] == 0x12);
    free(converted);
    put32(pool + 4, 0); /* absent curve pool must not traverse the pool header */
    converted = PartyboardMusyxPoolConvert(pool, sizeof(pool));
    assert(converted && host32(converted) == 16 && host32(converted + 4) == 0);
    free(converted);
    assert(!PartyboardMusyxPoolConvert(pool, 1075));
    put32(pool + 36, 8); /* truncated keymap payload */
    assert(!PartyboardMusyxPoolConvert(pool, sizeof(pool)));
    return 0;
}
