#ifndef PARTYBOARD_MUSYX_SOFTWARE_H
#define PARTYBOARD_MUSYX_SOFTWARE_H
#include "musyx_pcm_output.h"
#include "musyx_group_runtime.h"
#include "musyx_song_runtime.h"
#include "musyx_effect_parameters.h"
#include "musyx_aux_runtime.h"
#include "musyx_emitter_runtime.h"
#include "musyx_control_fade.h"
#if MUSY_VERSION != MUSY_VERSION_CHECK(2,0,0)
#error The software audio owner requires the linked MusyX2.0.0 ABI
#endif

/* One simulation-thread SDK owner. Stable, zeroed, heap/static storage only.
 * Borrowed private container outlives Shutdown. No console DSP/AI interrupt
 * backend is started; every original control period runs through the driver. */
typedef struct {
    uint32_t voice;
    int handle, effect, status, positional, continuous;
    unsigned volume, base_volume;
    PartyboardMusyxControlFade fade, pause;
    PartyboardMusyxEmitter emitter;
    SND_FVECTOR position, direction;
} PartyboardMusyxSoftwareEffectSlot;
typedef struct {
    int active, last_error;
    PartyboardMsmContainer container;
    PartyboardMsmContainerInfo info;
    DSPvoice voices[64]; _PB pbs[64]; _SPB spb;
    int32_t main[2][480], aux_buffers[2][3][480], surround[160], scratch[160*9];
    PartyboardMusyxSampleMemory memory;
    PartyboardMusyxVoiceScheduler scheduler;
    PartyboardMusyxBlockDriver driver;
    PartyboardMusyxStudioProcessor processor;
    PartyboardMusyxPcmBridge bridge;
    PartyboardPcmOutput output;
    PartyboardMusyxAuxRuntime aux;
    PartyboardMusyxGroup groups[128];
    PartyboardMsmContainerGroup descriptors[128];
    unsigned indices[128], loaded, base_loaded;
    PartyboardMusyxSong songs[4];
    PartyboardMusyxSoftwareEffectSlot effects[64];
    SND_LISTENER listener;
    int listener_active;
    float listener_distance;
    uint16_t emitter_group;
    PartyboardMusyxControlFade song_pause[4];
    PartyboardMusyxControlFade song_fade[4];
    unsigned song_volume[4];
    int song_paused[4];
    uint64_t periodic_calls, positional_starts;
    int next_handle;
    uint64_t effect_starts, song_starts, nonzero_samples;
} PartyboardMusyxSoftware;

