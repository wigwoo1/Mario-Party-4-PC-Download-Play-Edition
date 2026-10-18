#include "../src/port/musyx_depop.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    int32_t value,start;
    int16_t delta;
    assert(PartyboardMusyxDepopAdd(0x7ffff0,32767)==0x7fffff);
    assert(PartyboardMusyxDepopAdd(-0x7ffff0,-32768)==-0x7fffff);
    assert(PartyboardMusyxDepopAdd(123,-123)==0);
    for(value=-0x7fffff;value<=0x7fffff;++value) {
        int32_t sum=value;
        assert(PartyboardMusyxDepopFade(&sum,&start,&delta));
        assert(start==value && delta>=-20 && delta<=20);
        assert(sum==start+(int32_t)delta*160);
        assert(abs(sum)<=abs(start));
        if(abs(start)<3200) assert(abs(sum)<160);
        else assert(abs(start)-abs(sum)==3200);
        assert(value<0 ? delta>=0 : delta<=0);
    }
    value=0x800000;start=123;delta=7;
    assert(!PartyboardMusyxDepopFade(&value,&start,&delta));
    assert(value==0x800000 && start==123 && delta==7);
    assert(!PartyboardMusyxDepopFade(&start,&start,&delta));
    puts("MusyX host depop: all 16777215 signed 24-bit sums and bounds PASS");
    return 0;
}
