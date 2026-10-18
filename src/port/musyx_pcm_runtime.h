#ifndef PARTYBOARD_MUSYX_PCM_RUNTIME_H
#define PARTYBOARD_MUSYX_PCM_RUNTIME_H
#include "musyx_voice_source.h"
#include "msm_sample_resampler.h"
#include "src_linear_period.h"
#include "musyx_pcm_voice.h"
#include "musyx_studio_mix.h"
#include "musyx/sal.h"
typedef struct {
    PartyboardMsmSample sample;
    PartyboardSrcResampler resampler;
    PartyboardMusyxPcmEnvelope envelope;
    const PartyboardMusyxSampleMemory *memory;
    const void *sample_address;
    uint64_t sample_generation;
    uint32_t callback_user_value;
    uint16_t sample_id;
    const int16_t *coefficient_banks;
    size_t coefficient_bank_words;
    unsigned source_mode,coefficient_bank;
    int active;
    int completion_pending;
} PartyboardMusyxPcmRuntime;
/* Original host accounting advances one full 32-frame scheduled period and
 * leaves looped sources unchanged. This is not the decoder's consumed count. */
static inline int PartyboardMusyxPcmRuntimeAdvanceHostPeriod(DSPvoice *voice) {
    uint32_t pitch,old_lo;
    if(!voice || !voice->pb) return 0;
    if(voice->smp_info.loopLength) return 1;
    pitch=voice->pb->srcSelect!=2 ? voice->playInfo.pitch<<5 : 0x200000U;
    old_lo=voice->playInfo.posLo;
    voice->playInfo.posLo+=pitch*0x10000U;
    voice->playInfo.posHi+=(pitch>>16)+(old_lo>voice->playInfo.posLo);
    return 1;
}
/* Keep destination stable while active. Startup is transactional and selects
 * only the requested bank from caller-owned coefficients. The production
 * default is an explicitly identified open-source replacement, not ROM data. */
static inline int PartyboardMusyxPcmRuntimeStart(PartyboardMusyxPcmRuntime *destination,
        const DSPvoice *voice,const PartyboardMusyxSampleMemory *memory,
        uint32_t ratio,const int16_t *banks,size_t bank_words) {
    PartyboardMusyxPcmRuntime result={0};
    if(!destination || !voice || voice->srcTypeSelect>2
            || voice->srcCoefSelect>=3 || ratio>0x3fffU*16U
            || (voice->srcTypeSelect==0 && (!banks || bank_words<1536))) return 0;
    if(!PartyboardMusyxVoiceSourceOpen(&result.sample,voice,memory)
            || !PartyboardMusyxPcmEnvelopeStart(&result.envelope,&voice->adsr)) return 0;
    if(voice->srcTypeSelect==0) {
        if(!PartyboardMsmSampleResamplerInit(&result.resampler,&result.sample,
            ratio,banks+voice->srcCoefSelect*512,512)) return 0;
    } else {
        result.resampler.phase.ratio=ratio;
        result.resampler.read=PartyboardMsmResamplerReadSample;
        result.resampler.at_end=PartyboardMsmResamplerAtEnd;
        if(banks && bank_words>=1536) {
            result.resampler.coefficients=banks+voice->srcCoefSelect*512;
            result.resampler.coefficient_count=512;
        }
    }
    result.active=1;
    result.memory=memory;result.sample_address=voice->smp_info.addr;
    result.sample_generation=PartyboardMusyxSampleMemoryGeneration(memory,result.sample_address);
    result.callback_user_value=voice->mesgCallBackUserValue;
    result.sample_id=voice->smp_id;
    result.coefficient_banks=banks;result.coefficient_bank_words=bank_words;
    result.source_mode=voice->srcTypeSelect;result.coefficient_bank=voice->srcCoefSelect;
    result.resampler.mode=result.source_mode;
    *destination=result;
    /* Fix the callback address after moving the temporary startup state. */
    destination->resampler.context=&destination->sample;
    return 1;
}
static inline void PartyboardMusyxPcmRuntimeStop(PartyboardMusyxPcmRuntime *runtime) {
    if(runtime) memset(runtime,0,sizeof(*runtime));
}
static inline int PartyboardMusyxPcmRuntimeRegistered(PartyboardMusyxPcmRuntime *runtime) {
    if(!runtime || !runtime->active) return 0;
    if(!runtime->sample_generation || PartyboardMusyxSampleMemoryGeneration(runtime->memory,
            runtime->sample_address)!=runtime->sample_generation) {
        PartyboardMusyxPcmRuntimeStop(runtime);
        return 0;
    }
    return 1;
}
/* Original synthesis uses the full callback voice ID to reject stale sample
 * completion. Keep the same identity across rendering of a reused slot. */