static inline int PartyboardMusyxSoftwareLoadIndex(PartyboardMusyxSoftware *s, unsigned index) {
    PartyboardMsmContainerGroup descriptor;
    if(!s || !s->active || index>=s->info.groups || s->loaded>=128) return 0;
    for(unsigned i=0;i<s->loaded;++i) if(s->indices[i]==index) return 1;
    if(!PartyboardMsmContainerGetGroup(&descriptor,&s->container,index) || !descriptor.data_bytes || !descriptor.sample_bytes) return 0;
    if(!PartyboardMusyxGroupPush(&s->groups[s->loaded],&s->memory,descriptor.data,descriptor.data_bytes,
            (uint8_t*)descriptor.samples,descriptor.sample_bytes,(uint16_t)descriptor.id)) return 0;
    s->indices[s->loaded]=index; s->descriptors[s->loaded++]=descriptor; return 1;
}
static inline int PartyboardMusyxSoftwareLoadId(PartyboardMusyxSoftware *s, unsigned id) {
    PartyboardMsmContainerGroup descriptor;
    if(!s || !s->active) return 0;
    for(unsigned i=0;i<s->loaded;++i) if(s->descriptors[i].id==id) return 1;
    for(unsigned i=0;i<s->info.groups;++i) {
        if(!PartyboardMsmContainerGetGroup(&descriptor,&s->container,i)) return 0;
        if(descriptor.id==id && descriptor.data_bytes && descriptor.sample_bytes)
            return PartyboardMusyxSoftwareLoadIndex(s,i);
    }
    return 0;
}
static inline void PartyboardMusyxSoftwareStopSong(PartyboardMusyxSoftware *s,unsigned channel) {
    if(s && s->active && channel<4) {
        if(s->songs[channel].arrangement.data) PartyboardMusyxSongStop(&s->songs[channel]);
        memset(&s->song_pause[channel],0,sizeof s->song_pause[channel]);
        memset(&s->song_fade[channel],0,sizeof s->song_fade[channel]);
        s->song_paused[channel]=0;s->song_volume[channel]=0;
    }
}
static inline int PartyboardMusyxSoftwareFadeSong(PartyboardMusyxSoftware *s,unsigned channel,int32_t milliseconds) {
    int32_t periods=PartyboardMusyxControlPeriods(milliseconds);
    if(!s || !s->active || channel>=4 || !s->songs[channel].arrangement.data) return 0;
    if(periods) PartyboardMusyxControlFadeStart(&s->song_fade[channel],periods,1);
    else PartyboardMusyxSoftwareStopSong(s,channel);
    return 1;
}
static inline int PartyboardMusyxSoftwareEffectAlive(PartyboardMusyxSoftwareEffectSlot *e) {
    if(!e || !e->handle || !e->status) return 0;
    if(e->continuous) {
        if(sndCheckEmitter(&e->emitter.emitter)) { e->voice=sndEmitterVoiceID(&e->emitter.emitter);return 1; }
        e->emitter.active=0;
    } else if(sndFXCheck(e->voice)!=UINT32_MAX) return 1;
    e->status=0;return 0;
}
static inline PartyboardMusyxSoftwareEffectSlot *PartyboardMusyxSoftwareFindEffect(PartyboardMusyxSoftware *s,int handle) {
    if(s && s->active && handle>0) for(unsigned i=0;i<64;++i)
        if(s->effects[i].handle==handle && PartyboardMusyxSoftwareEffectAlive(&s->effects[i])) return &s->effects[i];
    return NULL;
}
static inline void PartyboardMusyxSoftwareApplyVolume(PartyboardMusyxSoftwareEffectSlot *e) {
    unsigned volume=PartyboardMusyxControlVolume(e->base_volume,e->volume,e->fade.volume,e->pause.volume);
    if(e->continuous) sndUpdateEmitter(&e->emitter.emitter,&e->position,&e->direction,(u8)volume,NULL);
    else sndFXVolume(e->voice,(u8)volume);
}
static inline void PartyboardMusyxSoftwareStopSlot(PartyboardMusyxSoftwareEffectSlot *e) {
    e->fade.total=0;e->status=1;
    if(e->continuous) {
        if(e->emitter.active && sndCheckEmitter(&e->emitter.emitter)) PartyboardMusyxEmitterStop(&e->emitter);
        e->emitter.active=0;e->status=0;
    } else if(sndFXCheck(e->voice)!=UINT32_MAX) sndFXKeyOff(e->voice);
}
static inline int PartyboardMusyxSoftwareStopEffect(PartyboardMusyxSoftware *s,int handle,int32_t milliseconds) {
    PartyboardMusyxSoftwareEffectSlot *e=PartyboardMusyxSoftwareFindEffect(s,handle);
    int32_t periods=PartyboardMusyxControlPeriods(milliseconds);
    if(!e) return 0;
    if(periods) PartyboardMusyxControlFadeStart(&e->fade,periods,1);
    else PartyboardMusyxSoftwareStopSlot(e);
    return 1;
}
static inline void PartyboardMusyxSoftwareStopEffects(PartyboardMusyxSoftware *s,int effect) {
    if(s && s->active) for(unsigned i=0;i<64;++i)
        if((effect<0 || s->effects[i].effect==effect) && PartyboardMusyxSoftwareEffectAlive(&s->effects[i]))
            PartyboardMusyxSoftwareStopSlot(&s->effects[i]);
}
static inline int PartyboardMusyxSoftwarePauseEffect(PartyboardMusyxSoftware *s,int handle,int pause,int32_t milliseconds) {
    PartyboardMusyxSoftwareEffectSlot *e=PartyboardMusyxSoftwareFindEffect(s,handle);
    int32_t periods=PartyboardMusyxControlPeriods(milliseconds);
    if(!e || e->status==1) return 0;
    if(pause && e->status==2) {
        PartyboardMusyxControlFadeStart(&e->pause,periods,1);e->status=3;
        if(!periods) {e->pause.volume=0;PartyboardMusyxSoftwareApplyVolume(e);}
    } else if(!pause && e->status==3) {
        if(periods && e->pause.total) e->pause.total=-periods;
        else PartyboardMusyxControlFadeStart(&e->pause,periods,0);
        e->status=2;
        if(!periods) {e->pause.volume=127;PartyboardMusyxSoftwareApplyVolume(e);}
    }
    return 1;
}
static inline int PartyboardMusyxSoftwareFiniteVector(const SND_FVECTOR *p) {
    return p && isfinite(p->x) && isfinite(p->y) && isfinite(p->z);
}
static inline void PartyboardMusyxSoftwareRemoveListener(PartyboardMusyxSoftware *s) {
    if(s && s->active && s->listener_active) {sndRemoveListener(&s->listener);s->listener_active=0;}
}
static inline int PartyboardMusyxSoftwareListener(PartyboardMusyxSoftware *s,const SND_FVECTOR *position,
        const SND_FVECTOR *heading,float distance,float speed,float start,float front,float back) {
    SND_FVECTOR zero={0},up={0,1,0},p,h;
    if(!s || !s->active || !PartyboardMusyxSoftwareFiniteVector(position) || !PartyboardMusyxSoftwareFiniteVector(heading)
            || !(heading->x*heading->x+heading->z*heading->z>0) || !isfinite(distance) || !(distance>0)
            || !isfinite(speed) || !(speed>0) || !isfinite(start) || !isfinite(front) || !isfinite(back) || front<0 || back<0) return 0;
    p=*position;h=*heading;PartyboardMusyxSoftwareRemoveListener(s);
    s->listener_active=sndAddListenerEx(&s->listener,&p,&zero,&h,&up,front,back,speed,-start,SND_EMITTER_CONTINOUS,127,NULL);
    if(s->listener_active) s->listener_distance=distance;
    return s->listener_active;
}
static inline int PartyboardMusyxSoftwareUpdateListener(PartyboardMusyxSoftware *s,const SND_FVECTOR *position,const SND_FVECTOR *heading) {
    SND_FVECTOR delta,up={0,1,0},p,h;
    if(!s || !s->active || !s->listener_active || !PartyboardMusyxSoftwareFiniteVector(position)
            || !PartyboardMusyxSoftwareFiniteVector(heading) || !(heading->x*heading->x+heading->z*heading->z>0)) return 0;
    p=*position;h=*heading;delta.x=p.x-s->listener.pos.x;delta.y=p.y-s->listener.pos.y;delta.z=p.z-s->listener.pos.z;
    return sndUpdateListener(&s->listener,&p,&delta,&h,&up,127,NULL);
}
static inline int PartyboardMusyxSoftwareMoveEffect(PartyboardMusyxSoftware *s,int handle,const SND_FVECTOR *position) {
    PartyboardMusyxSoftwareEffectSlot *e=PartyboardMusyxSoftwareFindEffect(s,handle);
    if(!e || !e->continuous || !PartyboardMusyxSoftwareFiniteVector(position)) return 0;
    e->direction.x=position->x-e->position.x;e->direction.y=position->y-e->position.y;e->direction.z=position->z-e->position.z;
    e->position=*position;PartyboardMusyxSoftwareApplyVolume(e);return 1;
}
static inline int PartyboardMusyxSoftwareVolume(PartyboardMusyxSoftware *s,int handle,unsigned volume) {
    PartyboardMusyxSoftwareEffectSlot *e=PartyboardMusyxSoftwareFindEffect(s,handle);
    if(!e || volume>127) return 0;
    e->volume=volume;PartyboardMusyxSoftwareApplyVolume(e);return 1;
}
static inline int PartyboardMusyxSoftwarePauseSong(PartyboardMusyxSoftware *s,unsigned channel,int pause,int32_t milliseconds) {
    int32_t periods=PartyboardMusyxControlPeriods(milliseconds);
    if(!s || !s->active || channel>=4 || !s->songs[channel].arrangement.data) return 0;
    if(pause && !s->song_paused[channel]) {
        PartyboardMusyxControlFadeStart(&s->song_pause[channel],periods,1);s->song_paused[channel]=1;
        if(!periods) {s->song_pause[channel].volume=0;sndSeqPause(s->songs[channel].sequence);}
    } else if(!pause && s->song_paused[channel]) {
        if(periods && s->song_pause[channel].total) s->song_pause[channel].total=-periods;
        else {sndSeqContinue(s->songs[channel].sequence);PartyboardMusyxControlFadeStart(&s->song_pause[channel],periods,0);}
        s->song_paused[channel]=0;
        if(!periods) {s->song_pause[channel].volume=127;sndSeqVolume((u8)(s->song_volume[channel]*s->song_fade[channel].volume/127),0,s->songs[channel].sequence,0);}
    }
    return 1;
}
static inline void PartyboardMusyxSoftwarePeriodic(PartyboardMusyxSoftware *s) {
    ++s->periodic_calls;
    for(unsigned i=0;i<4;++i) if(s->songs[i].arrangement.data) {
        int changed=s->song_pause[i].total!=0;
        int pause_done=PartyboardMusyxControlFadeStep(&s->song_pause[i]);
        if(!s->song_paused[i] && s->song_fade[i].total) {
            changed=1;
            if(PartyboardMusyxControlFadeStep(&s->song_fade[i])) {PartyboardMusyxSoftwareStopSong(s,i);continue;}
        }
        if(pause_done) sndSeqPause(s->songs[i].sequence);
        else if(changed) sndSeqVolume((u8)((uint64_t)s->song_volume[i]*s->song_pause[i].volume*s->song_fade[i].volume/(127*127)),0,s->songs[i].sequence,0);
    }
    for(unsigned i=0;i<64;++i) {
        PartyboardMusyxSoftwareEffectSlot *e=&s->effects[i];
        if(!PartyboardMusyxSoftwareEffectAlive(e)) continue;
        PartyboardMusyxControlFadeStep(&e->pause);
        e->direction.x=e->direction.y=e->direction.z=0;
        PartyboardMusyxSoftwareApplyVolume(e);
        if(e->status!=3 && PartyboardMusyxControlFadeStep(&e->fade)) PartyboardMusyxSoftwareStopSlot(e);
        else if(e->status!=3 && e->fade.total) PartyboardMusyxSoftwareApplyVolume(e);
    }
}
/* Called once per produced5ms block, even when the device drops output. */
static inline int PartyboardMusyxSoftwareSink(void *context,int32_t *buses,unsigned studios) {
    PartyboardMusyxSoftware *s=context;
    int result=PartyboardMusyxPcmBridgeSubmit(&s->bridge,buses,studios);
    if((s->driver.completed_blocks+1)%3==0) PartyboardMusyxSoftwarePeriodic(s);
    return result;
}
static inline int PartyboardMusyxSoftwareShutdown(PartyboardMusyxSoftware *s) {
    if(!s) return 0;
    if(!s->active) return 1;
    if(dspVoice!=s->voices) return 0;
    for(unsigned i=0;i<4;++i) PartyboardMusyxSoftwareStopSong(s,i);
    PartyboardMusyxSoftwareStopEffects(s,-1);PartyboardMusyxSoftwareRemoveListener(s);
    synthKillAllVoices(0);
    if(!PartyboardMusyxAuxRuntimeShutdown(&s->aux)) return 0;
    while(s->loaded) {
        unsigned i=s->loaded-1;
        if(!PartyboardMusyxGroupPop(&s->groups[i],&s->memory,s->descriptors[i].samples)) return 0;
        --s->loaded;
    }
    PartyboardPcmOutputClose(&s->output);
    s3dExit(); synthExit(); synthVoice=NULL; sndActive=0; dspVoice=NULL; dspSurround=NULL;
    salNumVoices=salMaxStudioNum=0; memset(dspStudio,0,sizeof dspStudio); memset(&synthInfo,0,sizeof synthInfo);
    memset(s,0,sizeof *s); return 1;
}
static inline int PartyboardMusyxSoftwareInit(PartyboardMusyxSoftware *s,const uint8_t *raw,size_t bytes) {
    PartyboardMsmContainer container; PartyboardMsmContainerInfo info; int error=0;
    if(!s) return 0;
    if(s->active || sndActive) { s->last_error=1;return 0; }
    if(!PartyboardMsmContainerOpen(&container,raw,bytes)) { s->last_error=2;return 0; }
    if(!PartyboardMsmContainerGetInfo(&info,&container) || !info.voices || info.voices>64) { s->last_error=3;return 0; }
    memset(s,0,sizeof *s); s->container=container; s->info=info;
    salHooks.malloc=malloc; salHooks.free=free;
    dspVoice=s->voices; dspSurround=s->surround; salNumVoices=(u8)info.voices; salMaxStudioNum=1;
    salFrame=salAuxFrame=0; memset(dspStudio,0,sizeof dspStudio); memset(&synthInfo,0,sizeof synthInfo);
    for(unsigned i=0;i<info.voices;++i) s->voices[i].pb=&s->pbs[i];
    synthInfo.voiceNum=(u8)info.voices; synthInfo.maxMusic=(u8)info.music; synthInfo.maxSFX=(u8)info.effects;
    synthInfo.studioNum=1; synthInfo.numSamples=32;
    synthInit(32000,info.voices); seqInit(); dataInitStack(); streamInit(); vsInit(); s3dInit(0); sndActive=1; s->active=1;
    dspStudio[0].state=dspStudio[0].isMaster=1; dspStudio[0].type=SND_STUDIO_TYPE_STD; dspStudio[0].spb=&s->spb;
    for(unsigned i=0;i<2;++i) dspStudio[0].main[i]=s->main[i];
    for(unsigned i=0;i<3;++i) { dspStudio[0].auxA[i]=s->aux_buffers[0][i]; dspStudio[0].auxB[i]=s->aux_buffers[1][i]; }
    for(unsigned i=0;i<info.base_count;++i) if(!PartyboardMusyxSoftwareLoadIndex(s,info.base_groups[i])) { error=100+(int)info.base_groups[i];goto failed; }
    s->base_loaded=s->loaded;
    error=4;if(!PartyboardMusyxVoiceSchedulerInitDefault(&s->scheduler,info.voices,1,&s->memory)) goto failed;
    error=5;if(!PartyboardMusyxBlockDriverInit(&s->driver,&s->scheduler,60)) goto failed;
    error=6;if(!PartyboardMusyxStudioProcessorInit(&s->processor,1)) goto failed;
    error=7;if(!PartyboardMusyxPcmBridgeInit(&s->bridge,&s->driver,&s->processor,NULL)) goto failed;
    error=8;if(!PartyboardMusyxAuxRuntimeInit(&s->aux,0)) goto failed;
    error=9;if(!PartyboardMusyxAuxRuntimeSelect(&s->aux,&s->container,info.aux_a,info.aux_b)) goto failed;
    sndVolume(127,0,255); sndOutputMode(SND_OUTPUTMODE_STEREO); return 1;
failed:
    PartyboardMusyxSoftwareShutdown(s); s->last_error=error;return 0;
}
/* Configuration, not device availability, selects original surround controls.
 * A failed/full/missing playback stream must not change source SDK behavior. */
