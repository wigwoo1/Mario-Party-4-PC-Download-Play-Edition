#include <assert.h>
#include "../src/port/musyx_project.h"
static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}
int main(void) {
    uint8_t input[64] = {0}, *out;
    uint16_t value;
    size_t i;
    put32(input, 40); input[5] = 9; input[7] = 1;
    for (i = 8; i <= 24; i += 4) put32(input + i, 44);
    put32(input + 28, 48); put32(input + 40, UINT32_MAX);
    input[44] = input[45] = 255;
    input[49] = 1;
    input[52] = 0x12; input[53] = 0x34;
    input[54] = 0xab; input[55] = 0xcd; input[56] = 7;
    out = PartyboardMusyxProjectConvert(input, sizeof(input));
    assert(out);
    memcpy(&value, out + 52, 2); assert(value == 0x1234);
    memcpy(&value, out + 54, 2); assert(value == 0xabcd);
    assert(out[56] == 7 && input[52] == 0x12);
    free(out);
    assert(!PartyboardMusyxProjectConvert(input, 61));
    put32(input, 0); assert(!PartyboardMusyxProjectConvert(input, sizeof(input)));
    return 0;
}