static inline int PartyboardMusyxPcmRuntimeVoiceMatches(PartyboardMusyxPcmRuntime *runtime,
        const DSPvoice *voice) {
    if(!PartyboardMusyxPcmRuntimeRegistered(runtime)) return 0;
    if(!voice || voice->mesgCallBackUserValue!=runtime->callback_user_value
            || voice->smp_id!=runtime->sample_id || voice->smp_info.addr!=runtime->sample_address) {
        PartyboardMusyxPcmRuntimeStop(runtime);
        return 0;
    }
    return 1;
}
/* Original continuation flags select the next block's mode/bank. Preserve
 * phase/history and reject unsupported modes before any selection mutation.
 * Caller retains all coefficient banks for the complete voice lifetime. */
static inline int PartyboardMusyxPcmRuntimeSelectSource(PartyboardMusyxPcmRuntime *runtime,
        DSPvoice *voice,int startup) {
    unsigned mode,bank;
    if(!runtime || !voice || !voice->pb || !runtime->active) return 0;
    mode=(startup || (voice->changed[0]&0x100)) ? voice->srcTypeSelect : runtime->source_mode;
    bank=(startup || (voice->changed[0]&0x80)) ? voice->srcCoefSelect : runtime->coefficient_bank;
    if(mode>2 || bank>=3 || (mode==0 && (!runtime->coefficient_banks
            || runtime->coefficient_bank_words<1536))) return 0;
    runtime->resampler.coefficients=runtime->coefficient_banks && runtime->coefficient_bank_words>=1536
        ? runtime->coefficient_banks+bank*512 : NULL;
    runtime->resampler.coefficient_count=runtime->resampler.coefficients ? 512 : 0;
    runtime->source_mode=mode;runtime->coefficient_bank=bank;
    runtime->resampler.mode=mode;
    voice->pb->srcSelect=(uint16_t)mode;voice->pb->coefSelect=(uint16_t)bank;
    return 1;
}
/* Consume natural completion once. The scheduler must still send original
 * message 0 and deactivate the studio voice; invalidation is not completion. */
static inline int PartyboardMusyxPcmRuntimeTakeCompletion(PartyboardMusyxPcmRuntime *runtime,
        const DSPvoice *voice,uint32_t *callback_user_value) {
    if(!runtime || !runtime->completion_pending || !voice || !callback_user_value) return 0;
    if(voice->mesgCallBackUserValue!=runtime->callback_user_value
            || voice->smp_id!=runtime->sample_id || voice->smp_info.addr!=runtime->sample_address
            || PartyboardMusyxSampleMemoryGeneration(runtime->memory,runtime->sample_address)
                !=runtime->sample_generation) {
        PartyboardMusyxPcmRuntimeStop(runtime);
        return 0;
    }
    *callback_user_value=runtime->callback_user_value;
    runtime->completion_pending=0;
    return 1;
}
/* Match original natural completion ordering: notify, then unlink the voice.
 * Invoke from the synthesis scheduler, never from the audio device callback. */
