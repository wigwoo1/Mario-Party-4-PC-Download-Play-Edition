#include <assert.h>
#include <stdio.h>
#include "../src/port/msm_sample_resampler.h"
#include "../src/port/musyx_pcm_voice.h"
#include "../src/port/musyx_pcm_ramp.h"
#include "../src/port/musyx_volume_schedule.h"
int main(void) {
    const uint8_t pcm[]={0x40,0};
    int16_t bank[512]={0},output[96*9];
    int64_t whole[96*9]={0},split[96*9]={0};
    PartyboardMsmSample x={0},y;
    PartyboardSrcResampler a,b;
    PartyboardMusyxPcmEnvelope e={0},f;
    SAL_VOLINFO volume={0};
    DSPvoice voice={0};
    unsigned i;
    for(i=0;i<128;++i) bank[i*4+3]=32767;
    x.format=2;
    assert(PartyboardPcm16SampleInit(&x.source.pcm,pcm,sizeof pcm,1,0,0,1));
    y=x;
    assert(PartyboardMsmSampleResamplerInit(&a,&x,65536,bank,512));
    assert(PartyboardMsmSampleResamplerInit(&b,&y,65536,bank,512));
    e.adsr.mode=0;e.adsr.state=4;e.adsr.cnt=2;
    e.adsr.currentVolume=16384*65536;e.adsr.currentDelta=-8192*65536;
    f=e;
    {
        PartyboardMusyxPcmEnvelope scheduled=e;
        ADSR_VARS expected=e.adsr;
        u16 start,delta;
        int done=adsrHandle(&expected,&start,&delta)!=0;
        assert(PartyboardMusyxPcmEnvelopeBeginPeriod(&scheduled));
        assert(!memcmp(&scheduled.adsr,&expected,sizeof expected));
        assert(scheduled.remaining==32 && scheduled.volume==start);
        assert(scheduled.delta==(int16_t)delta && scheduled.done_after_block==done);
        assert(!PartyboardMusyxPcmEnvelopeBeginPeriod(&scheduled));
        assert(!memcmp(&scheduled.adsr,&expected,sizeof expected));
    }
    volume.volL=1;volume.volAuxAR=0.5f;
    a.phase.ratio=32768;
    voice.changed[0]=8;voice.pitch[0]=65536;
    assert(PartyboardMusyxPcmVoiceRenderPeriod(&a,&e,&voice,0,&volume,whole)==32);
    assert(a.phase.ratio==65536 && voice.changed[0]==8);
    assert(PartyboardMusyxPcmVoiceRender(&a,&e,&volume,whole+32*9,64)==32);
    assert(PartyboardMusyxPcmVoiceRender(&b,&f,&volume,split,7)==7);
    assert(PartyboardMusyxPcmVoiceRenderPeriod(&b,&f,&voice,0,&volume,split)==0);
    assert(PartyboardMusyxPcmVoiceRender(&b,&f,&volume,split+7*9,89)==57);
    assert(!memcmp(whole,split,sizeof whole) && e.finished && f.finished);
    assert(a.history[3]==16384 && b.history[3]==16384);
    PartyboardPcmMixFinish(whole,output,96*9);
    assert(output[0]>8000 && output[63*9]>0 && output[64*9]==0);
    for(i=0;i<96;++i) {
        assert(output[i*9+1]==0 && output[i*9+2]==0);
        assert(output[i*9+4]<=output[i*9]/2);
    }
    {
        int64_t key_bus[160*9]={0};
        PartyboardMusyxPcmEnvelope sustained={0},before;
        PartyboardSrcResampler saved;
        sustained.adsr.mode=0;sustained.adsr.state=3;
        sustained.adsr.currentVolume=0x7fff0000;
        sustained.adsr.data.linear.rTime=4;
        assert(PartyboardMsmSampleResamplerInit(&a,&x,65536,bank,512));
        assert(PartyboardMusyxPcmVoiceRender(&a,&sustained,&volume,key_bus,32)==32);
        voice.changed[2]=0x48;voice.pitch[2]=0x40000;
        before=sustained;saved=a;
        assert(!PartyboardMusyxPcmVoiceRenderPeriod(&a,&sustained,&voice,2,&volume,key_bus));
        assert(!memcmp(&before,&sustained,sizeof before) && !memcmp(&saved,&a,sizeof saved));
        voice.pitch[2]=65536;
        assert(PartyboardMusyxPcmVoiceRenderPeriod(&a,&sustained,&voice,2,&volume,key_bus+32*9)==32);
        assert(sustained.adsr.state==4 && !sustained.finished);
        assert(PartyboardMusyxPcmVoiceRender(&a,&sustained,&volume,key_bus+64*9,96)==96);
        assert(sustained.finished && sustained.adsr.currentVolume==0);
        assert(key_bus[32*9]>key_bus[159*9] && key_bus[159*9]>0);
        assert(voice.changed[2]==0x48);
    }
    {
        _PBMIX mix={0};
        PartyboardMusyxPcmRamp ramp,other;
        int16_t constant[160];
        int64_t all[160*9]={0},chunks[160*9]={0};
        mix.vL=16384;mix.vDeltaL=0xffff;
        mix.vAuxAS=100;mix.vDeltaAuxAS=1;
        mix.vAuxBR=700;mix.vDeltaAuxBR=2;
        PartyboardMusyxPcmRampLoad(&ramp,&mix);other=ramp;
        for(i=0;i<160;++i) constant[i]=1000;
        PartyboardMusyxPcmRampMix(&ramp,all,constant,160);
        PartyboardMusyxPcmRampMix(&other,chunks,constant,7);
        PartyboardMusyxPcmRampMix(&other,chunks+7*9,constant+7,153);
        assert(!memcmp(all,chunks,sizeof all) && !memcmp(&ramp,&other,sizeof ramp));
        assert(ramp.gain[0]==16224 && ramp.gain[5]==260 && ramp.gain[7]==1020);
        assert(all[0]==16384000 && all[159*9]==16225000);
        assert(all[5]==100000 && all[7]==700000 && all[2]==0);
        {
            uint16_t flags[5]={0},gain=0,delta=0,last=0;
            memset(all,0,sizeof all);memset(&mix,0,sizeof mix);
            assert(PartyboardMusyxVolumeResidual(&gain,&delta,&last,100,flags,1));
            assert(last==96 && flags[3]==1);
            mix.vL=gain;mix.vDeltaL=delta;
            mix.vAuxAS=0;mix.vDeltaAuxAS=1;flags[1]|=0x20;
            PartyboardMusyxPcmRampLoad(&ramp,&mix);
            for(i=0;i<5;++i) PartyboardMusyxPcmRampMixPeriod(&ramp,flags[i],all+i*32*9,constant+i*32);
            assert(ramp.gain[0]==96 && ramp.gain[5]==32);
            assert(ramp.delta[0]==0 && ramp.delta[5]==0);
            assert(all[95*9]==95000 && all[96*9]==96000 && all[159*9]==96000);
            assert(all[31*9+5]==31000 && all[32*9+5]==32000 && all[159*9+5]==32000);
        }
    }
    {
        PartyboardMusyxPcmEnvelope sustain={0};
        PartyboardMusyxPcmRamp ramp;
        _PBMIX mix={0};
        int64_t bus[64*9]={0};
        sustain.adsr.mode=0;sustain.adsr.state=3;
        sustain.adsr.currentVolume=0x7fff0000;
        mix.vL=16384;mix.vDeltaL=0xffff;
        mix.vAuxBR=400;mix.vDeltaAuxBR=1;
        PartyboardMusyxPcmRampLoad(&ramp,&mix);
        memset(&voice,0,sizeof voice);
        assert(PartyboardMsmSampleResamplerInit(&a,&x,65536,bank,512));
        assert(PartyboardMusyxPcmVoiceRenderRampPeriod(&a,&sustain,&voice,0,&ramp,0,bus)==32);
        assert(bus[0]==(int64_t)16382*16384 && bus[31*9]==(int64_t)16382*16353);
        assert(PartyboardMusyxPcmVoiceRenderRampPeriod(&a,&sustain,&voice,1,&ramp,1,bus+32*9)==32);
        assert(ramp.gain[0]==16352 && ramp.delta[0]==0);
        assert(ramp.gain[7]==464 && ramp.delta[7]==1);
        assert(bus[32*9]==bus[63*9] && bus[63*9+7]>bus[32*9+7]);
    }
    puts("decoder/SRC/original-envelope/nine-bus voice regression PASS");
    return 0;
}
