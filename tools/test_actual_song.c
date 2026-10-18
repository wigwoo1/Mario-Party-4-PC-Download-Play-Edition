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
#include "../src/port/musyx_arrangement_convert.h"
#include "../src/port/musyx_voice_source.h"
#include "../src/port/musyx_song_runtime.h"
void hwSetTimeOffset(u8 offset);
int main(void) {
    uint8_t header[8],*raw;
    size_t bytes,sequence_bytes;
    const uint8_t *sequence;
    unsigned song_index,i,loaded=0,indices[24],selected=0;
    unsigned seen[64]={0};
    int16_t first_pcm[64][32];
    size_t first_count[64]={0};
    uint32_t first_id[64]={0},first_offset[64]={0};
    PartyboardMsmContainer container;
    PartyboardMsmContainerInfo info;
    PartyboardMsmContainerSong song;
    PartyboardMsmContainerGroup descriptors[24];
    PartyboardMusyxGroup groups[24]={0};
    PartyboardMusyxSampleMemory memory={0};
    PartyboardMusyxSong runtime={0};
    DSPvoice voices[64]={0};
    SND_PLAYPARA parameters={0};
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);
#endif
    if(fread(header,1,8,stdin)!=8) return 2;
    bytes=PartyboardMsmU32(header);song_index=PartyboardMsmU32(header+4);
    if(bytes<96 || bytes>67108864) return 2;
    raw=malloc(bytes);if(!raw) return 3;
    if(fread(raw,1,bytes,stdin)!=bytes) return 4;
    if(!PartyboardMsmContainerOpen(&container,raw,bytes)
            || !PartyboardMsmContainerGetInfo(&info,&container)
            || !PartyboardMsmContainerGetSong(&song,&container,song_index)
            || !PartyboardMsmContainerResolveSong(&sequence,&sequence_bytes,&container,&song)) return 5;
    for(i=0;i<info.base_count;++i) indices[loaded++]=info.base_groups[i];
    for(i=0;i<info.groups;++i) {
        PartyboardMsmContainerGroup group;
        unsigned j;
        if(!PartyboardMsmContainerGetGroup(&group,&container,i)) return 5;
        if(group.id!=song.group) continue;
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
    if(song.volume<0 || song.volume>127) return 12;
    parameters.flags=SND_PLAYPARA_VOLUME;
    parameters.volume.time=0;
    parameters.volume.target=(u8)song.volume; /* Original MSM default base/fade volumes127. */
    if(PartyboardMusyxSongStart(&runtime,sequence,sequence_bytes,65535,(u16)song.song,NULL,1,0)
            || runtime.arrangement.data) return 11;
    if(!PartyboardMusyxSongStartContainer(&runtime,&container,song_index,indices,loaded,1,0)) return 8;
    if(PartyboardMusyxSongStart(&runtime,sequence,sequence_bytes,(u16)song.group,(u16)song.song,NULL,1,0)) return 11;
    for(i=0;i<2000;++i) {
        unsigned voice;
        hwSetTimeOffset((u8)(i%5));
        seqHandle(256);synthHandle(256);
        for(voice=0;voice<64;++voice) if(!seen[voice] && voices[voice].smp_info.addr) {
            PartyboardMsmSample source;
            if(!PartyboardMusyxVoiceSourceOpen(&source,&voices[voice],&memory)
                    || !(first_count[voice]=PartyboardMsmSampleRead(&source,first_pcm[voice],32))) return 9;
            first_id[voice]=voices[voice].smp_id;first_offset[voice]=voices[voice].smp_info.offset;
            seen[voice]=1;++selected;
        }
    }
    for(i=0;i<64;++i) if(voices[i].smp_info.addr) {
        PartyboardMsmSample source;int16_t pcm[32];
        if(!PartyboardMusyxVoiceSourceOpen(&source,&voices[i],&memory)
                || !PartyboardMsmSampleRead(&source,pcm,32)) return 9;
    }
    printf("%u %u %u\n",song_index,loaded,selected);
    for(i=0;i<64;++i) if(seen[i]) {
        size_t sample;
        printf("%u %u %u %zu\n",i,first_id[i],first_offset[i],first_count[i]);
        for(sample=0;sample<first_count[i];++sample) printf("%d\n",first_pcm[i][sample]);
    }
    if(!PartyboardMusyxSongStop(&runtime) || PartyboardMusyxSongStop(&runtime)) return 11;
    synthKillAllVoices(0);
    for(i=loaded;i;--i) if(!PartyboardMusyxGroupPop(&groups[i-1],&memory,descriptors[i-1].samples)) return 10;
    synthExit();free(raw);return 0;
}