static inline int PartyboardMusyxPcmRuntimeNotifyCompletion(PartyboardMusyxPcmRuntime *runtime,
        DSPvoice *voice) {
    uint32_t callback_user_value;
    if(!voice || !voice->state) return 0;
    if(!PartyboardMusyxPcmRuntimeTakeCompletion(runtime,voice,&callback_user_value)) return 0;
    salSynthSendMessage(voice,0);
    salDeactivateVoice(voice);
    return 1;
}
/* Write the verified original ratio/fraction/history layout. Decoder address,
 * ADPCM loop predictors and host play-position accounting are separate. */
static inline int PartyboardMusyxPcmRuntimeStoreSourceState(
        const PartyboardMusyxPcmRuntime *runtime,DSPvoice *voice) {
    unsigned tap;
    if(!runtime || !voice || !voice->pb || (!runtime->active && !runtime->completion_pending)
            || voice->mesgCallBackUserValue!=runtime->callback_user_value
            || voice->smp_id!=runtime->sample_id || voice->smp_info.addr!=runtime->sample_address
            || runtime->resampler.phase.fraction>65535
            || runtime->resampler.phase.ratio>0x3fffU*16U
            || PartyboardMusyxSampleMemoryGeneration(runtime->memory,runtime->sample_address)
                !=runtime->sample_generation) return 0;
    voice->pb->src.ratioHi=(uint16_t)(runtime->resampler.phase.ratio>>16);
    voice->pb->src.ratioLo=(uint16_t)runtime->resampler.phase.ratio;
    voice->pb->src.currentAddressFrac=(uint16_t)runtime->resampler.phase.fraction;
    for(tap=0;tap<4;++tap) voice->pb->src.last_samples[tap]=(uint16_t)runtime->resampler.history[tap];
    return 1;
}
/* Caller reports completion back to original synthesis and preserves registry
 * lifetime. Never touch retained payload after its registration disappears. */
static inline size_t PartyboardMusyxPcmRuntimeRenderPeriod(PartyboardMusyxPcmRuntime *runtime,
        const DSPvoice *voice,unsigned offset,const SAL_VOLINFO *volume,int64_t *bus) {
    size_t rendered;
    if(runtime && runtime->source_mode==1) return 0; /* Requires planned block scratch. */
    if(!PartyboardMusyxPcmRuntimeVoiceMatches(runtime,voice)) return 0;
    rendered=PartyboardMusyxPcmVoiceRenderPeriod(&runtime->resampler,&runtime->envelope,
        voice,offset,volume,bus);
    if(runtime->resampler.ended || runtime->envelope.finished) {
        runtime->active=0;runtime->completion_pending=1;
    }
    return rendered;
}
static inline size_t PartyboardMusyxPcmRuntimeRenderRampPeriod(PartyboardMusyxPcmRuntime *runtime,
        const DSPvoice *voice,unsigned offset,PartyboardMusyxPcmRamp *ramp,
        uint16_t reset_flags,int64_t *bus) {
    size_t rendered;
    if(runtime && runtime->source_mode==1) return 0;
    if(!PartyboardMusyxPcmRuntimeVoiceMatches(runtime,voice)) return 0;
    rendered=PartyboardMusyxPcmVoiceRenderRampPeriod(&runtime->resampler,&runtime->envelope,
        voice,offset,ramp,reset_flags,bus);
    if(runtime->resampler.ended || runtime->envelope.finished) {
        runtime->active=0;runtime->completion_pending=1;
    }
    return rendered;
}
/* Standard-studio numerical scheduling adapter. All groups must be enabled
 * by the caller's original mixerCtrl routing; this does not implement inactive
 * aux gating or DPL2, whose auxB surround component is not scheduled here. */
