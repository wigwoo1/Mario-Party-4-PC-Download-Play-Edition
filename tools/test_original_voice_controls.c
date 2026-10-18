#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "musyx/hardware.h"
#include "musyx/sal.h"
#include "../src/port/src_phase.h"
#include "../src/port/musyx_pcm_controls.h"
#include "../src/port/musyx_voice_source.h"
#include "../src/port/musyx_pcm_runtime.h"
void hwSetTimeOffset(u8 offset);

static DSPvoice voices[3];
static unsigned completion_calls;
static int completion_in_middle;
static u32 completion_callback(u32 message,u32 user_value) {
  assert(message==0 && user_value==voices[1].mesgCallBackUserValue);
  assert(voices[1].state);
  if(completion_in_middle) {
    assert(dspStudio[2].voiceRoot==&voices[0]);
    assert(voices[1].prev==&voices[0] && voices[1].next==&voices[2]);
    assert(voices[0].next==&voices[1] && voices[2].prev==&voices[1]);
  } else assert(dspStudio[2].voiceRoot==&voices[1]);
  ++completion_calls;
  return 0;
}

int main(void) {
  unsigned char encoded[56] = {0x12, 0x34, 0xff, 0xfe};
  SAMPLE_INFO sample;
  DSPvoice* voice = &voices[1];
  PartyboardSrcPhase phase = {0, 0};
  PartyboardMusyxPcmEnvelope envelope = {0};
  PartyboardMusyxSampleMemory memory = {0};
  PartyboardMsmSample source = {0}, saved;
  int16_t decoded[2];
  dspVoice = voices;
  memset(dspStudio, 0, sizeof(dspStudio));
  memset(&sample, 0, sizeof(sample));
  sample.addr = encoded;
  sample.length = 28;
  sample.info = 32000;
  sample.compType = 2;
  memset(voices, 0, sizeof(voices));
  voice->changed[0] = 0x28;
  voice->changed[1] = 0x40;
  voice->changed[2] = 0x20;
  voice->changed[3] = 0x55;
  hwSetTimeOffset(2);
  hwInitSamplePlayback(1, 123, &sample, 1, 17, 99, 1, 0);
  assert(voice->smp_info.addr == encoded && voice->smp_info.length == 28);
  assert(voice->smp_id == 123 && voice->prio == 17);
  assert(!PartyboardMusyxVoiceSourceOpen(&source, voice, &memory));
  assert(PartyboardMusyxSampleMemoryAdd(&memory, encoded, sizeof(encoded)));
  assert(PartyboardMusyxVoiceSourceOpen(&source, voice, &memory));
  assert(PartyboardMsmSampleRead(&source, decoded, 2) == 2);
  assert(decoded[0] == 0x1234 && decoded[1] == -2);
  {
    DSPvoice scheduled={0};
    _PB host_pb={0};
    PartyboardMusyxPcmEnvelope host_envelope={0};
    PartyboardSrcPhase host_phase={0,65536};
    unsigned period;
    scheduled.pb=&host_pb;scheduled.smp_info.length=28;
    scheduled.playInfo.pitch=65536;
    host_envelope.adsr.state=3;host_envelope.adsr.currentVolume=0x40000000;
    for(period=0;period<5;++period) {
      int16_t silent_period[32]={0};
      assert(PartyboardMusyxPcmScheduleHostPeriod(&host_envelope,&host_phase,&scheduled,period,period==0));
      assert(scheduled.playInfo.posHi==(period+1)*32);
      assert(!PartyboardMusyxPcmScheduleHostPeriod(&host_envelope,&host_phase,&scheduled,period,0));
      assert(scheduled.playInfo.posHi==(period+1)*32);
      assert(PartyboardMusyxPcmEnvelopeApply(&host_envelope,silent_period,32)==32);
    }
    assert(scheduled.playInfo.posHi==160 && !scheduled.playInfo.posLo);
    {
      PartyboardMusyxHostBlock plan;
      scheduled.playInfo.posHi=scheduled.playInfo.posLo=0;
      scheduled.changed[2]=8;scheduled.pitch[2]=131072;
      assert(PartyboardMusyxPcmScheduleHostBlock(&host_envelope,&host_phase,&scheduled,0,1,&plan));
      assert(plan.count==5 && !plan.source_done && !plan.envelope_done);
      assert(plan.periods[0].ratio==65536 && plan.periods[2].ratio==131072);
      assert(scheduled.playInfo.posHi==256 && !scheduled.playInfo.posLo);
      assert(PartyboardMusyxPcmScheduleHostBlock(&host_envelope,&host_phase,&scheduled,0,0,&plan));
      assert(!plan.count && plan.source_done && scheduled.playInfo.posHi==256);
      scheduled.changed[0]=8;scheduled.pitch[0]=0x40000;
      assert(PartyboardMusyxPcmScheduleHostBlock(&host_envelope,&host_phase,&scheduled,0,0,&plan));
      assert(plan.source_done && !plan.count && scheduled.playInfo.posHi==256);
      {
        ADSR_VARS expected={0};
        u16 start,delta;
        expected.mode=0;expected.data.linear.aTime=5;
        expected.data.linear.dTime=3;expected.data.linear.sLevel=0x5000;
        scheduled.adsr=expected;scheduled.smp_info.loopLength=28;
        memset(scheduled.changed,0,sizeof scheduled.changed);
        scheduled.changed[0]=0x10;
        host_envelope.finished=1;host_envelope.done_after_block=1;
        assert(!adsrSetup(&expected));
        for(period=0;period<5;++period) assert(!adsrHandle(&expected,&start,&delta));
        assert(PartyboardMusyxPcmScheduleHostBlock(&host_envelope,&host_phase,&scheduled,0,0,&plan));
        assert(plan.count==5 && !plan.envelope_done && !host_envelope.finished);
        assert(!memcmp(&expected,&host_envelope.adsr,sizeof expected));
        assert(!memcmp(&expected,&scheduled.adsr,sizeof expected));
        assert(host_pb.ve.currentVolume==(uint16_t)plan.periods[0].volume);
        assert(host_pb.ve.currentDelta==(uint16_t)plan.periods[0].delta);
        assert(scheduled.changed[0]==0x10);
        {
          PartyboardMusyxPcmEnvelope before=host_envelope;
          ADSR_VARS before_voice=scheduled.adsr;
          uint32_t before_position=scheduled.playInfo.posHi;
          scheduled.changed[4]=8;scheduled.pitch[4]=0x40000;
          assert(!PartyboardMusyxPcmScheduleHostBlock(&host_envelope,&host_phase,&scheduled,0,0,&plan));
          assert(!memcmp(&before,&host_envelope,sizeof before));
          assert(!memcmp(&before_voice,&scheduled.adsr,sizeof before_voice));
          assert(scheduled.playInfo.posHi==before_position);
        }
      }
    }
  }
  {
    int16_t banks[1536]={0},pcm[2];
    PartyboardMusyxPcmRuntime runtime={0};
    unsigned phase_index;
    for(phase_index=0;phase_index<128;++phase_index) banks[512+phase_index*4+3]=32767;
    assert(PartyboardMusyxPcmRuntimeStart(&runtime,voice,&memory,65536,banks,1536));
    assert(runtime.resampler.context==&runtime.sample);
    assert(runtime.resampler.coefficients==banks+512);
    {
      _PB selection_pb={0};
      _PB *original_pb=voice->pb;
      DSPvoice before=*voice;
      PartyboardSrcPhase before_phase=runtime.resampler.phase;
      voice->pb=&selection_pb;voice->changed[0]=0x80;voice->srcCoefSelect=2;
      assert(PartyboardMusyxPcmRuntimeSelectSource(&runtime,voice,0));
      assert(runtime.resampler.coefficients==banks+1024 && selection_pb.coefSelect==2);
      assert(!memcmp(&before_phase,&runtime.resampler.phase,sizeof before_phase));
      voice->changed[0]=0;voice->srcCoefSelect=0;
      assert(PartyboardMusyxPcmRuntimeSelectSource(&runtime,voice,0));
      assert(runtime.coefficient_bank==2);
      voice->changed[0]=0x180;voice->srcTypeSelect=3;
      assert(!PartyboardMusyxPcmRuntimeSelectSource(&runtime,voice,0));
      assert(runtime.coefficient_bank==2 && selection_pb.coefSelect==2);
      {
        PartyboardMusyxPcmRuntime unchanged=runtime;
        ADSR_VARS before_adsr=voice->adsr;
        int64_t selection_bus[160*9]={0};
        u32 before_hi=voice->playInfo.posHi,before_lo=voice->playInfo.posLo;
        assert(!PartyboardMusyxPcmRuntimeRenderPlannedStandard200(&runtime,voice,
            SND_STUDIO_TYPE_STD,0,0,selection_bus));
        assert(!memcmp(&unchanged,&runtime,sizeof runtime));
        assert(!memcmp(&before_adsr,&voice->adsr,sizeof before_adsr));
        assert(voice->playInfo.posHi==before_hi && voice->playInfo.posLo==before_lo);
      }
      *voice=before;voice->pb=original_pb;
      for(unsigned source_mode=1;source_mode<=2;++source_mode) {
        DSPvoice linear_voice=*voice;
        _PB linear_pb={0};
        PartyboardMusyxPcmRuntime linear_runtime={0};
        int64_t linear_bus[160*9]={0};
        linear_voice.pb=&linear_pb;linear_voice.srcTypeSelect=(u8)source_mode;
        linear_voice.playInfo.posHi=linear_voice.playInfo.posLo=0;
        memset(linear_voice.changed,0,sizeof linear_voice.changed);
        linear_voice.changed[0]=8;linear_voice.pitch[0]=65536;
        assert(PartyboardMusyxPcmRuntimeStart(&linear_runtime,&linear_voice,&memory,65536,NULL,0));
        assert(!linear_runtime.resampler.coefficients);
        linear_voice.changed[0]=0x100;linear_voice.srcTypeSelect=0;
        assert(!PartyboardMusyxPcmRuntimeSelectSource(&linear_runtime,&linear_voice,0));
        assert(linear_runtime.source_mode==source_mode && !linear_runtime.resampler.coefficients);
        linear_voice.srcTypeSelect=(u8)source_mode;linear_voice.changed[0]=8;
        if(source_mode==1) {
          PartyboardMusyxPcmRuntime saved_runtime=linear_runtime;
          DSPvoice saved_voice=linear_voice;
          _PB saved_pb=linear_pb;
          PartyboardMusyxPcmRamp rejected_ramp={0};
          SAL_VOLINFO rejected_volume={0};
          assert(!PartyboardMusyxPcmRuntimeRenderPeriod(&linear_runtime,&linear_voice,0,&rejected_volume,linear_bus));
          assert(!PartyboardMusyxPcmRuntimeRenderRampPeriod(&linear_runtime,&linear_voice,0,&rejected_ramp,0,linear_bus));
          assert(!PartyboardMusyxPcmRuntimeRenderScheduledPeriod(&linear_runtime,&linear_voice,0,1,&rejected_ramp,0,linear_bus));
          assert(!PartyboardMusyxPcmRuntimeRenderStandard200(&linear_runtime,&linear_voice,SND_STUDIO_TYPE_STD,0,1,linear_bus));
          assert(!memcmp(&saved_runtime,&linear_runtime,sizeof saved_runtime));
          assert(!memcmp(&saved_voice,&linear_voice,sizeof saved_voice));
          assert(!memcmp(&saved_pb,&linear_pb,sizeof saved_pb));
          for(unsigned channel=0;channel<9;++channel) assert(!rejected_ramp.gain[channel] && !rejected_ramp.delta[channel]);
          for(unsigned frame=0;frame<160*9;++frame) assert(!linear_bus[frame]);
        }
        assert(PartyboardMusyxPcmRuntimeRenderPlannedStandard200(&linear_runtime,&linear_voice,
            SND_STUDIO_TYPE_STD,0,1,linear_bus)==32);
        assert(linear_runtime.resampler.ended && !linear_pb.state && linear_pb.srcSelect==source_mode);
        assert(linear_runtime.active && !linear_runtime.completion_pending && linear_voice.playInfo.posHi==160);
        assert(!PartyboardMusyxPcmRuntimeRenderPlannedStandard200(&linear_runtime,&linear_voice,
            SND_STUDIO_TYPE_STD,0,0,linear_bus));
        assert(!linear_runtime.active && linear_runtime.completion_pending);
      }
      assert(PartyboardMusyxPcmRuntimeStart(&runtime,voice,&memory,65536,banks,1536));
    }
    assert(PartyboardSrcResamplerRead(&runtime.resampler,pcm,2)==2);
    assert(pcm[0]==0x1233 && pcm[1]==-2);
    {
      _PB stored={0};
      _PB *saved_pb=voice->pb;
      voice->pb=&stored;
      assert(PartyboardMusyxPcmRuntimeStoreSourceState(&runtime,voice));
      assert(stored.src.ratioHi==1 && !stored.src.ratioLo && !stored.src.currentAddressFrac);
      assert(!stored.src.last_samples[0] && !stored.src.last_samples[1]);
      assert(stored.src.last_samples[2]==0x1234 && stored.src.last_samples[3]==65534);
      runtime.resampler.phase.fraction=65536;
      assert(!PartyboardMusyxPcmRuntimeStoreSourceState(&runtime,voice));
      assert(!stored.src.currentAddressFrac && stored.src.last_samples[3]==65534);
      runtime.resampler.phase.fraction=0;
      voice->pb=saved_pb;
    }
    assert(!PartyboardMusyxPcmRuntimeStart(&runtime,voice,&memory,65536,NULL,0));
    assert(runtime.active && runtime.resampler.context==&runtime.sample);
    {
      int64_t bus[32*9]={0};
      SAL_VOLINFO volume={0};volume.volL=1;
      {
        _PB finite_pb={0};
        _PB *saved_pb=voice->pb;
        DSPvoice saved_voice=*voice;
        PartyboardMusyxPcmRamp ramp={0};
        voice->pb=&finite_pb;voice->changed[0]=8;voice->pitch[0]=65536;
        voice->playInfo.posHi=voice->playInfo.posLo=0;
        ramp.gain[0]=32767;
        assert(PartyboardMusyxPcmRuntimeStart(&runtime,voice,&memory,65536,banks,1536));
        assert(PartyboardMusyxPcmRuntimeRenderScheduledPeriod(&runtime,voice,0,1,&ramp,0,bus)==28);
        assert(voice->playInfo.posHi==32 && !voice->playInfo.posLo);
        assert(!runtime.active && runtime.completion_pending && runtime.envelope.remaining==4);
        assert(!PartyboardMusyxPcmRuntimeRenderScheduledPeriod(&runtime,voice,0,1,&ramp,0,bus));
        assert(voice->playInfo.posHi==32);
        {
          PartyboardMusyxHostBlock plan;
          PartyboardSrcPhase scheduler_phase={0,65536};
          PartyboardMusyxPcmEnvelope scheduled_envelope;
          int64_t planned_bus[160*9]={0};
          uint16_t resets[5]={0};
          voice->playInfo.posHi=voice->playInfo.posLo=0;
          memset(voice->changed,0,sizeof voice->changed);
          assert(PartyboardMusyxPcmRuntimeStart(&runtime,voice,&memory,65536,banks,1536));
          assert(PartyboardMusyxPcmScheduleHostBlock(&runtime.envelope,&scheduler_phase,voice,0,1,&plan));
          assert(plan.count==5 && voice->playInfo.posHi==160);
          scheduled_envelope=runtime.envelope;
          assert(PartyboardMusyxPcmRenderHostPlan(&runtime.resampler,&plan,0,&ramp,resets,planned_bus)==28);
          assert(!memcmp(&runtime.envelope,&scheduled_envelope,sizeof scheduled_envelope));
          assert(voice->playInfo.posHi==160 && planned_bus[0]>0 && !planned_bus[28*9]);
          assert(runtime.active && !runtime.completion_pending);
          voice->playInfo.posHi=voice->playInfo.posLo=0;
          voice->volL=32767;
          assert(PartyboardMusyxPcmRuntimeStart(&runtime,voice,&memory,65536,banks,1536));
          memset(planned_bus,0,sizeof planned_bus);
          assert(PartyboardMusyxPcmRuntimeRenderPlannedStandard200(&runtime,voice,SND_STUDIO_TYPE_STD,0,1,planned_bus)==32);
          assert(runtime.active && runtime.resampler.ended && !runtime.completion_pending);
          assert(!finite_pb.state && !planned_bus[32*9]);
          assert(voice->playInfo.posHi==160 && !runtime.envelope.remaining);
          hwStart(1,2);
          hwSetMesgCallback(completion_callback);
          assert(!PartyboardMusyxPcmRuntimeNotifyCompletion(&runtime,voice));
          assert(!completion_calls && voice->state && dspStudio[2].voiceRoot==voice);
          /* The original finite completion gate precedes unused SRC updates. */
          voice->changed[0]=0x180;voice->srcTypeSelect=2;voice->srcCoefSelect=7;
          assert(!PartyboardMusyxPcmRuntimeRenderPlannedStandard200(&runtime,voice,SND_STUDIO_TYPE_STD,0,0,planned_bus));
          assert(!runtime.active && runtime.completion_pending && voice->playInfo.posHi==160);
          assert(PartyboardMusyxPcmRuntimeNotifyCompletion(&runtime,voice));
          assert(completion_calls==1 && !voice->state && !dspStudio[2].voiceRoot);
          assert(!PartyboardMusyxPcmRuntimeNotifyCompletion(&runtime,voice));
          assert(completion_calls==1);
          hwSetMesgCallback(NULL);
          completion_calls=0;
          voice->srcTypeSelect=0;voice->srcCoefSelect=1;
          {
            unsigned boundary;
            const u32 lengths[2]={16,24},ratios[2]={32768,49152};
            for(boundary=0;boundary<2;++boundary) {
              voice->smp_info.length=lengths[boundary];
              voice->playInfo.posHi=voice->playInfo.posLo=0;
              memset(voice->changed,0,sizeof voice->changed);
              voice->changed[0]=8;voice->pitch[0]=ratios[boundary];
              assert(PartyboardMusyxPcmRuntimeStart(&runtime,voice,&memory,ratios[boundary],banks,1536));
              memset(planned_bus,0,sizeof planned_bus);
              assert(PartyboardMusyxPcmRuntimeRenderPlannedStandard200(&runtime,voice,SND_STUDIO_TYPE_STD,0,1,planned_bus)==32);
              assert(runtime.resampler.ended && !finite_pb.state && !planned_bus[32*9]);
              assert(runtime.active && !runtime.completion_pending);
              assert(!PartyboardMusyxPcmRuntimeRenderPlannedStandard200(&runtime,voice,SND_STUDIO_TYPE_STD,0,0,planned_bus));
              assert(!runtime.active && runtime.completion_pending);
            }
          }
        }
        *voice=saved_voice;voice->pb=saved_pb;
        assert(PartyboardMusyxPcmRuntimeStart(&runtime,voice,&memory,65536,banks,1536));
        assert(PartyboardSrcResamplerRead(&runtime.resampler,pcm,2)==2);
        memset(bus,0,sizeof bus);
      }
      assert(PartyboardMusyxPcmRuntimeRenderPeriod(&runtime,voice,0,&volume,bus)==26);
      assert(!runtime.active && runtime.resampler.ended);
      assert(!PartyboardMusyxPcmRuntimeRenderPeriod(&runtime,voice,0,&volume,bus));
      {
        uint32_t completion=0xffffffff;
        assert(!PartyboardMusyxPcmRuntimeTakeCompletion(&runtime,voice,NULL));
        assert(PartyboardMusyxPcmRuntimeTakeCompletion(&runtime,voice,&completion));
        assert(completion==voice->mesgCallBackUserValue);
        assert(!PartyboardMusyxPcmRuntimeTakeCompletion(&runtime,voice,&completion));
      }
      assert(PartyboardMusyxPcmRuntimeStart(&runtime,voice,&memory,65536,banks,1536));
      assert(PartyboardMusyxPcmRuntimeRenderPeriod(&runtime,voice,0,&volume,bus)==28);
      hwStart(1,2);
      hwSetMesgCallback(completion_callback);
      assert(PartyboardMusyxPcmRuntimeNotifyCompletion(&runtime,voice));
      assert(completion_calls==1 && !voice->state && !dspStudio[2].voiceRoot);
      assert(!PartyboardMusyxPcmRuntimeNotifyCompletion(&runtime,voice));
      assert(completion_calls==1);
      assert(PartyboardMusyxPcmRuntimeStart(&runtime,voice,&memory,65536,banks,1536));
      assert(PartyboardMusyxPcmRuntimeRenderPeriod(&runtime,voice,0,&volume,bus)==28);
      hwStart(2,2);
      hwStart(1,2);
      hwStart(0,2);
      completion_in_middle=1;
      assert(PartyboardMusyxPcmRuntimeNotifyCompletion(&runtime,voice));
      assert(completion_calls==2 && !voice->state);
      assert(dspStudio[2].voiceRoot==&voices[0]);
      assert(voices[0].next==&voices[2] && voices[2].prev==&voices[0]);
      assert(voices[0].state && voices[2].state);
      assert(!PartyboardMusyxPcmRuntimeNotifyCompletion(&runtime,voice));
      assert(completion_calls==2);
      hwOff(0);
      hwOff(2);
      assert(!dspStudio[2].voiceRoot);
      completion_in_middle=0;
      hwSetMesgCallback(NULL);
      {
        DSPvoice saved_voice=*voice;
        int64_t block_bus[160*9]={0};
        uint16_t resets[5]={0,0,1,0,0};
        PartyboardMusyxPcmRamp block_ramp;
        _PBMIX block_mix={0};
        unsigned period;
        block_mix.vL=100;block_mix.vDeltaL=1;
        PartyboardMusyxPcmRampLoad(&block_ramp,&block_mix);
        voice->smp_info.loop=0;voice->smp_info.loopLength=28;
        for(period=0;period<5;++period) {
          voice->changed[period]=8;voice->pitch[period]=65536;
        }
        voice->pitch[2]=32768;
        voice->pitch[4]=0x40000;
        assert(PartyboardMusyxPcmRuntimeStart(&runtime,voice,&memory,65536,banks,1536));
        assert(!PartyboardMusyxPcmRuntimeRenderBlock(&runtime,voice,0,&block_ramp,resets,block_bus));
        assert(runtime.active && block_ramp.gain[0]==100 && !block_bus[0]);
        assert(PartyboardSrcResamplerRead(&runtime.resampler,pcm,1)==1 && pcm[0]==0x1233);
        assert(PartyboardMusyxPcmRuntimeStart(&runtime,voice,&memory,65536,banks,1536));
        voice->pitch[4]=65536;
        assert(PartyboardMusyxPcmRuntimeRenderBlock(&runtime,voice,0,&block_ramp,resets,block_bus)==160);
        assert(runtime.active && !runtime.completion_pending);
        assert(block_ramp.gain[0]==164 && !block_ramp.delta[0]);
        assert(runtime.resampler.phase.ratio==65536);
        {
          _PB actual_pb={0};
          voice->pb=&actual_pb;
          voice->volL=32767;
          voice->volLa=voice->volRa=voice->volSa=0;
          voice->volLb=voice->volRb=voice->volSb=0;
          {
            DSPvoice selected=*voice;
            _PB selected_pb={0};
            PartyboardMusyxPcmRuntime selected_runtime={0};
            int64_t selected_bus[160*9]={0};
            unsigned channel;
            int nonzero=0;
            selected.pb=&selected_pb;
            memset(selected.changed,0,sizeof selected.changed);
            assert(PartyboardMusyxPcmRuntimeStart(&selected_runtime,&selected,&memory,65536,banks,1536));
            assert(PartyboardMusyxPcmRuntimeRenderPlannedStandard200(&selected_runtime,&selected,
                SND_STUDIO_TYPE_STD,0,1,selected_bus)==160);
            for(channel=0;channel<160*9;++channel) nonzero|=selected_bus[channel]!=0;
            assert(nonzero && selected_pb.coefSelect==1);
            selected.changed[0]=0x80;selected.srcCoefSelect=2;
            memset(selected_bus,0,sizeof selected_bus);
            assert(PartyboardMusyxPcmRuntimeRenderPlannedStandard200(&selected_runtime,&selected,
                SND_STUDIO_TYPE_STD,0,0,selected_bus)==160);
            assert(selected_pb.coefSelect==2 && selected_runtime.coefficient_bank==2);
            for(channel=0;channel<160*9;++channel) assert(!selected_bus[channel]);
            assert(selected_runtime.active && !selected_runtime.resampler.ended);
            selected.changed[0]=0x100;selected.srcTypeSelect=2;
            memset(selected_bus,0,sizeof selected_bus);nonzero=0;
            assert(PartyboardMusyxPcmRuntimeRenderPlannedStandard200(&selected_runtime,&selected,
                SND_STUDIO_TYPE_STD,0,0,selected_bus)==160);
            for(channel=0;channel<160*9;++channel) nonzero|=selected_bus[channel]!=0;
            assert(nonzero && selected_pb.srcSelect==2 && selected_runtime.resampler.mode==2);
            assert(!selected_runtime.resampler.phase.fraction && selected_runtime.active);
            selected.changed[0]=0x108;selected.srcTypeSelect=1;selected.pitch[0]=0;
            memset(selected_bus,0,sizeof selected_bus);
            assert(PartyboardMusyxPcmRuntimeRenderPlannedStandard200(&selected_runtime,&selected,
                SND_STUDIO_TYPE_STD,0,0,selected_bus)==160);
            assert(selected_pb.srcSelect==1 && selected_runtime.resampler.mode==1);
            assert(selected_runtime.resampler.history[0]==0xd60
                && selected_runtime.resampler.history[1]==0xd60 && selected_runtime.active);
          }
          {
            DSPvoice direct_voice=*voice,planned_voice;
            _PB direct_pb={0},planned_pb={0};
            PartyboardMusyxPcmRuntime direct_runtime={0},planned_runtime={0};
            PartyboardMusyxPcmRamp direct_ramp,planned_ramp;
            PartyboardMusyxHostBlock plan;
            PartyboardSrcPhase host_phase={0,65536};
            int64_t direct_bus[160*9]={0},planned_bus[160*9]={0};
            uint16_t plan_resets[5]={0};
            direct_voice.volLa=12345;direct_voice.volRa=23456;direct_voice.volSa=3456;
            direct_voice.volLb=4567;direct_voice.volRb=5678;direct_voice.volSb=6789;
            direct_voice.pb=&direct_pb;
            planned_voice=direct_voice;planned_voice.pb=&planned_pb;
            assert(PartyboardMusyxPcmRuntimeStart(&direct_runtime,&direct_voice,&memory,65536,banks,1536));
            assert(PartyboardMusyxPcmRuntimeStart(&planned_runtime,&planned_voice,&memory,65536,banks,1536));
            assert(PartyboardMusyxPcmRuntimeInitStandardMixer200(&direct_voice,SND_STUDIO_TYPE_STD,&direct_ramp));
            assert(PartyboardMusyxPcmRuntimeInitStandardMixer200(&planned_voice,SND_STUDIO_TYPE_STD,&planned_ramp));
            assert(PartyboardMusyxPcmRuntimeRenderScheduledBlock(&direct_runtime,&direct_voice,0,&direct_ramp,plan_resets,direct_bus)==160);
            assert(PartyboardMusyxPcmScheduleHostBlock(&planned_runtime.envelope,&host_phase,&planned_voice,0,1,&plan));
            assert(plan.count==5);
            assert(PartyboardMusyxPcmRenderHostPlan(&planned_runtime.resampler,&plan,0,&planned_ramp,plan_resets,planned_bus)==160);
            assert(!memcmp(direct_bus,planned_bus,sizeof direct_bus));
            assert(!memcmp(&direct_ramp,&planned_ramp,sizeof direct_ramp));
            assert(!memcmp(&direct_runtime.envelope.adsr,&planned_runtime.envelope.adsr,sizeof(ADSR_VARS)));
            assert(!memcmp(direct_runtime.resampler.history,planned_runtime.resampler.history,sizeof direct_runtime.resampler.history));
            assert(direct_runtime.resampler.phase.ratio==planned_runtime.resampler.phase.ratio);
            assert(direct_runtime.resampler.phase.fraction==planned_runtime.resampler.phase.fraction);
            {
              unsigned block_index;
              const size_t expected_frames[3]={160,160,64};
              direct_voice.postBreak=planned_voice.postBreak=0;
              memset(direct_voice.changed,0,sizeof direct_voice.changed);
              memset(planned_voice.changed,0,sizeof planned_voice.changed);
              direct_voice.changed[2]=planned_voice.changed[2]=0x68;
              direct_voice.changed[3]=planned_voice.changed[3]=0x48;
              direct_voice.pitch[2]=planned_voice.pitch[2]=0x40000;
              direct_voice.pitch[3]=planned_voice.pitch[3]=0x40000;
              for(block_index=0;block_index<3;++block_index) {
                memset(direct_bus,0,sizeof direct_bus);
                memset(planned_bus,0,sizeof planned_bus);
                assert(PartyboardMusyxPcmRuntimeRenderScheduledBlock(&direct_runtime,&direct_voice,0,&direct_ramp,plan_resets,direct_bus)==expected_frames[block_index]);
                assert(PartyboardMusyxPcmScheduleHostBlock(&planned_runtime.envelope,&host_phase,&planned_voice,0,0,&plan));
                assert(plan.count*32==expected_frames[block_index]);
                assert(PartyboardMusyxPcmRenderHostPlan(&planned_runtime.resampler,&plan,0,&planned_ramp,plan_resets,planned_bus)==expected_frames[block_index]);
                assert(!memcmp(direct_bus,planned_bus,sizeof direct_bus));
                assert(!memcmp(&direct_ramp,&planned_ramp,sizeof direct_ramp));
                assert(!memcmp(&direct_runtime.envelope.adsr,&planned_runtime.envelope.adsr,sizeof(ADSR_VARS)));
                assert(direct_voice.postBreak==planned_voice.postBreak);
                assert(direct_voice.playInfo.pitch==planned_voice.playInfo.pitch);
                direct_voice.changed[2]=planned_voice.changed[2]=0;
                direct_voice.changed[3]=planned_voice.changed[3]=0;
              }
              assert(plan.envelope_done && direct_runtime.completion_pending);
            }
          }
          assert(PartyboardMusyxPcmRuntimeStart(&runtime,voice,&memory,65536,banks,1536));
          memset(block_bus,0,sizeof block_bus);
          assert(PartyboardMusyxPcmRuntimeRenderStandard200(&runtime,voice,SND_STUDIO_TYPE_STD,0,1,block_bus)==160);
          assert(block_bus[0]>0 && actual_pb.mix.vL==32767 && !actual_pb.mix.vDeltaL);
          assert(actual_pb.src.ratioHi==1 && !actual_pb.src.ratioLo);
          assert(PartyboardMusyxPcmRuntimeRenderStandard200(&runtime,voice,SND_STUDIO_TYPE_STD,0,0,block_bus)==160);
          assert(runtime.active && !runtime.completion_pending && actual_pb.mix.vL==32767);
          {
            unsigned release_periods=0;
            voice->postBreak=0;voice->changed[2]=0x68;voice->pitch[2]=0x40000;
            PartyboardMusyxPcmRampLoad(&block_ramp,&actual_pb.mix);
            assert(PartyboardMusyxPcmRuntimeRenderScheduledPeriod(&runtime,voice,2,0,&block_ramp,0,block_bus)==32);
            ++release_periods;
            assert(voice->postBreak && runtime.resampler.phase.ratio==65536);
            voice->changed[3]=0x48;voice->pitch[3]=0x40000;
            while(runtime.active && release_periods<20) {
              assert(PartyboardMusyxPcmRuntimeRenderScheduledPeriod(&runtime,voice,3,0,&block_ramp,0,block_bus)==32);
              ++release_periods;
            }
            assert(release_periods==10 && !runtime.active && runtime.completion_pending);
            assert(runtime.resampler.phase.ratio==65536);
          }
          voice->postBreak=0;
          for(period=0;period<5;++period) { voice->changed[period]=8;voice->pitch[period]=65536; }
          voice->changed[2]=0x68;voice->pitch[2]=0x40000;
          voice->pitch[3]=0x40000;
          assert(PartyboardMusyxPcmRuntimeStart(&runtime,voice,&memory,65536,banks,1536));
          assert(PartyboardMusyxPcmRuntimeRenderStandard200(&runtime,voice,SND_STUDIO_TYPE_STD,0,1,block_bus)==160);
          assert(voice->postBreak && runtime.active && !runtime.completion_pending);
          voice->changed[2]=voice->changed[3]=0;
          assert(PartyboardMusyxPcmRuntimeRenderStandard200(&runtime,voice,SND_STUDIO_TYPE_STD,0,0,block_bus)==160);
          assert(PartyboardMusyxPcmRuntimeRenderStandard200(&runtime,voice,SND_STUDIO_TYPE_STD,0,0,block_bus)==64);
          assert(!runtime.active && runtime.completion_pending);
        }
        *voice=saved_voice;
      }
      {
        PartyboardMusyxPcmRamp scheduled={0};
        uint16_t last[9]={0},target[9]={100,320,640,96,64,32,160,480,800};
        uint16_t resets[5]={0};
        int16_t mono[32]={0};
        int64_t scheduled_bus[32*9]={0};
        unsigned period;
        PartyboardMusyxPcmRampSchedule(&scheduled,last,target,7,resets);
        assert(scheduled.delta[0]==0 && scheduled.delta[1]==2 && scheduled.delta[8]==5);
        for(period=0;period<5;++period)
          PartyboardMusyxPcmRampMixPeriod(&scheduled,resets[period],scheduled_bus,mono);
        assert(scheduled.gain[1]==320 && scheduled.gain[8]==800);
        PartyboardMusyxPcmRampSchedule(&scheduled,last,target,0,resets);
        assert(resets[3]==9 && resets[2]==16 && resets[1]==32);
        for(period=0;period<5;++period)
          PartyboardMusyxPcmRampMixPeriod(&scheduled,resets[period],scheduled_bus,mono);
        assert(scheduled.gain[0]==96 && scheduled.gain[3]==96);
        assert(scheduled.gain[4]==64 && scheduled.gain[5]==32);
        assert(scheduled.gain[1]==320 && scheduled.gain[8]==800);
        {
          _PBMIX stored={0};
          PartyboardMusyxPcmRamp continued;
          PartyboardMusyxPcmRampStore(&scheduled,&stored);
          assert(stored.vL==96 && stored.vR==320 && stored.vS==640);
          assert(stored.vAuxAL==96 && stored.vAuxAR==64 && stored.vAuxAS==32);
          assert(stored.vAuxBL==160 && stored.vAuxBR==480 && stored.vAuxBS==800);
          assert(!stored.vDeltaL && !stored.vDeltaAuxAS && !stored.vDeltaAuxBS);
          scheduled.delta[7]=-3;
          PartyboardMusyxPcmRampStore(&scheduled,&stored);
          assert(stored.vDeltaAuxBR==65533);
          PartyboardMusyxPcmRampLoad(&continued,&stored);
          assert(continued.delta[7]==-3 && continued.gain[7]==480);
          PartyboardMusyxPcmRampMixPeriod(&continued,0,scheduled_bus,mono);
          PartyboardMusyxPcmRampStore(&continued,&stored);
          assert(stored.vAuxBR==384 && stored.vAuxBS==800);
        }
      }
      {
        DSPvoice saved_voice=*voice;
        PartyboardMusyxPcmRamp scheduled={0};
        uint16_t resets[5]={0};
        voice->volL=320;voice->volR=640;voice->volS=960;
        voice->volLa=96;voice->volRa=64;voice->volSa=32;
        voice->volLb=160;voice->volRb=480;voice->volSb=800;
        voice->lastVolL=voice->lastVolR=voice->lastVolS=0;
        voice->lastVolLa=voice->lastVolRa=voice->lastVolSa=0;
        voice->lastVolLb=voice->lastVolRb=voice->lastVolSb=0;
        voice->changed[0]=7;
        assert(PartyboardMusyxPcmRuntimeScheduleStandardVolume(voice,&scheduled,resets));
        assert(voice->lastVolL==320 && voice->lastVolR==640 && voice->lastVolS==960);
        assert(voice->lastVolLa==0 && voice->lastVolRa==0 && voice->lastVolSa==0);
        assert(voice->lastVolLb==160 && voice->lastVolRb==480 && voice->lastVolSb==800);
        assert(scheduled.delta[0]==2 && scheduled.delta[1]==4 && scheduled.delta[2]==6);
        assert(scheduled.delta[6]==1 && scheduled.delta[7]==3 && scheduled.delta[8]==5);
        assert(voice->changed[0]==7);
        voice->changed[0]=0;
        assert(PartyboardMusyxPcmRuntimeScheduleStandardVolume(voice,&scheduled,resets));
        assert(voice->lastVolLa==96 && voice->lastVolRa==64 && voice->lastVolSa==32);
        assert(resets[3]==8 && resets[2]==16 && resets[1]==32);
        *voice=saved_voice;
      }
      {
        PartyboardMusyxPcmRamp gated={0};
        uint16_t last[9]={0},target[9]={64,64,64,96,96,96,128,128,128};
        uint16_t resets[5]={0};
        unsigned channel;
        for(channel=0;channel<9;++channel) gated.delta[channel]=5;
        PartyboardMusyxPcmRampScheduleActive(&gated,last,target,0,1,resets);
        assert(resets[2]==7 && !resets[3] && !resets[4]);
        for(channel=3;channel<9;++channel)
          assert(!gated.delta[channel] && !last[channel] && !gated.gain[channel]);
        memset(resets,0,sizeof resets);
        PartyboardMusyxPcmRampScheduleActive(&gated,last,target,2,1,resets);
        assert(!last[3] && !gated.delta[3]);
        PartyboardMusyxPcmRampScheduleActive(&gated,last,target,0,3,resets);
        assert(last[3]==96 && gated.delta[3]==1 && resets[3]==0x38);
        assert(!last[6] && !gated.delta[6]);
        assert(PartyboardMusyxPcmRampAuxControl200(&gated,0,0x80)==0x80);
        assert(PartyboardMusyxPcmRampAuxControl200(&gated,2,0x80)==0x81);
        assert(PartyboardMusyxPcmRampAuxControl200(&gated,0,0x83)==0x81);
        gated.delta[6]=1;
        assert(PartyboardMusyxPcmRampAuxControl200(&gated,4,0x80)==0x82);
        memset(&gated,0,sizeof gated);
        assert(PartyboardMusyxPcmRampAuxControl200(&gated,6,0x83)==0x80);
        assert(PartyboardMusyxPcmRampStandardControl200(&gated,1,0x80)==0x88);
        assert(PartyboardMusyxPcmRampStandardControl200(&gated,0,0x8f)==0x80);
        gated.gain[5]=96;
        assert(PartyboardMusyxPcmRampStandardControl200(&gated,2,0x80)==0x85);
        assert(PartyboardMusyxPcmRampStandardControl200(&gated,0,0x81)==0x8d);
      }
      {
        DSPvoice saved_voice=*voice;
        _PB pb={0};
        PartyboardMusyxPcmRamp prepared={0};
        uint16_t resets[5]={0};
        voice->pb=&pb;pb.mixerCtrl=0x80;
        voice->changed[0]=0;
        voice->volLa=96;voice->lastVolLa=0;
        voice->volLb=320;voice->lastVolLb=0;
        assert(!PartyboardMusyxPcmRuntimePrepareStandard200(voice,SND_STUDIO_TYPE_DPL2,&prepared,resets));
        assert(pb.mixerCtrl==0x80 && voice->lastVolLb==0);
        assert(PartyboardMusyxPcmRuntimePrepareStandard200(voice,SND_STUDIO_TYPE_STD,&prepared,resets));
        assert(!pb.mix.vDeltaAuxAL && voice->lastVolLa==0 && pb.mixerCtrl==0x80);
        voice->changed[0]=4;
        assert(PartyboardMusyxPcmRuntimePrepareStandard200(voice,SND_STUDIO_TYPE_STD,&prepared,resets));
        assert(pb.mix.vDeltaAuxBL==2 && voice->lastVolLb==320 && pb.mixerCtrl==0x8a);
        assert(voice->changed[0]==4);
        voice->volS=12;voice->volSa=34;voice->volSb=56;
        assert(PartyboardMusyxPcmRuntimeInitStandardMixer200(voice,SND_STUDIO_TYPE_STD,&prepared));
        assert(pb.mixerCtrl==7 && pb.mix.vL==voice->volL && pb.mix.vAuxAL==96);
        assert(pb.mix.vS==12 && pb.mix.vAuxAS==34 && pb.mix.vAuxBS==56);
        assert(voice->lastVolLa==96 && voice->lastVolLb==320 && voice->lastVolSb==56);
        assert(!pb.mix.vDeltaAuxBL && !pb.mix.vDeltaS && !prepared.delta[8]);
        *voice=saved_voice;
      }
      {
        int64_t block_bus[160*9]={0};
        uint16_t resets[5]={0};
        PartyboardMusyxPcmRamp block_ramp;
        _PBMIX block_mix={0};
        block_mix.vL=32767;
        PartyboardMusyxPcmRampLoad(&block_ramp,&block_mix);
        assert(PartyboardMusyxPcmRuntimeStart(&runtime,voice,&memory,65536,banks,1536));
        assert(PartyboardMusyxPcmRuntimeRenderBlock(&runtime,voice,2,&block_ramp,resets,block_bus)==28);
        assert(runtime.completion_pending && !runtime.active);
        assert(!block_bus[63*9] && block_bus[64*9]>0 && block_bus[65*9]<0);
        assert(!block_bus[92*9] && !block_bus[159*9]);
      }
      assert(PartyboardMusyxPcmRuntimeStart(&runtime,voice,&memory,65536,banks,1536));
      assert(PartyboardMusyxSampleMemoryRemove(&memory,encoded));
      assert(!PartyboardMusyxPcmRuntimeRenderPeriod(&runtime,voice,0,&volume,bus));
      assert(!runtime.active && !runtime.resampler.context);
      assert(PartyboardMusyxSampleMemoryAdd(&memory,encoded,sizeof encoded));
      assert(PartyboardMusyxPcmRuntimeStart(&runtime,voice,&memory,65536,banks,1536));
      {
        PartyboardMusyxPcmRamp ramp;
        _PBMIX mix={0};
        mix.vL=32767;mix.vDeltaL=0xffff;
        PartyboardMusyxPcmRampLoad(&ramp,&mix);
        memset(bus,0,sizeof bus);
        assert(PartyboardMusyxPcmRuntimeRenderRampPeriod(&runtime,voice,0,&ramp,0,bus)==28);
        assert(!runtime.active && ramp.gain[0]==32739);
        {
          uint32_t completion=0xffffffff;
          voice->mesgCallBackUserValue ^= 0x100;
          assert(!PartyboardMusyxPcmRuntimeTakeCompletion(&runtime,voice,&completion));
          assert(completion==0xffffffff && !runtime.completion_pending);
          voice->mesgCallBackUserValue ^= 0x100;
        }
        assert(bus[0]>0 && bus[9]<0 && !bus[28*9]);
        assert(PartyboardMusyxPcmRuntimeStart(&runtime,voice,&memory,65536,banks,1536));
      }
      assert(PartyboardMusyxSampleMemoryRemove(&memory,encoded));
      assert(PartyboardMusyxSampleMemoryAdd(&memory,encoded,sizeof encoded));
      assert(!PartyboardMusyxPcmRuntimeRenderPeriod(&runtime,voice,0,&volume,bus));
      assert(!runtime.active);
      assert(PartyboardMusyxPcmRuntimeStart(&runtime,voice,&memory,65536,banks,1536));
      voice->mesgCallBackUserValue ^= 0x100;
      memset(bus,0,sizeof bus);
      assert(!PartyboardMusyxPcmRuntimeRenderPeriod(&runtime,voice,0,&volume,bus));
      assert(!runtime.active && !runtime.resampler.context && !bus[0]);
      voice->mesgCallBackUserValue ^= 0x100;
      assert(PartyboardMusyxPcmRuntimeStart(&runtime,voice,&memory,65536,banks,1536));
      voice->smp_id ^= 1;
      assert(!PartyboardMusyxPcmRuntimeRenderPeriod(&runtime,voice,0,&volume,bus));
      assert(!runtime.active && !bus[0]);
      voice->smp_id ^= 1;
      assert(PartyboardMusyxPcmRuntimeStart(&runtime,voice,&memory,65536,banks,1536));
      voice->smp_info.addr=encoded+2;
      assert(!PartyboardMusyxPcmRuntimeRenderPeriod(&runtime,voice,0,&volume,bus));
      assert(!runtime.active && !bus[0]);
      voice->smp_info.addr=encoded;
    }
    PartyboardMusyxPcmRuntimeStop(&runtime);
    assert(!runtime.active && !runtime.resampler.context);
  }
  saved = source;
  assert(PartyboardMusyxSampleMemoryRemove(&memory, encoded));
  assert(!PartyboardMusyxVoiceSourceOpen(&source, voice, &memory));
  assert(memcmp(&source, &saved, sizeof(source)) == 0);
  assert(voice->mesgCallBackUserValue == 99);
  assert(voice->changed[0] == 0x1a0 && voice->changed[1] == 0);
  assert(voice->changed[2] == 0 && voice->changed[3] == 0x55);
  assert(voice->srcTypeSelect == 0 && voice->srcCoefSelect == 1);
  assert(voice->adsr.mode == 0 && voice->adsr.data.dls.sLevel == 0x7fff);
  {
    ADSR_VARS original = voice->adsr;
    int16_t block[32];
    unsigned sample_index;
    for (sample_index = 0; sample_index < 32; ++sample_index) block[sample_index] = 30000;
    assert(!PartyboardMusyxPcmEnvelopeStart(&envelope, NULL));
    assert(PartyboardMusyxPcmEnvelopeStart(&envelope, &voice->adsr));
    assert(memcmp(&original, &voice->adsr, sizeof(original)) == 0);
    assert(PartyboardMusyxPcmEnvelopeApply(&envelope, block, 32) == 32);
    assert(block[0] == 29999 && block[31] == 29999);
    assert(envelope.remaining == 0);
  }
  assert(voice->lastUpdate.pitch == 0xff && voice->lastUpdate.vol == 0xff);
  assert(voice->flags == 0x80000000 && voice->itdShiftL == 16 && voice->itdShiftR == 16);
  hwSetPitch(1, 4096);
  assert(voice->pitch[2] == 65536 && voice->changed[2] == 8);
  phase.ratio = voice->pitch[2];
  assert(PartyboardSrcPhaseAdvance(&phase) == 1 && phase.fraction == 0);
  hwSetPitch(1, 2048);
  phase.ratio = voice->pitch[2];
  assert(PartyboardSrcPhaseAdvance(&phase) == 0 && phase.fraction == 32768);
  assert(PartyboardSrcPhaseAdvance(&phase) == 1 && phase.fraction == 0);
  hwSetPitch(1, 4096);
  hwSetTimeOffset(3);
  voice->changed[3] = 0;
  hwSetPitch(1, 4096);
  assert(voice->changed[3] == 0 && voice->lastUpdate.pitch == 2);
  hwSetPitch(1, 0xffff);
  assert(voice->pitch[3] == 0x3fff0 && voice->lastUpdate.pitch == 3);
  hwKeyOff(1);
  assert(voice->changed[3] == 0x48);
  envelope.adsr.mode = 0;
  envelope.adsr.state = 3;
  envelope.adsr.currentVolume = 0x7fff0000;
  envelope.adsr.data.linear.rTime = 4;
  envelope.remaining = 16;
  phase.ratio = 65536;
  assert(!PartyboardMusyxPcmControlsApply(&envelope, &phase, voice, 3));
  assert(envelope.adsr.state == 3 && phase.ratio == 65536);
  envelope.remaining = 0;
  assert(!PartyboardMusyxPcmControlsApply(&envelope, &phase, voice, 5));
  assert(PartyboardMusyxPcmControlsApply(&envelope, &phase, voice, 3));
  assert(envelope.adsr.state == 4 && phase.ratio == 0x3fff0);
  assert(voice->changed[3] == 0x48);
  hwStart(1, 2);
  assert(dspStudio[2].voiceRoot == voice && voice->studio == 2);
  assert(voice->state == 1 && voice->singleOffset == 3);
  hwStart(0, 2);
  assert(dspStudio[2].voiceRoot == &voices[0]);
  assert(voices[0].next == voice && voice->prev == &voices[0]);
  hwChangeStudio(1, 3);
  assert(voices[0].next == NULL && dspStudio[3].voiceRoot == voice);
  assert(voice->prev == NULL && voice->studio == 3);
  hwSetTimeOffset(0);
  voice->state = 1;
  hwBreak(1);
  assert(voice->startupBreak == 1 && (voice->changed[0] & 0x20));
  hwSetITDMode(1, 1);
  assert(!(voice->flags & 0x80000000));
  hwOff(1);
  assert(voice->state == 0 && dspStudio[3].voiceRoot == NULL);
  hwOff(0);
  assert(dspStudio[2].voiceRoot == NULL);
  {
    ADSR_VARS expected;
    DSPvoice scheduled={0};
    PartyboardMusyxPcmEnvelope controlled={0};
    PartyboardSrcPhase scheduled_phase={0,65536};
    controlled.adsr=envelope.adsr;
    controlled.adsr.state=3;
    controlled.adsr.currentVolume=0x40000000;
    expected=controlled.adsr;
    adsrStartRelease(&expected,10);
    scheduled.changed[2]=0x68;scheduled.pitch[2]=0x40000;
    assert(PartyboardMusyxPcmControlsScheduled(&controlled,&scheduled_phase,&scheduled,2,0));
    assert(scheduled.postBreak && scheduled_phase.ratio==65536);
    assert(!memcmp(&controlled.adsr,&expected,sizeof expected));
    scheduled.changed[3]=0x48;scheduled.pitch[3]=0x40000;
    assert(PartyboardMusyxPcmControlsScheduled(&controlled,&scheduled_phase,&scheduled,3,0));
    assert(scheduled_phase.ratio==65536 && !memcmp(&controlled.adsr,&expected,sizeof expected));
    scheduled.changed[0]=8;scheduled.pitch[0]=32768;
    assert(PartyboardMusyxPcmControlsScheduled(&controlled,&scheduled_phase,&scheduled,0,1));
    assert(scheduled_phase.ratio==32768 && scheduled.postBreak);
  }
  puts("Original voice sample handoff, scheduling and pitch controls PASS");
  return 0;
}
