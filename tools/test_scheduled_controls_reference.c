#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/port/musyx_pcm_controls.h"
#define MUSY_ASSERT(condition) assert(condition)
#include "original-scheduled-controls-reference.h"
int main(void) {
  unsigned flags,offset,first,breaking,count=0;
  for(flags=0;flags<256;++flags) for(offset=0;offset<5;++offset)
    for(first=0;first<2;++first) for(breaking=0;breaking<2;++breaking) {
      DSPvoice expected={0},actual;
      _PB pb={0};
      PartyboardMusyxPcmEnvelope envelope={0};
      PartyboardSrcPhase phase={12345,65536};
      expected.pb=&pb;expected.changed[offset]=flags;
      expected.pitch[offset]=(flags*197U+offset*31U)%0x4000U*16U;
      expected.playInfo.pitch=65536;expected.postBreak=(u8)breaking;
      expected.adsr.state=3;expected.adsr.currentVolume=0x40000000;
      expected.adsr.data.dls.rTime=4;
      actual=expected;envelope.adsr=expected.adsr;
      if(first) original_first_controls(&expected,offset);
      else original_later_controls(&expected,offset);
      assert(PartyboardMusyxPcmControlsScheduled(&envelope,&phase,&actual,offset,first));
      assert(!memcmp(&envelope.adsr,&expected.adsr,sizeof expected.adsr));
      assert(actual.postBreak==expected.postBreak && phase.ratio==expected.playInfo.pitch);
      assert(actual.playInfo.pitch==expected.playInfo.pitch);
      assert(phase.fraction==12345 && actual.changed[offset]==flags);
      ++count;
    }
  printf("%u original scheduled control comparisons PASS\n",count);
  return 0;
}