static inline int PartyboardMusyxPcmRuntimeScheduleStandardVolumeActive(DSPvoice *voice,
        PartyboardMusyxPcmRamp *ramp,unsigned active_groups,uint16_t reset_flags[5]) {
    uint16_t last[9],target[9];
    if(!voice || !ramp || !reset_flags) return 0;
    last[0]=voice->lastVolL;last[1]=voice->lastVolR;last[2]=voice->lastVolS;
    last[3]=voice->lastVolLa;last[4]=voice->lastVolRa;last[5]=voice->lastVolSa;
    last[6]=voice->lastVolLb;last[7]=voice->lastVolRb;last[8]=voice->lastVolSb;
    target[0]=voice->volL;target[1]=voice->volR;target[2]=voice->volS;
    target[3]=voice->volLa;target[4]=voice->volRa;target[5]=voice->volSa;
    target[6]=voice->volLb;target[7]=voice->volRb;target[8]=voice->volSb;
    PartyboardMusyxPcmRampScheduleActive(ramp,last,target,voice->changed[0]&7,active_groups,reset_flags);
    voice->lastVolL=last[0];voice->lastVolR=last[1];voice->lastVolS=last[2];
    voice->lastVolLa=last[3];voice->lastVolRa=last[4];voice->lastVolSa=last[5];
    voice->lastVolLb=last[6];voice->lastVolRb=last[7];voice->lastVolSb=last[8];
    return 1;
}
static inline int PartyboardMusyxPcmRuntimeScheduleStandardVolume(DSPvoice *voice,
        PartyboardMusyxPcmRamp *ramp,uint16_t reset_flags[5]) {
    return PartyboardMusyxPcmRuntimeScheduleStandardVolumeActive(voice,ramp,7,reset_flags);
}
/* Explicitly scoped to original MusyX 2.0.0 standard studios. Other studio
 * modes/versions have different enable bits and require their own adapter. */
static inline int PartyboardMusyxPcmRuntimeInitStandardMixer200(DSPvoice *voice,
        SND_STUDIO_TYPE studio_type,PartyboardMusyxPcmRamp *ramp) {
    if(!voice || !voice->pb || !ramp || studio_type!=SND_STUDIO_TYPE_STD) return 0;
    memset(ramp,0,sizeof(*ramp));
    ramp->gain[0]=voice->lastVolL=voice->volL;
    ramp->gain[1]=voice->lastVolR=voice->volR;
    ramp->gain[2]=voice->lastVolS=voice->volS;
    ramp->gain[3]=voice->lastVolLa=voice->volLa;
    ramp->gain[4]=voice->lastVolRa=voice->volRa;
    ramp->gain[5]=voice->lastVolSa=voice->volSa;
    ramp->gain[6]=voice->lastVolLb=voice->volLb;
    ramp->gain[7]=voice->lastVolRb=voice->volRb;
    ramp->gain[8]=voice->lastVolSb=voice->volSb;
    voice->pb->mixerCtrl=0;
    if(ramp->gain[3] || ramp->gain[4] || ramp->gain[5]) voice->pb->mixerCtrl|=1;
    if(ramp->gain[6] || ramp->gain[7] || ramp->gain[8]) voice->pb->mixerCtrl|=2;
    if(ramp->gain[2] || ramp->gain[5] || ramp->gain[8]) voice->pb->mixerCtrl|=4;
    PartyboardMusyxPcmRampStore(ramp,&voice->pb->mix);
    return 1;
}
static inline int PartyboardMusyxPcmRuntimePrepareStandard200(DSPvoice *voice,
        SND_STUDIO_TYPE studio_type,PartyboardMusyxPcmRamp *ramp,uint16_t reset_flags[5]) {
    unsigned active_groups;
    if(!voice || !voice->pb || !ramp || !reset_flags || studio_type!=SND_STUDIO_TYPE_STD) return 0;
    active_groups=1U|((voice->pb->mixerCtrl&1U)?2U:0U)|((voice->pb->mixerCtrl&2U)?4U:0U);
    PartyboardMusyxPcmRampLoad(ramp,&voice->pb->mix);
    PartyboardMusyxPcmRuntimeScheduleStandardVolumeActive(voice,ramp,active_groups,reset_flags);
    voice->pb->mixerCtrl=PartyboardMusyxPcmRampStandardControl200(ramp,voice->changed[0]&7,
            voice->pb->mixerCtrl);
    PartyboardMusyxPcmRampStore(ramp,&voice->pb->mix);
    return 1;
}
/* Host scheduling does not depend on decoded sample availability. The caller
 * consumes the returned envelope period before scheduling another one. */
