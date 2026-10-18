#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../src/port/musyx_arrangement_layout.h"
int main(void) {
    uint8_t source[12]={0x12,0x34,0x56,0x78,0xff,0xfe,60,100,1,2,3,4};
    uint8_t map[12]={0},saved[12],result[12]={0};
    uint32_t word;
    uint16_t half;
    assert(PartyboardMusyxLayoutMark(map,12,0,4,4));
    assert(PartyboardMusyxLayoutMark(map,12,4,2,2));
    assert(PartyboardMusyxLayoutMark(map,12,6,6,1));
    memcpy(saved,map,12);
    assert(PartyboardMusyxLayoutMark(map,12,0,4,4));
    assert(!PartyboardMusyxLayoutMark(map,12,0,4,2));
    assert(!PartyboardMusyxLayoutMark(map,12,3,4,4));
    assert(memcmp(map,saved,12)==0);
    assert(PartyboardMusyxLayoutApply(result,source,map,12));
    memcpy(&word,result,4);memcpy(&half,result+4,2);
    assert(word==0x12345678 && half==65534 && !memcmp(result+6,source+6,6));
    memcpy(saved,result,12);map[11]=4;
    assert(!PartyboardMusyxLayoutApply(result,source,map,12));
    assert(memcmp(result,saved,12)==0);
    assert(!PartyboardMusyxLayoutMark(map,12,11,2,2));
    puts("Sequence layout conflict and endian conversion PASS");return 0;
}
