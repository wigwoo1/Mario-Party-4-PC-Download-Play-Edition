#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/port/musyx_pcm_runtime.h"
#include "original-volume-schedule-reference.h"
#include "original-standard-volume-reference.h"
static uint32_t seed=12345;
static uint16_t random_gain(void) { seed=seed*1664525U+1013904223U;return (uint16_t)((seed>>8)&32767); }
int main(void) {
  unsigned iteration;
  for(iteration=0;iteration<65536;++iteration) {
    DSPvoice a={0},b;
    _PB pa={0},pb;
    PartyboardMusyxPcmRamp ramp;
    uint16_t fa[5]={0},fb[5]={0};
    a.pb=&pa;a.changed[0]=iteration&7;pa.mixerCtrl=(uint16_t)(iteration&255);
#define SET(T,L,G) a.T=random_gain();a.L=pa.mix.G=random_gain()
    SET(volL,lastVolL,vL);SET(volR,lastVolR,vR);SET(volS,lastVolS,vS);
    SET(volLa,lastVolLa,vAuxAL);SET(volRa,lastVolRa,vAuxAR);SET(volSa,lastVolSa,vAuxAS);
    SET(volLb,lastVolLb,vAuxBL);SET(volRb,lastVolRb,vAuxBR);SET(volSb,lastVolSb,vAuxBS);
#undef SET
    b=a;pb=pa;b.pb=&pb;
    original_prepare_standard(&b,fb);
    assert(PartyboardMusyxPcmRuntimePrepareStandard200(&a,SND_STUDIO_TYPE_STD,&ramp,fa));
    assert(!memcmp(&pa.mix,&pb.mix,sizeof pa.mix));
    assert(pa.mixerCtrl==pb.mixerCtrl && !memcmp(fa,fb,sizeof fa));
#define CHECK(L) assert(a.L==b.L)
    CHECK(lastVolL);CHECK(lastVolR);CHECK(lastVolS);
    CHECK(lastVolLa);CHECK(lastVolRa);CHECK(lastVolSa);
    CHECK(lastVolLb);CHECK(lastVolRb);CHECK(lastVolSb);
    original_init_standard(&b);
    assert(PartyboardMusyxPcmRuntimeInitStandardMixer200(&a,SND_STUDIO_TYPE_STD,&ramp));
    assert(!memcmp(&pa.mix,&pb.mix,sizeof pa.mix) && pa.mixerCtrl==pb.mixerCtrl);
    CHECK(lastVolL);CHECK(lastVolR);CHECK(lastVolS);
    CHECK(lastVolLa);CHECK(lastVolRa);CHECK(lastVolSa);
    CHECK(lastVolLb);CHECK(lastVolRb);CHECK(lastVolSb);
#undef CHECK
    a.smp_info.loopLength=b.smp_info.loopLength=iteration%7==0?28:0;
    pa.srcSelect=pb.srcSelect=(uint16_t)(iteration%3);
    a.playInfo.pitch=b.playInfo.pitch=(uint32_t)random_gain()*16;
    a.playInfo.posHi=b.playInfo.posHi=random_gain();
    a.playInfo.posLo=b.playInfo.posLo=seed;
    sal_update_hostplayinfo(&b);
    assert(PartyboardMusyxPcmRuntimeAdvanceHostPeriod(&a));
    assert(a.playInfo.posHi==b.playInfo.posHi && a.playInfo.posLo==b.playInfo.posLo);
  }
  puts("65536 original standard-studio preparation/startup comparisons PASS");
  return 0;
}