static inline int PartyboardMusyxPcmScheduleHostPeriod(
        PartyboardMusyxPcmEnvelope *envelope,PartyboardSrcPhase *phase,
        DSPvoice *voice,unsigned offset,int first_period) {
    if(!envelope || !phase || !voice || !voice->pb || envelope->remaining
            || envelope->finished || envelope->done_after_block) return 0;
    if(!PartyboardMusyxPcmControlsScheduled(envelope,phase,voice,offset,first_period)) return 0;
    if(!PartyboardMusyxPcmEnvelopeBeginPeriod(envelope)) return 0;
    voice->adsr=envelope->adsr;
    return PartyboardMusyxPcmRuntimeAdvanceHostPeriod(voice);
}
/* Render this voice into the original five 32-frame periods. first_period is
 * supplied by startup scheduling; bus is an existing 160-frame accumulator.
 * Completion stays pending until the scheduler finishes traversing voices. */
static inline size_t PartyboardMusyxPcmRuntimeRenderScheduledPeriod(
        PartyboardMusyxPcmRuntime *runtime,DSPvoice *voice,unsigned offset,int first_period,
        PartyboardMusyxPcmRamp *ramp,uint16_t reset_flags,int64_t *bus) {
    size_t rendered;
    if(runtime && runtime->source_mode==1) return 0;
    if(!ramp || !bus || !voice || !voice->pb
            || !PartyboardMusyxPcmRuntimeVoiceMatches(runtime,voice)) return 0;
    if(runtime->resampler.ended || runtime->envelope.finished) return 0;
    if(!PartyboardMusyxPcmScheduleHostPeriod(&runtime->envelope,&runtime->resampler.phase,
                voice,offset,first_period)) return 0;
    rendered=PartyboardMusyxPcmVoiceRenderRampPrepared(&runtime->resampler,&runtime->envelope,
            ramp,reset_flags,bus);
    if(runtime->resampler.ended || runtime->envelope.finished) {
        runtime->active=0;runtime->completion_pending=1;
    }
    return rendered;
}
static inline size_t PartyboardMusyxPcmRuntimeRenderBlock(PartyboardMusyxPcmRuntime *runtime,
        const DSPvoice *voice,unsigned first_period,PartyboardMusyxPcmRamp *ramp,
        const uint16_t reset_flags[5],int64_t *bus) {
    unsigned period;
    size_t total=0;
    if(!runtime || !voice || !ramp || !reset_flags || !bus || first_period>=5) return 0;
    for(period=first_period;period<5;++period)
        if((voice->changed[period]&8) && voice->pitch[period]>0x3fffU*16U) return 0;
    for(period=first_period;period<5 && runtime->active;++period) {
        size_t rendered=PartyboardMusyxPcmRuntimeRenderRampPeriod(runtime,voice,period,
                ramp,reset_flags[period],bus+period*32*9);
        total+=rendered;
        if(rendered!=32) break;
    }
    return total;
}
/* Coherent numerical standard-studio block path. Startup/source ownership and
 * completion notification remain scheduler-owned; no device callback work. */
