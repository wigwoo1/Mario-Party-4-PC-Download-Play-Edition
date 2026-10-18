#include <assert.h>
#include <stdio.h>
#include "../src/port/src_resampler.h"
#include "../src/port/msm_sample_resampler.h"
typedef struct {unsigned cursor,limit;} Input;
static int read_sample(void *context,int16_t *sample) {
    Input *input=context;
    if(input->cursor==input->limit) return 0;
    *sample=(int16_t)(++input->cursor*100);
    return 1;
}
int main(void) {
    int16_t bank[512]={0},whole[32]={0},split[32]={0},sample;
    PartyboardSrcResampler a,b;
    Input x={0,100},y={0,100};
    unsigned i;
    /* Synthetic newest-tap bank tests state flow, not original ROM fidelity. */
    for(i=0;i<128;++i) bank[i*4+3]=32767;
    assert(PartyboardSrcResamplerInit(&a,32768,bank,512,read_sample,&x));
    assert(PartyboardSrcResamplerInit(&b,32768,bank,512,read_sample,&y));
    assert(PartyboardSrcResamplerRead(&a,whole,32)==32);
    assert(PartyboardSrcResamplerRead(&b,split,7)==7);
    assert(PartyboardSrcResamplerRead(&b,split+7,25)==25);
    assert(!memcmp(whole,split,sizeof whole));
    assert(x.cursor==16 && y.cursor==16 && a.phase.fraction==0);
    assert(whole[0]==0 && whole[1]==99 && whole[2]==99);
    a.phase.ratio=65536;
    assert(PartyboardSrcResamplerRead(&a,&sample,1)==1 && sample==1699);
    /* Preserve fractional position across a live pitch change. */
    b.phase.ratio=16384;
    assert(PartyboardSrcResamplerRead(&b,&sample,1)==1);
    b.phase.ratio=49152;
    assert(PartyboardSrcResamplerRead(&b,&sample,1)==1);
    assert(b.phase.fraction==0 && y.cursor==17 && sample==1699);
    x.cursor=0;x.limit=2;
    assert(PartyboardSrcResamplerInit(&a,65536,bank,512,read_sample,&x));
    assert(PartyboardSrcResamplerRead(&a,whole,32)==2 && a.ended);
    assert(PartyboardSrcResamplerRead(&a,whole,1)==0);
    {
        int16_t tail_bank[512]={0};
        for(i=0;i<128;++i) tail_bank[i*4]=32767;
        x.cursor=0;x.limit=2;
        assert(PartyboardSrcResamplerInit(&a,65536,tail_bank,512,read_sample,&x));
        assert(!PartyboardSrcResamplerReadStoppedPeriod(&a,whole,33));
        assert(!x.cursor && !a.ended);
        assert(PartyboardSrcResamplerReadStoppedPeriod(&a,whole,32)==32);
        assert(a.ended && x.cursor==2 && !a.phase.fraction);
        assert(!whole[0] && !whole[1] && !whole[2]);
        assert(whole[3]==99 && whole[4]==199 && !whole[5]);
        for(i=6;i<32;++i) assert(!whole[i]);
        assert(!a.history[0] && !a.history[1] && !a.history[2] && !a.history[3]);
        assert(!PartyboardSrcResamplerReadStoppedPeriod(&a,whole,32));
    }
    x.cursor=0;x.limit=100;
    {
        PartyboardSrcResampler direct={0};
        Input input={0,100};
        direct.read=read_sample;direct.context=&input;
        direct.phase.ratio=1;direct.phase.fraction=12345;
        assert(!PartyboardSrcResamplerReadBypassPeriod(&direct,whole,33));
        assert(!input.cursor && direct.phase.fraction==12345);
        assert(PartyboardSrcResamplerReadBypassPeriod(&direct,whole,32)==32);
        assert(input.cursor==32 && !direct.phase.fraction && direct.phase.ratio==1);
        for(i=0;i<32;++i) assert(whole[i]==(int16_t)((i+1)*100));
        for(i=0;i<4;++i) assert(direct.history[i]==(int16_t)((29+i)*100));
        input.cursor=0;input.limit=2;direct.ended=0;
        assert(PartyboardSrcResamplerReadBypassPeriod(&direct,whole,32)==32);
        assert(whole[0]==100 && whole[1]==200 && direct.ended && input.cursor==2);
        for(i=2;i<32;++i) assert(!whole[i]);
        for(i=0;i<4;++i) assert(!direct.history[i]);
        assert(!PartyboardSrcResamplerReadBypassPeriod(&direct,whole,32));
        input.cursor=0;input.limit=100;direct.ended=0;direct.mode=2;
        assert(PartyboardSrcResamplerRead(&direct,whole,32)==32);
        assert(whole[0]==100 && whole[31]==3200 && input.cursor==32);
        assert(PartyboardSrcResamplerReadStoppedPeriod(&direct,whole,32)==32);
        assert(whole[0]==3300 && whole[31]==6400 && input.cursor==64);
        direct.mode=1;
        assert(!PartyboardSrcResamplerRead(&direct,whole,32) && input.cursor==64);
    }
    {
        uint8_t pcm[64]={0};
        PartyboardMsmSample source={0};
        unsigned lengths[]={32,16,24},ratios[]={65536,32768,49152};
        pcm[63]=100;
        source.format=2;
        for(i=0;i<3;++i) {
            assert(PartyboardPcm16SampleInit(&source.source.pcm,pcm,sizeof pcm,
                lengths[i],0,0,0));
            assert(PartyboardMsmSampleResamplerInit(&a,&source,ratios[i],bank,512));
            assert(PartyboardSrcResamplerReadStoppedPeriod(&a,whole,32)==32);
            assert(a.ended && source.source.pcm.position==lengths[i]);
            if(i==0) assert(whole[31]==99);
            assert(!PartyboardSrcResamplerReadStoppedPeriod(&a,whole,32));
        }
    }
    assert(PartyboardSrcResamplerInit(&a,0x3fff*16,bank,512,read_sample,&x));
    assert(PartyboardSrcResamplerRead(&a,whole,2)==2 && x.cursor==7);
    assert(!PartyboardSrcResamplerInit(&a,0x4000*16,bank,512,read_sample,&x));
    {
        uint8_t pcm[]={0,100,0,200,0,250};
        uint8_t adpcm[]={0,0x12,0x34,0x56,0x70,0x12,0x34,0x56};
        PartyboardMsmSample source={0};
        PartyboardDspAdpcm decoder={0};
        source.format=2;
        assert(PartyboardPcm16SampleInit(&source.source.pcm,pcm,sizeof pcm,3,0,1,2));
        assert(PartyboardMsmSampleResamplerInit(&a,&source,65536,bank,512));
        assert(PartyboardSrcResamplerRead(&a,whole,8)==8);
        assert(whole[0]==99 && whole[1]==199 && whole[2]==249
            && whole[3]==199 && whole[7]==199 && !a.ended);
        source.format=0;
        assert(PartyboardDspAdpcmSampleInit(&source.source.dsp,&decoder,adpcm,
            sizeof adpcm,14,0,0,0,0,0,0));
        assert(PartyboardMsmSampleResamplerInit(&a,&source,65536,bank,512));
        assert(PartyboardSrcResamplerRead(&a,whole,32)==14 && a.ended);
        assert(whole[0]==0 && whole[1]==1 && whole[6]==6);
        assert(PartyboardDspAdpcmSampleInit(&source.source.dsp,&decoder,adpcm,
            sizeof adpcm,14,0,0,0,0,0,0));
        assert(PartyboardMsmSampleResamplerInit(&a,&source,28672,bank,512));
        assert(PartyboardSrcResamplerReadStoppedPeriod(&a,whole,32)==32);
        assert(a.ended && source.source.dsp.position==14 && !a.phase.fraction);
        assert(!PartyboardSrcResamplerReadStoppedPeriod(&a,whole,32));
        /* Ending exactly at a loop boundary must remain resumable. */
        assert(PartyboardDspAdpcmSampleInit(&source.source.dsp,&decoder,adpcm,
            sizeof adpcm,14,0,0,14,0,0,0));
        assert(PartyboardMsmSampleResamplerInit(&a,&source,28672,bank,512));
        assert(PartyboardSrcResamplerReadStoppedPeriod(&a,whole,32)==32 && !a.ended);
        assert(PartyboardSrcResamplerReadStoppedPeriod(&a,split,32)==32 && !a.ended);
        assert(source.source.dsp.position==14 && !a.phase.fraction);
        /* Initial silent history differs from history carried across a loop. */
        assert(!memcmp(whole+3,split+3,29*sizeof *whole));
        source.format=7;
        assert(!PartyboardMsmSampleResamplerInit(&a,&source,65536,bank,512));
    }
    puts("streaming SRC chunk/pitch/history/end regression PASS");
    return 0;
}
