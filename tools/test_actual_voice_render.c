#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#include "musyx/synth.h"
#include "musyx/seq.h"
#include "../src/port/msm_container.h"
#include "../src/port/musyx_group_runtime.h"
#include "../src/port/musyx_voice_source.h"
#include "../src/port/musyx_effect_parameters.h"
#include "../src/port/musyx_pcm_runtime.h"
#ifdef PARTYBOARD_REPLACEMENT_SRC
#include "../src/port/src_coefficients.h"
#endif
void hwSetTimeOffset(u8 offset);
#ifdef PARTYBOARD_DIAGNOSTIC_BLOCK
#define DIAGNOSTIC_FRAMES 160
#else
#define DIAGNOSTIC_FRAMES 32
#endif
#ifdef PARTYBOARD_DIAGNOSTIC_BUSES
#define DIAGNOSTIC_CHANNELS 9
#else
#define DIAGNOSTIC_CHANNELS 1
#endif
int main(void) {
    uint8_t header[8],*raw;
    size_t bytes;
    unsigned effect_index,i,loaded=0,indices[24],selected=0;
    unsigned seen[64]={0};
    unsigned source_modes[3]={0};
    unsigned coefficient_banks[3]={0};
    int16_t first_pcm[64][DIAGNOSTIC_FRAMES*DIAGNOSTIC_CHANNELS];
    size_t first_count[64]={0};
    uint32_t first_id[64]={0},first_offset[64]={0};
    PartyboardMsmContainer container;
    PartyboardMsmContainerInfo info;
    PartyboardMsmContainerEffect effect;
    PartyboardMsmContainerGroup descriptors[24];
    PartyboardMusyxGroup groups[24]={0};
    PartyboardMusyxSampleMemory memory={0};
    DSPvoice voices[64]={0};
#ifdef PARTYBOARD_REPLACEMENT_SRC
    const int16_t *test_banks=PartyboardSrcDefaultCoefficients(NULL);
    fprintf(stderr,"SRC_PROVIDER %s\n",PARTYBOARD_SRC_COEFFICIENT_PROVIDER);
#else
    int16_t test_banks[1536]={0};
    /* Diagnostic newest-tap bank; never production ROM coefficients. */
    for(unsigned phase=0;phase<384;++phase) test_banks[phase*4+3]=32767;
#endif
    uint32_t effect_voice;
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);
#endif
    if(fread(header,1,8,stdin)!=8) return 2;
    bytes=PartyboardMsmU32(header);effect_index=PartyboardMsmU32(header+4);
    if(bytes<96 || bytes>67108864) return 2;
    raw=malloc(bytes);if(!raw) return 3;
    if(fread(raw,1,bytes,stdin)!=bytes) return 4;
    if(!PartyboardMsmContainerOpen(&container,raw,bytes)
            || !PartyboardMsmContainerGetInfo(&info,&container)
            || !PartyboardMsmContainerGetEffect(&effect,&container,effect_index)) return 5;
    for(i=0;i<info.base_count;++i) indices[loaded++]=info.base_groups[i];
    for(i=0;i<info.groups;++i) {
        PartyboardMsmContainerGroup group;
        unsigned j;
        if(!PartyboardMsmContainerGetGroup(&group,&container,i)) return 5;
        /* Entry zero is an empty placeholder sharing ID zero with the real
         * base group. It cannot supply a synthesis project or sample bank. */
        if(group.id!=effect.group || !group.data_bytes || !group.sample_bytes) continue;
        for(j=0;j<loaded && indices[j]!=i;++j) {}
        if(j==loaded) indices[loaded++]=i;
        break;
    }
    if(i==info.groups) return 6;
    salHooks.malloc=malloc;salHooks.free=free;dspVoice=voices;
    synthInfo.voiceNum=64;synthInfo.maxMusic=64;synthInfo.maxSFX=64;synthInfo.numSamples=32;
    synthInit(32000,64);seqInit();sndActive=1;dataInitStack();
    for(i=0;i<loaded;++i) {
        if(!PartyboardMsmContainerGetGroup(&descriptors[i],&container,indices[i])
                || !PartyboardMusyxGroupPush(&groups[i],&memory,descriptors[i].data,descriptors[i].data_bytes,
                    (uint8_t*)descriptors[i].samples,descriptors[i].sample_bytes,descriptors[i].id)) return 7;
    }
    effect_voice=PartyboardMusyxEffectStartNonPositional(&effect,127,64,0,effect.reverb,effect.chorus,0);
    /* Serialized tombstones retain group zero but use invalid FXID65535.
     * Execute the original API and verify its failure instead of claiming play. */
    if(effect.effect==0xffffu) {
        if(effect_voice!=0xffffffffu) return 8;
    } else if(effect_voice==0xffffffffu) return 8;
    for(i=0;i<(effect.effect==0xffffu?0u:2000u);++i) {
        unsigned voice;
        hwSetTimeOffset((u8)(i%5));
        seqHandle(256);synthHandle(256);
        for(voice=0;voice<64;++voice) if(!seen[voice] && voices[voice].smp_info.addr) {
            PartyboardMusyxPcmRuntime runtime={0};
            PartyboardMusyxPcmRamp ramp;
#ifndef PARTYBOARD_DIAGNOSTIC_BLOCK
            _PBMIX mix={0};
#endif
            int64_t bus[DIAGNOSTIC_FRAMES*9]={0};
            int16_t interleaved[DIAGNOSTIC_FRAMES*9];
            unsigned frame,offset=voices[voice].lastUpdate.pitch;
            if(offset>=5) return 16;
#ifdef PARTYBOARD_DIAGNOSTIC_BLOCK
            {
                DSPvoice copied=voices[voice];
                _PB pb={0};
#ifndef PARTYBOARD_DIAGNOSTIC_PLANNED
                uint16_t resets[5]={0};
#endif
                int64_t block_bus[160*9]={0};
                size_t rendered;
                copied.pb=&pb;
                if(!PartyboardMusyxPcmRuntimeInitStandardMixer200(&copied,SND_STUDIO_TYPE_STD,&ramp)
                        || !PartyboardMusyxPcmRuntimeStart(&runtime,&copied,&memory,
                            copied.pitch[offset],test_banks,1536)) return 17;
#ifdef PARTYBOARD_DIAGNOSTIC_PLANNED
                rendered=PartyboardMusyxPcmRuntimeRenderPlannedStandard200(&runtime,&copied,
                        SND_STUDIO_TYPE_STD,offset,1,block_bus);
#else
                rendered=PartyboardMusyxPcmRuntimeRenderBlock(&runtime,&copied,offset,&ramp,resets,block_bus);
#endif
                first_count[voice]=rendered;
                memcpy(bus,block_bus+offset*32*9,first_count[voice]*9*sizeof(*bus));
            }
#else
            if(!PartyboardMusyxPcmRuntimeStart(&runtime,&voices[voice],&memory,
                    voices[voice].pitch[offset],test_banks,1536)) return 17;
            mix.vL=voices[voice].volL;mix.vR=voices[voice].volR;mix.vS=voices[voice].volS;
            mix.vAuxAL=voices[voice].volLa;mix.vAuxAR=voices[voice].volRa;mix.vAuxAS=voices[voice].volSa;
            mix.vAuxBL=voices[voice].volLb;mix.vAuxBR=voices[voice].volRb;mix.vAuxBS=voices[voice].volSb;
            PartyboardMusyxPcmRampLoad(&ramp,&mix);
            first_count[voice]=PartyboardMusyxPcmRuntimeRenderRampPeriod(&runtime,
                    &voices[voice],offset,&ramp,0,bus);
#endif
            if(!first_count[voice]) return 18;
            PartyboardPcmMixFinish(bus,interleaved,first_count[voice]*9);
#ifdef PARTYBOARD_DIAGNOSTIC_BUSES
            first_count[voice]*=9;
            for(frame=0;frame<first_count[voice];++frame) first_pcm[voice][frame]=interleaved[frame];
#else
            for(frame=0;frame<first_count[voice];++frame) first_pcm[voice][frame]=interleaved[frame*9];
#endif
            PartyboardMusyxPcmRuntimeStop(&runtime);
            first_id[voice]=voices[voice].smp_id;first_offset[voice]=voices[voice].smp_info.offset;
            if(voices[voice].srcTypeSelect>2) return 19;
            ++source_modes[voices[voice].srcTypeSelect];
            if(voices[voice].srcCoefSelect>=3) return 20;
            ++coefficient_banks[voices[voice].srcCoefSelect];
            seen[voice]=1;++selected;
        }
    }
    for(i=0;i<64;++i) if(voices[i].smp_info.addr) {
        PartyboardMsmSample source;int16_t pcm[32];
        if(!PartyboardMusyxVoiceSourceOpen(&source,&voices[i],&memory)
                || !PartyboardMsmSampleRead(&source,pcm,32)) return 9;
    }
    printf("%u %u %u\n",effect_index,loaded,selected);
    fprintf(stderr,"SRC_MODES %u %u %u\n",source_modes[0],source_modes[1],source_modes[2]);
    fprintf(stderr,"SRC_BANKS %u %u %u\n",coefficient_banks[0],coefficient_banks[1],coefficient_banks[2]);
    for(i=0;i<64;++i) if(seen[i]) {
        size_t sample;
        printf("%u %u %u %zu\n",i,first_id[i],first_offset[i],first_count[i]);
        for(sample=0;sample<first_count[i];++sample) printf("%d\n",first_pcm[i][sample]);
    }
    if(effect_voice!=0xffffffffu) sndFXKeyOff(effect_voice);
    synthKillAllVoices(0);
    for(i=loaded;i;--i) if(!PartyboardMusyxGroupPop(&groups[i-1],&memory,descriptors[i-1].samples)) return 10;
    synthExit();free(raw);return 0;
}