static inline int PartyboardMusyxPcmRuntimeScheduledBlockValid(const DSPvoice *voice,
        unsigned first_period) {
    unsigned period;
    int breaking;
    if(!voice || first_period>=5) return 0;
    breaking=voice->postBreak!=0;
    for(period=first_period;period<5;++period) {
        uint32_t changes=voice->changed[period];
        int regular=period==first_period || (!(changes&0x20) && !breaking);
        if(regular && (changes&8) && voice->pitch[period]>0x3fffU*16U) return 0;
        if(period!=first_period && (changes&0x20)) breaking=1;
    }
    return 1;
}
typedef struct {
    uint32_t ratio;
    int32_t volume,delta;
    int done;
} PartyboardMusyxHostPeriod;
typedef struct {
    PartyboardMusyxHostPeriod periods[5];
    unsigned count;
    int source_done,envelope_done;
} PartyboardMusyxHostBlock;
/* Plan the original host block without reading PCM. Recorded period state is
 * for a separate renderer; advancing control time does not invent DSP tails. */
static inline int PartyboardMusyxPcmScheduleHostBlock(
        PartyboardMusyxPcmEnvelope *envelope,PartyboardSrcPhase *phase,DSPvoice *voice,
        unsigned first_period,int startup,PartyboardMusyxHostBlock *block) {
    unsigned period;
    if(!envelope || !phase || !voice || !voice->pb || !block || first_period>=5
            || envelope->remaining) return 0;
    memset(block,0,sizeof *block);
    if(!startup && !voice->smp_info.loopLength
            && voice->playInfo.posHi>=voice->smp_info.length) {
        block->source_done=1;return 1;
    }
    if(!startup && (voice->changed[0]&0x10)) {
        PartyboardMusyxPcmEnvelope reset;
        if(!PartyboardMusyxPcmRuntimeScheduledBlockValid(voice,first_period)) return 0;
        if(!PartyboardMusyxPcmEnvelopeStart(&reset,&voice->adsr)) {
            block->envelope_done=1;return 1;
        }
        *envelope=reset;
    }
    if(envelope->finished || envelope->done_after_block) {
        block->envelope_done=1;return 1;
    }
    if(!PartyboardMusyxPcmRuntimeScheduledBlockValid(voice,first_period)) return 0;
    for(period=first_period;period<5;++period) {
        PartyboardMusyxHostPeriod *record=&block->periods[period];
        if(!PartyboardMusyxPcmScheduleHostPeriod(envelope,phase,voice,period,
                    period==first_period)) return 0;
        record->ratio=phase->ratio;record->volume=envelope->volume;
        record->delta=envelope->delta;record->done=envelope->done_after_block;
        if(period==first_period) {
            voice->pb->ve.currentVolume=(uint16_t)record->volume;
            voice->pb->ve.currentDelta=(uint16_t)record->delta;
        }
        ++block->count;
        envelope->remaining=0;
        if(record->done) {
            envelope->finished=1;block->envelope_done=1;break;
        }
    }
    return 1;
}
/* Consume scheduled control snapshots without ADSR or host scheduling calls.
 * Finite source tails are deliberately left untouched until DSP semantics are
 * verified. The host plan, not decoded count, owns completion notification. */
