#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#include "../src/port/musyx_software.h"
static PartyboardMusyxSoftware owner;
static uint8_t *raw;
static size_t raw_bytes;
static unsigned output_mode;
static uint64_t energy[2];
static uint32_t pcm_hash, state_hash;
static unsigned nonzero;
static int16_t prefill[8000*2];
static uint32_t hash32(uint32_t h,uint32_t value) {return (h^value)*16777619u;}
static void start(void) {
    SDL_AudioSpec spec={SDL_AUDIO_S16,2,32000};
    assert(PartyboardMusyxSoftwareInit(&owner,raw,raw_bytes));
    if(output_mode!=2) {
        owner.output.stream=SDL_CreateAudioStream(&spec,&spec);assert(owner.output.stream);
        owner.output.frequency=32000;owner.output.channels=2;
        assert(PartyboardMusyxPcmBridgeSetOutput(&owner.bridge,&owner.output));
        if(output_mode==1) assert(SDL_PutAudioStreamData(owner.output.stream,prefill,sizeof prefill));
    }
    energy[0]=energy[1]=0;pcm_hash=state_hash=2166136261u;nonzero=0;
}
static void ticks(unsigned count) {
    for(unsigned tick=0;tick<count;++tick) {
        unsigned before=(unsigned)owner.driver.completed_blocks;
        if(!PartyboardMusyxSoftwareTick(&owner)) {
            fprintf(stderr,"controls render failed block=%llu voice=%u\n",(unsigned long long)owner.driver.completed_blocks,owner.scheduler.failed_voice);exit(10);
        }
        assert(owner.periodic_calls==owner.driver.completed_blocks/3);
        state_hash=hash32(state_hash,(uint32_t)owner.driver.completed_blocks);
        state_hash=hash32(state_hash,(uint32_t)owner.periodic_calls);
        state_hash=hash32(state_hash,(uint32_t)synthRealTime);state_hash=hash32(state_hash,(uint32_t)owner.listener_active);
        for(unsigned i=0;i<4;++i) {
            state_hash=hash32(state_hash,owner.song_pause[i].volume);state_hash=hash32(state_hash,owner.song_fade[i].volume);
            state_hash=hash32(state_hash,(uint32_t)owner.song_paused[i]);
        }
        for(unsigned i=0;i<480;++i) state_hash=hash32(state_hash,(uint32_t)owner.processor.master_lrs[i]);
        for(unsigned i=0;i<64;++i) {
            PartyboardMusyxSoftwareEffectSlot *e=&owner.effects[i];
            state_hash=hash32(state_hash,(uint32_t)e->status);state_hash=hash32(state_hash,e->fade.volume);
            state_hash=hash32(state_hash,e->pause.volume);state_hash=hash32(state_hash,e->voice);
            state_hash=hash32(state_hash,(uint32_t)e->fade.time);state_hash=hash32(state_hash,(uint32_t)e->fade.total);
            state_hash=hash32(state_hash,(uint32_t)e->pause.time);state_hash=hash32(state_hash,(uint32_t)e->pause.total);
        }
        if(output_mode==0) {
            int16_t pcm[1280];unsigned frames=((unsigned)owner.driver.completed_blocks-before)*160;
            assert(PartyboardPcmOutputFinish(&owner.output));
            assert(SDL_GetAudioStreamData(owner.output.stream,pcm,(int)(frames*4))==(int)(frames*4));
            for(unsigned i=0;i<frames*2;++i) {
                unsigned magnitude=(unsigned)(pcm[i]<0?-(int)pcm[i]:pcm[i]);
                energy[i%2]+=magnitude;nonzero+=magnitude!=0;
                pcm_hash=hash32(pcm_hash,(uint16_t)pcm[i]);
            }
        }
    }
}
static void finish(void) {
    if(output_mode==0) assert(owner.driver.accepted_blocks==owner.driver.completed_blocks && !owner.driver.dropped_blocks);
    else assert(!owner.driver.accepted_blocks && owner.driver.dropped_blocks==owner.driver.completed_blocks);
    if(output_mode==1) assert(SDL_GetAudioStreamQueued(owner.output.stream)==sizeof prefill);
    assert(!owner.driver.device_errors);
    assert(PartyboardMusyxSoftwareShutdown(&owner));assert(!owner.active && !sndActive && !dspVoice);
}
static void fade_reference(void) {
    static const int32_t durations[]={0,14,15,29,30,100,1500,INT32_MAX};
    for(unsigned i=0;i<sizeof durations/sizeof durations[0];++i) {
        int32_t periods=durations[i]/15;
        assert(PartyboardMusyxControlPeriods(durations[i])==periods);
        if(!periods) continue;
        PartyboardMusyxControlFade f;
        PartyboardMusyxControlFadeStart(&f,periods,1);
        for(int32_t n=1;n<=periods && n<=12;++n) {
            int done=PartyboardMusyxControlFadeStep(&f);
            unsigned expected=(unsigned)((int64_t)(periods-n)*127/periods);
            assert(f.volume==expected && done==(n==periods));
        }
        f.total=periods;f.time=1;assert(PartyboardMusyxControlFadeStep(&f) && !f.total && !f.volume);
        PartyboardMusyxControlFadeStart(&f,periods,0);
        for(int32_t n=1;n<=periods && n<=12;++n) {
            assert(!PartyboardMusyxControlFadeStep(&f));
            assert(f.volume==(unsigned)((int64_t)n*127/periods));
        }
    }
    assert(PartyboardMusyxControlPeriods(-1)==0);
    assert(PartyboardMusyxControlVolume(100,64,63,84)==(unsigned)((uint64_t)100*64*63*84/(127*127*127)));
}
static uint32_t spatial(int side,uint64_t *left,uint64_t *right,uint32_t *pcm) {
    SND_FVECTOR origin={0},heading={0,0,-1},pos={(float)(side*30),0,-20},bad={NAN,0,0};
    start();assert(PartyboardMusyxAuxRuntimeSelect(&owner.aux,&owner.container,-2,-2));
    assert(PartyboardMusyxSoftwarePositional(&owner,0,&pos,127,-1,-1)<0); /* No listener. */
    assert(!PartyboardMusyxSoftwareListener(&owner,&bad,&heading,100,343,0,25,25));
    assert(PartyboardMusyxSoftwareListener(&owner,&origin,&heading,100,343,0,25,25));
    assert(PartyboardMusyxSoftwareListener(&owner,&origin,&heading,100,343,0,25,25)); /* Replace, don't double-link. */
    int handle=PartyboardMusyxSoftwarePositional(&owner,0,&pos,127,0,0);assert(handle>0);
    ticks(60);if(output_mode==0) assert(nonzero>100);
    *left=energy[0];*right=energy[1];*pcm=pcm_hash;uint32_t result=state_hash;
    PartyboardMusyxSoftwareRemoveListener(&owner);PartyboardMusyxSoftwareRemoveListener(&owner);
    assert(!owner.listener_active && !PartyboardMusyxSoftwareUpdateListener(&owner,&origin,&heading));
    finish();return result;
}
int main(int argc,char **argv) {
    uint8_t header[8];uint64_t ll,lr,rl,rr;uint32_t left_state,right_state,controls_state,left_pcm,right_pcm;int handle,other;
    SND_FVECTOR origin={0},heading={0,0,-1},pos={10,2,-5},moved={14,3,-7},new_listener={1,0,0};
    output_mode=argc==2?(unsigned)atoi(argv[1]):0;assert(output_mode<=2);
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);
#endif
    assert(fread(header,1,8,stdin)==8);raw_bytes=PartyboardMsmU32(header);assert(raw_bytes<=67108864);
    raw=malloc(raw_bytes);assert(raw && fread(raw,1,raw_bytes,stdin)==raw_bytes);fade_reference();
    left_state=spatial(-1,&ll,&lr,&left_pcm);right_state=spatial(1,&rl,&rr,&right_pcm);
    if(output_mode==0) assert((ll>lr && rr>rl) || (lr>ll && rl>rr));
    start();assert(PartyboardMusyxSoftwareListener(&owner,&origin,&heading,100,343,0,25,25));
    handle=PartyboardMusyxSoftwarePositional(&owner,54,&pos,127,23,45);assert(handle>0);
    PartyboardMusyxSoftwareEffectSlot *e=PartyboardMusyxSoftwareFindEffect(&owner,handle);assert(e && e->continuous);
    assert(e->emitter.emitter.paraInfo==&e->emitter.info && e->emitter.info.paraArray==e->emitter.parameters);
    assert(e->emitter.parameters[0].paraData.value7==23 && e->emitter.parameters[1].paraData.value7==45);
    assert(PartyboardMusyxSoftwareVolume(&owner,handle,64));
    assert(fabsf(e->emitter.emitter.maxVol-50.0f/127.0f)<0.000001f);
    assert(PartyboardMusyxSoftwarePauseEffect(&owner,handle,1,0));assert(e->emitter.emitter.maxVol==0);
    assert(PartyboardMusyxSoftwarePauseEffect(&owner,handle,0,0));
    assert(PartyboardMusyxSoftwareMoveEffect(&owner,handle,&moved));
    assert(e->emitter.emitter.dir.x==4 && e->emitter.emitter.dir.y==1 && e->emitter.emitter.dir.z==-2);
    assert(PartyboardMusyxSoftwareUpdateListener(&owner,&new_listener,&heading));assert(owner.listener.dir.x==1);
    ticks(1);assert(PartyboardMusyxSoftwareFindEffect(&owner,handle));
    assert(PartyboardMusyxSoftwareStopEffect(&owner,handle,30));ticks(2);
    assert(!sndCheckEmitter(&e->emitter.emitter) && e->status==0);
    assert(!PartyboardMusyxSoftwareFindEffect(&owner,handle));
    handle=PartyboardMusyxSoftwareEffect(&owner,2200,127,64,0,0,0,-1);assert(handle>0);
    e=PartyboardMusyxSoftwareFindEffect(&owner,handle);assert(e);
    assert(PartyboardMusyxSoftwarePauseEffect(&owner,handle,1,100));
    ticks(6);assert(e->pause.volume==0 && e->status==3 && !e->pause.total);
    assert(PartyboardMusyxSoftwareStopEffect(&owner,handle,45));ticks(4);assert(e->fade.time==3); /* Paused fade freezes. */
    assert(PartyboardMusyxSoftwareVolume(&owner,handle,64));assert(e->pause.volume==0);
    assert(PartyboardMusyxSoftwarePauseEffect(&owner,handle,0,45));ticks(3);
    assert(!e->fade.total && e->status<=1);
    assert(!PartyboardMusyxSoftwareVolume(&owner,-1,64));assert(!PartyboardMusyxSoftwareStopEffect(&owner,-1,0));
    assert(PartyboardMusyxSoftwareScene(&owner,18));assert(!PartyboardMusyxSoftwareFindEffect(&owner,handle));
    handle=PartyboardMusyxSoftwareEffect(&owner,0,127,64,0,0,0,-1);assert(handle>0);
    other=PartyboardMusyxSoftwareEffect(&owner,2200,127,64,0,0,0,-1);assert(other>0);
    PartyboardMusyxSoftwareStopEffects(&owner,0);
    assert(owner.effects[0].effect==0 && owner.effects[0].status==1);
    assert(PartyboardMusyxSoftwareFindEffect(&owner,other)->status==2);
    assert(PartyboardMusyxSoftwareSong(&owner,0,0)==0);ticks(3);
    assert(PartyboardMusyxSoftwarePauseSong(&owner,0,1,100));ticks(6);
    assert(owner.song_paused[0] && !owner.song_pause[0].total && !owner.song_pause[0].volume);
    assert(seqInstance[seqGetPrivateId(owner.songs[0].sequence)].state==2);
    assert(PartyboardMusyxSoftwarePauseSong(&owner,0,0,45));ticks(3);
    assert(!owner.song_paused[0] && !owner.song_pause[0].total && owner.song_pause[0].volume==127);
    assert(seqInstance[seqGetPrivateId(owner.songs[0].sequence)].state==1);
    assert(PartyboardMusyxSoftwarePauseSong(&owner,0,1,0));assert(owner.song_pause[0].volume==0);
    assert(PartyboardMusyxSoftwarePauseSong(&owner,0,0,0));assert(owner.song_pause[0].volume==127);
    assert(!PartyboardMusyxSoftwarePauseSong(&owner,4,1,100));
    assert(PartyboardMusyxSoftwareFadeSong(&owner,0,90));
    assert(PartyboardMusyxSoftwarePauseSong(&owner,0,1,30));ticks(3);
    assert(owner.song_fade[0].time==6 && owner.songs[0].arrangement.data);
    assert(PartyboardMusyxSoftwarePauseSong(&owner,0,0,30));ticks(6);
    assert(!owner.songs[0].arrangement.data && !owner.song_pause[0].total && !owner.song_fade[0].total);
    handle=PartyboardMusyxSoftwarePositional(&owner,54,&pos,127,0,0);assert(handle>0);
    e=PartyboardMusyxSoftwareFindEffect(&owner,handle);assert(e && sndCheckEmitter(&e->emitter.emitter));
    assert(PartyboardMusyxSoftwareScene(&owner,18));assert(!sndCheckEmitter(&e->emitter.emitter));ticks(3);
    controls_state=state_hash;uint64_t calls=owner.periodic_calls,blocks=owner.driver.completed_blocks;
    if(output_mode==0) assert(nonzero>100);
    finish();free(raw);
    printf("{\"mode\":%u,\"leftState\":%u,\"rightState\":%u,\"controlsState\":%u,\"leftPcm\":%u,\"rightPcm\":%u,\"controlsPcm\":%u,\"leftEnergy\":[%llu,%llu],\"rightEnergy\":[%llu,%llu],\"blocks\":%llu,\"periodicCalls\":%llu}\n",
        output_mode,left_state,right_state,controls_state,left_pcm,right_pcm,pcm_hash,(unsigned long long)ll,(unsigned long long)lr,
        (unsigned long long)rl,(unsigned long long)rr,(unsigned long long)blocks,(unsigned long long)calls);
    return 0;
}