static inline int PartyboardMusyxSoftwareSetChannels(PartyboardMusyxSoftware *s,unsigned channels) {
    if(!s || !s->active || (channels!=2 && channels!=6)) return 0;
    sndOutputMode(channels==6?SND_OUTPUTMODE_SURROUND:SND_OUTPUTMODE_STEREO);
    return 1;
}
static inline int PartyboardMusyxSoftwareTick(PartyboardMusyxSoftware *s) {
    if(!s || !s->active || !PartyboardMusyxPcmBridgeAligned(&s->bridge)
            || !PartyboardMusyxBlockDriverTickProcessed(&s->driver,s->scratch,PartyboardMusyxStudioProcess,&s->processor,PartyboardMusyxSoftwareSink,s)) return 0;
    for(unsigned i=0;i<320;++i) if(s->processor.stereo[i]) ++s->nonzero_samples;
    for(unsigned i=0;i<4;++i) if(s->songs[i].arrangement.data && !sndSeqGetValid(s->songs[i].sequence))
        PartyboardMusyxSoftwareStopSong(s,i);
    return 1;
}
static inline int PartyboardMusyxSoftwareSong(PartyboardMusyxSoftware *s,unsigned index,unsigned channel) {
    PartyboardMsmContainerSong descriptor;
    if(!s || !s->active || channel>=4 || !PartyboardMsmContainerGetSong(&descriptor,&s->container,index)
            || descriptor.group==65535 || !PartyboardMusyxSoftwareLoadId(s,descriptor.group)
            || (descriptor.song_group>=0 && !PartyboardMusyxSoftwareLoadIndex(s,(unsigned)descriptor.song_group))) return -1;
    PartyboardMusyxSoftwareStopSong(s,channel);
    if(!PartyboardMusyxSongStartContainer(&s->songs[channel],&s->container,index,s->indices,s->loaded,1,0)) return -1;
    s->song_pause[channel]=(PartyboardMusyxControlFade){0,0,127};s->song_paused[channel]=0;s->song_volume[channel]=(unsigned)descriptor.volume;
    s->song_fade[channel]=(PartyboardMusyxControlFade){0,0,127};
    ++s->song_starts; return (int)channel;
}
static inline int PartyboardMusyxSoftwareEffect(PartyboardMusyxSoftware *s,unsigned index,
        unsigned volume,unsigned pan,int pitch,int aux_a,int aux_b,int span) {
    PartyboardMsmContainerEffect descriptor; uint32_t voice; unsigned slot;
    if(!s || !s->active || !PartyboardMsmContainerGetEffect(&descriptor,&s->container,index)
            || !PartyboardMusyxSoftwareLoadId(s,descriptor.group)) return -1;
    for(slot=0;slot<64;++slot) if(!PartyboardMusyxSoftwareEffectAlive(&s->effects[slot])) break;
    if(slot==64) return -1;
    if(span>=0) descriptor.span=(unsigned)span;
    voice=PartyboardMusyxEffectStartNonPositional(&descriptor,volume,pan,pitch,
        aux_a<0?descriptor.reverb:(unsigned)aux_a,aux_b<0?descriptor.chorus:(unsigned)aux_b,0);
    if(voice==UINT32_MAX) return -1;
    memset(&s->effects[slot],0,sizeof s->effects[slot]);
    if(s->next_handle==INT32_MAX) s->next_handle=0;
    s->effects[slot].handle=++s->next_handle; s->effects[slot].voice=voice;
    s->effects[slot].effect=(int)index; s->effects[slot].volume=volume;
    s->effects[slot].base_volume=(unsigned)descriptor.volume;s->effects[slot].status=2;
    s->effects[slot].fade.volume=s->effects[slot].pause.volume=127;
    ++s->effect_starts; return s->effects[slot].handle;
}
static inline uint32_t PartyboardMusyxSoftwareVoice(PartyboardMusyxSoftware *s,int handle) {
    PartyboardMusyxSoftwareEffectSlot *e=PartyboardMusyxSoftwareFindEffect(s,handle);
    return e ? e->voice : UINT32_MAX;
}
static inline int PartyboardMusyxSoftwarePositional(PartyboardMusyxSoftware *s,unsigned index,const SND_FVECTOR *position,
        unsigned volume,int aux_a,int aux_b) {
    PartyboardMsmContainerEffect descriptor; PartyboardMusyxSoftwareEffectSlot *e; SND_FVECTOR p; unsigned slot,a,b;
    if(!s || !s->active || !s->listener_active || !PartyboardMusyxSoftwareFiniteVector(position) || volume>127
            || !PartyboardMsmContainerGetEffect(&descriptor,&s->container,index) || descriptor.effect==65535
            || !PartyboardMusyxSoftwareLoadId(s,descriptor.group)) return -1;
    a=aux_a<0?descriptor.reverb:(unsigned)aux_a;b=aux_b<0?descriptor.chorus:(unsigned)aux_b;if(a>127 || b>127) return -1;
    for(slot=0;slot<64;++slot) if(!PartyboardMusyxSoftwareEffectAlive(&s->effects[slot])) break;
    if(slot==64) return -1;
    e=&s->effects[slot];memset(e,0,sizeof *e);p=*position;e->voice=UINT32_MAX;
    if(descriptor.emitter==1) {
        if(!PartyboardMusyxEmitterStart(&e->emitter,&descriptor,&p,s->listener_distance,s->emitter_group++,volume,a,b)) return -1;
        e->continuous=1;
    } else {
        e->voice=PartyboardMusyxEmitterStartOneShot(&descriptor,&p,s->listener_distance,s->emitter_group++,volume,a,b);
        if(e->voice==UINT32_MAX) return -1;
    }
    if(s->next_handle==INT32_MAX) s->next_handle=0;
    e->handle=++s->next_handle;e->effect=(int)index;e->status=2;e->positional=1;e->position=p;
    e->volume=volume;e->base_volume=(unsigned)descriptor.volume;e->fade.volume=e->pause.volume=127;
    ++s->effect_starts;++s->positional_starts;return e->handle;
}
/* Scene change releases dynamic groups in reverse order after stopping users.
 * The original base prefix remains available. No live arrangement is freed
 * while an SDK sequence can still reference it. */
static inline int PartyboardMusyxSoftwareScene(PartyboardMusyxSoftware *s,unsigned index) {
    if(!s || !s->active) return 0;
    for(unsigned i=0;i<4;++i) PartyboardMusyxSoftwareStopSong(s,i);
    PartyboardMusyxSoftwareStopEffects(s,-1);
    synthKillAllVoices(0);
    while(s->loaded>s->base_loaded) {
        unsigned i=s->loaded-1;
        if(!PartyboardMusyxGroupPop(&s->groups[i],&s->memory,s->descriptors[i].samples)) return 0;
        --s->loaded;
    }
    memset(s->effects,0,sizeof s->effects);
    return index==0 || PartyboardMusyxSoftwareLoadIndex(s,index);
}
#endif