static inline size_t PartyboardMusyxPcmRenderHostPlanMode(
        PartyboardSrcResampler *source,const PartyboardMusyxHostBlock *block,
        unsigned first_period,PartyboardMusyxPcmRamp *ramp,
        const uint16_t resets[5],int64_t *bus,int stopped_input) {
    unsigned period;
    uint16_t scratch_pointer=0x0ce0;
    size_t total=0;
    if(!source || !block || !ramp || !resets || !bus || first_period>=5
            || block->count>5-first_period || (block->source_done && block->count)) return 0;
    for(period=first_period;period<first_period+block->count;++period)
        if(block->periods[period].ratio>0x3fffU*16U) return 0;
    for(period=first_period;period<first_period+block->count && !source->ended;++period) {
        const PartyboardMusyxHostPeriod *record=&block->periods[period];
        PartyboardMusyxPcmEnvelope playback={0};
        int16_t pcm[32];
        size_t decoded;
        source->phase.ratio=record->ratio;
        playback.volume=record->volume;playback.delta=record->delta;
        playback.remaining=32;playback.done_after_block=record->done;
        decoded=source->mode==1 ? PartyboardSrcReadLinearPeriod(source,pcm,scratch_pointer,scratch_pointer)
            : source->mode==2 ? PartyboardSrcResamplerReadBypassPeriod(source,pcm,32)
            : stopped_input ? PartyboardSrcResamplerReadStoppedPeriod(source,pcm,32)
                             : PartyboardSrcResamplerRead(source,pcm,32);
        if(decoded==32) scratch_pointer+=32;
        PartyboardMusyxPcmEnvelopeApply(&playback,pcm,decoded);
        PartyboardMusyxPcmRampReset(ramp,resets[period]);
        PartyboardMusyxPcmRampMix(ramp,bus+period*32*9,pcm,decoded);
        total+=decoded;
        if(decoded!=32) break;
    }
    return total;
}
static inline size_t PartyboardMusyxPcmRenderHostPlan(
        PartyboardSrcResampler *source,const PartyboardMusyxHostBlock *block,
        unsigned first_period,PartyboardMusyxPcmRamp *ramp,
        const uint16_t resets[5],int64_t *bus) {
    return PartyboardMusyxPcmRenderHostPlanMode(source,block,first_period,ramp,resets,bus,0);
}
static inline size_t PartyboardMusyxPcmRuntimeRenderScheduledBlock(
        PartyboardMusyxPcmRuntime *runtime,DSPvoice *voice,unsigned first_period,
        PartyboardMusyxPcmRamp *ramp,const uint16_t resets[5],int64_t *bus) {
    unsigned period;
    size_t total=0;
    if(!runtime || !ramp || !resets || !bus
            || !PartyboardMusyxPcmRuntimeScheduledBlockValid(voice,first_period)) return 0;
    for(period=first_period;period<5 && runtime->active;++period) {
        size_t rendered=PartyboardMusyxPcmRuntimeRenderScheduledPeriod(runtime,voice,period,
                period==first_period,ramp,resets[period],bus+period*32*9);
        total+=rendered;
        if(rendered!=32) break;
    }
    return total;
}
static inline size_t PartyboardMusyxPcmRuntimeRenderStandard200(
        PartyboardMusyxPcmRuntime *runtime,DSPvoice *voice,SND_STUDIO_TYPE studio_type,
        unsigned first_period,int mixer_startup,int64_t *bus) {
    PartyboardMusyxPcmRamp ramp;
    uint16_t resets[5]={0};
    size_t rendered;
    if(runtime && runtime->source_mode==1) return 0;
    if(!runtime || !voice || !voice->pb || !bus || studio_type!=SND_STUDIO_TYPE_STD
            || first_period>=5 || runtime->envelope.remaining) return 0;
    if(!PartyboardMusyxPcmRuntimeScheduledBlockValid(voice,first_period)) return 0;
    if(!PartyboardMusyxPcmRuntimeVoiceMatches(runtime,voice)) return 0;
    if(mixer_startup) PartyboardMusyxPcmRuntimeInitStandardMixer200(voice,studio_type,&ramp);
    else PartyboardMusyxPcmRuntimePrepareStandard200(voice,studio_type,&ramp,resets);
    rendered=PartyboardMusyxPcmRuntimeRenderScheduledBlock(runtime,voice,first_period,&ramp,resets,bus);
    PartyboardMusyxPcmRampStore(&ramp,&voice->pb->mix);
    PartyboardMusyxPcmRuntimeStoreSourceState(runtime,voice);
    return rendered;
}
/* Separate original host lifetime from decoded source lifetime. Source end
 * remains active until the original continuation gate reports completion.
 * Missing DSP tail/studio effects fidelity still prevents production enable. */
