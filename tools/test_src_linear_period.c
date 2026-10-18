#include <assert.h>
#include <stdio.h>
#include "../src/port/src_linear_period.h"
typedef struct { unsigned cursor,limit; } Input;
static int read_sample(void *context,int16_t *sample) {
    Input *input=context;
    if(input->cursor==input->limit) return 0;
    *sample=(int16_t)(++input->cursor*100);return 1;
}
static int at_end(void *context) {
    Input *input=context;return input->cursor==input->limit;
}
int main(void) {
    PartyboardSrcResampler source={0};Input input={0,200};int16_t output[32];unsigned i;
    source.read=read_sample;source.context=&input;source.phase.ratio=65536;
    assert(PartyboardSrcReadLinearPeriod(&source,output,0xce0,0xce0)==32);
    for(i=0;i<32;++i) assert(output[i]==(int16_t)(i*100));
    assert(input.cursor==32 && source.history[0]==2900 && source.history[3]==3200);
    source.phase.ratio=0;
    assert(PartyboardSrcReadLinearPeriod(&source,output,0xd00,0xd20)==32);
    for(i=0;i<32;++i) assert(output[i]==3100);
    assert(input.cursor==32 && source.history[0]==0xd00 && source.history[1]==0xd20);
    assert(source.history[2]==3100 && source.history[3]==3200);
    memset(source.history,0,sizeof source.history);input.cursor=0;
    source.phase.ratio=32768;
    assert(PartyboardSrcReadLinearPeriod(&source,output,0xce0,0xce0)==32);
    for(i=0;i<32;++i) assert(output[i]==(i<2?0:(int16_t)((i-1)*50)));
    assert(input.cursor==16 && !source.phase.fraction);
    memset(source.history,0,sizeof source.history);input.cursor=0;input.limit=16;
    source.at_end=at_end;
    assert(PartyboardSrcReadLinearPeriod(&source,output,0xce0,0xce0)==32);
    assert(source.ended && input.cursor==16 && output[31]==1500);
    assert(!PartyboardSrcReadLinearPeriod(&source,output,0xce0,0xce0));
    source.ended=0;
    memset(source.history,0,sizeof source.history);input.cursor=0;input.limit=2;
    source.phase.ratio=65536;
    assert(PartyboardSrcReadLinearPeriod(&source,output,0xce0,0xce0)==32);
    assert(output[0]==0 && output[1]==100 && output[2]==200 && source.ended);
    for(i=3;i<32;++i) assert(!output[i]);
    assert(input.cursor==2 && !PartyboardSrcReadLinearPeriod(&source,output,0xce0,0xce0));
    memset(source.history,0,sizeof source.history);input.cursor=0;input.limit=200;source.ended=0;
    source.phase.ratio=0x3fff*16;source.phase.fraction=65535;
    assert(PartyboardSrcReadLinearPeriod(&source,output,0xce0,0xce0)==32);
    assert(input.cursor==128 && source.phase.fraction==65023 && output[31]==12799);
    assert(source.history[0]==12500 && source.history[3]==12800);
    source.phase.ratio=0x40000;
    assert(!PartyboardSrcReadLinearPeriod(&source,output,0xce0,0xce0) && input.cursor==128);
    puts("Mode1 period unity/zero-pitch/scratch history regression PASS");return 0;
}
