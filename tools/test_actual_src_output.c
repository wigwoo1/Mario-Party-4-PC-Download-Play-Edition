/* Private authored-effect -> original controls -> replacement SRC -> SDL.
 * Only counters/hashes leave this process; no game payload is written out.
 * Dry standard studio: authored aux processors are a separate integration. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#include "../src/port/msm_container.h"
#include "../src/port/musyx_group_runtime.h"
#include "../src/port/musyx_effect_parameters.h"
#include "../src/port/musyx_pcm_output.h"

static DSPvoice voices[64];
static _PB pbs[64];
static _SPB spb;
static int32_t main_buffers[2][480], aux_buffers[2][3][480], surround[160], scratch[160*9];
static PartyboardMusyxSampleMemory memory;
static PartyboardMusyxVoiceScheduler scheduler;
static PartyboardMusyxBlockDriver driver;
static PartyboardMusyxStudioProcessor processor;
static PartyboardMusyxPcmBridge bridge;
static PartyboardPcmOutput output;
static PartyboardMusyxGroup groups[24];
static PartyboardMsmContainerGroup descriptors[24];

int main(void) {
    uint8_t header[8], *raw; size_t bytes; unsigned effect_index, loaded=0, indices[24];
    PartyboardMsmContainer container; PartyboardMsmContainerInfo info; PartyboardMsmContainerEffect effect;
    uint32_t voice_id, hash=2166136261u, nonzero=0, frames=0, peak=0, selected[3]={0};
    SDL_AudioSpec spec={SDL_AUDIO_S16,2,32000};
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);
#endif
    assert(fread(header,1,8,stdin)==8); bytes=PartyboardMsmU32(header); effect_index=PartyboardMsmU32(header+4);
    assert(bytes>=96 && bytes<=67108864); raw=malloc(bytes); assert(raw && fread(raw,1,bytes,stdin)==bytes);
    assert(PartyboardMsmContainerOpen(&container,raw,bytes));
    assert(PartyboardMsmContainerGetInfo(&info,&container));
    assert(PartyboardMsmContainerGetEffect(&effect,&container,effect_index) && effect.effect!=65535u);
    for(unsigned i=0;i<info.base_count;++i) indices[loaded++]=info.base_groups[i];
    for(unsigned i=0;i<info.groups;++i) {
        PartyboardMsmContainerGroup group; unsigned j;
        assert(PartyboardMsmContainerGetGroup(&group,&container,i));
        if(group.id!=effect.group || !group.data_bytes || !group.sample_bytes) continue;
        for(j=0;j<loaded && indices[j]!=i;++j) {}
        if(j==loaded) { assert(loaded<24); indices[loaded++]=i; }
        break;
    }
    dspVoice=voices; dspSurround=surround; salNumVoices=64; salMaxStudioNum=1; salFrame=salAuxFrame=0;
    salHooks.malloc=malloc; salHooks.free=free;
    for(unsigned i=0;i<64;++i) voices[i].pb=&pbs[i];
    synthInfo.voiceNum=synthInfo.maxMusic=synthInfo.maxSFX=64; synthInfo.studioNum=1; synthInfo.numSamples=32;
    synthInit(32000,64); seqInit(); dataInitStack(); streamInit(); vsInit(); s3dInit(0); sndActive=1;
    dspStudio[0].state=dspStudio[0].isMaster=1; dspStudio[0].type=SND_STUDIO_TYPE_STD; dspStudio[0].spb=&spb;
    for(unsigned i=0;i<2;++i) dspStudio[0].main[i]=main_buffers[i];
    for(unsigned i=0;i<3;++i) { dspStudio[0].auxA[i]=aux_buffers[0][i]; dspStudio[0].auxB[i]=aux_buffers[1][i]; }
    for(unsigned i=0;i<loaded;++i) {
        assert(PartyboardMsmContainerGetGroup(&descriptors[i],&container,indices[i]));
        assert(PartyboardMusyxGroupPush(&groups[i],&memory,descriptors[i].data,descriptors[i].data_bytes,
            (uint8_t*)descriptors[i].samples,descriptors[i].sample_bytes,(uint16_t)descriptors[i].id));
    }
    assert(PartyboardMusyxVoiceSchedulerInitDefault(&scheduler,64,1,&memory));
    assert(PartyboardMusyxBlockDriverInit(&driver,&scheduler,60));
    assert(PartyboardMusyxStudioProcessorInit(&processor,1));
    output.stream=SDL_CreateAudioStream(&spec,&spec); assert(output.stream); output.frequency=32000; output.channels=2;
    assert(PartyboardMusyxPcmBridgeInit(&bridge,&driver,&processor,&output));
    /* Original msmInit call (msmsys.c): synthInit deliberately starts master
     * faders at zero. Use the original API, not a test-only gain override. */
    sndVolume(127,0,255);
    voice_id=PartyboardMusyxEffectStartNonPositional(&effect,127,64,0,effect.reverb,effect.chorus,0);
    assert(voice_id!=UINT32_MAX);
    for(unsigned tick=0;tick<60;++tick) {
        int16_t pcm[640*2]; unsigned before=(unsigned)driver.completed_blocks, count;
        if(!PartyboardMusyxPcmBridgeTick(&bridge,scratch)) {
            fprintf(stderr,"render failed tick=%u voice=%u\n",tick,scheduler.failed_voice); return 10;
        }
        for(unsigned i=0;i<64;++i) if(voices[i].state) { assert(voices[i].srcCoefSelect<3); ++selected[voices[i].srcCoefSelect]; }
        count=((unsigned)driver.completed_blocks-before)*160;
        assert(PartyboardPcmOutputFinish(&output));
        assert(SDL_GetAudioStreamData(output.stream,pcm,(int)(count*2*sizeof(int16_t)))==(int)(count*2*sizeof(int16_t)));
        assert(!SDL_GetAudioStreamQueued(output.stream));
        for(unsigned i=0;i<count*2;++i) {
            unsigned magnitude=pcm[i]<0 ? (unsigned)-(int)pcm[i] : (unsigned)pcm[i];
            if(magnitude) ++nonzero; if(magnitude>peak) peak=magnitude;
            hash=(hash^((uint16_t)pcm[i]&255u))*16777619u; hash=(hash^((uint16_t)pcm[i]>>8))*16777619u;
        }
        frames+=count;
    }
    assert(frames==32000 && nonzero>100 && peak>10);
    assert(driver.completed_blocks==200 && driver.accepted_blocks==200 && !driver.dropped_blocks && !driver.device_errors);
    assert(synthRealTime==256000);
    printf("{\"effect\":%u,\"frames\":%u,\"nonzeroSamples\":%u,\"peak\":%u,\"pcmHash\":%u,\"bankVoiceTicks\":[%u,%u,%u],\"blocks\":200,\"controlPeriods\":1000}\n",
        effect_index,frames,nonzero,peak,hash,selected[0],selected[1],selected[2]);
    synthKillAllVoices(0);
    for(unsigned i=loaded;i;--i) assert(PartyboardMusyxGroupPop(&groups[i-1],&memory,descriptors[i-1].samples));
    PartyboardPcmOutputClose(&output); s3dExit(); synthExit(); sndActive=0; free(raw);
    return 0;
}