static inline size_t PartyboardMusyxPcmRuntimeRenderPlannedStandard200(
        PartyboardMusyxPcmRuntime *runtime,DSPvoice *voice,SND_STUDIO_TYPE studio_type,
        unsigned first_period,int startup,int64_t *bus) {
    PartyboardMusyxHostBlock plan;
    PartyboardMusyxPcmRamp ramp;
    PartyboardSrcPhase host_phase;
    uint16_t resets[5]={0};
    size_t rendered;
    if(!runtime || !voice || !voice->pb || !bus || studio_type!=SND_STUDIO_TYPE_STD
            || first_period>=5 || runtime->envelope.remaining) return 0;
    if(!PartyboardMusyxPcmRuntimeVoiceMatches(runtime,voice)) return 0;
    host_phase=runtime->resampler.phase;
    /* Validate selection on copies before the host planner mutates ADSR/time.
     * Original finite completion takes precedence over unused controls. */
    if(startup || voice->smp_info.loopLength
            || voice->playInfo.posHi<voice->smp_info.length) {
        PartyboardMusyxPcmRuntime selection=*runtime;
        DSPvoice selection_voice=*voice;
        _PB selection_pb=*voice->pb;
        selection_voice.pb=&selection_pb;
        if(!PartyboardMusyxPcmRuntimeSelectSource(&selection,&selection_voice,startup)) return 0;
    }
    if(!PartyboardMusyxPcmScheduleHostBlock(&runtime->envelope,&host_phase,voice,
                first_period,startup,&plan)) return 0;
    if(plan.source_done || !plan.count) {
        if(plan.source_done || plan.envelope_done) {
            runtime->active=0;runtime->completion_pending=1;
        }
        return 0;
    }
    if(!PartyboardMusyxPcmRuntimeSelectSource(runtime,voice,startup)) return 0;
    if(startup) PartyboardMusyxPcmRuntimeInitStandardMixer200(voice,studio_type,&ramp);
    else PartyboardMusyxPcmRuntimePrepareStandard200(voice,studio_type,&ramp,resets);
    if(startup) voice->pb->state=1;
    rendered=PartyboardMusyxPcmRenderHostPlanMode(&runtime->resampler,&plan,first_period,
            &ramp,resets,bus,1);
    if(runtime->resampler.ended) voice->pb->state=0;
    if(plan.envelope_done) { runtime->active=0;runtime->completion_pending=1; }
    PartyboardMusyxPcmRampStore(&ramp,&voice->pb->mix);
    PartyboardMusyxPcmRuntimeStoreSourceState(runtime,voice);
    return rendered;
}
/* Coherent voice-to-studio adapter. Its scratch belongs to one voice, so
 * residuals cannot accidentally include another voice or output clipping.
 * Caller still owns studio traversal, completion/depop and output scheduling;
 * this does not enable production MusyX or supply missing ROM coefficients.
 * Interaural delay needs separate left/right source buffers and is rejected. */
static inline size_t PartyboardMusyxPcmRuntimeRenderStudio200(
        PartyboardMusyxPcmRuntime *runtime, DSPvoice *voice,
        SND_STUDIO_TYPE studio_type, unsigned first_period, int startup, int32_t *bus) {
    int64_t products[160 * 9] = {0};
    size_t rendered;
    if(!runtime || !voice || !voice->pb || !bus || first_period >= 5
            || studio_type != SND_STUDIO_TYPE_STD || (voice->pb->mixerCtrl & 0xfff0U)
            || (voice->flags & 0x80000000U) || voice->pb->itd.flag) return 0;
    rendered = PartyboardMusyxPcmRuntimeRenderPlannedStandard200(runtime, voice,
        studio_type, first_period, startup, products);
    if(rendered && !PartyboardMusyxStudioMixVoice200(bus + first_period * 32 * 9,
            products + first_period * 32 * 9, rendered, voice->pb)) return 0;
    return rendered;
}
#endif
