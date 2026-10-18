/* Private-data lifecycle acceptance. No playback device or PCM file is opened.
 * Observe the original SDK's natural ending/first reported loop, then exercise
 * fade/stop and ownership cleanup. This is not an audible fidelity reference. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#include "../src/port/musyx_software.h"

static PartyboardMusyxSoftware owner;
static uint32_t pcm_hash;
static uint64_t samples,nonzero,energy[6];
static void tick(void) {
    int16_t pcm[640*6];
    uint64_t before=owner.driver.completed_blocks;
    assert(PartyboardMusyxSoftwareTick(&owner));
    size_t frames=(size_t)(owner.driver.completed_blocks-before)*160;
    size_t count=frames*owner.output.channels;
    assert(frames<=640 && PartyboardPcmOutputFinish(&owner.output));
    assert(SDL_GetAudioStreamData(owner.output.stream,pcm,(int)(count*sizeof(int16_t)))==(int)(count*sizeof(int16_t)));
    for(size_t index=0;index<count;++index) {
        int value=pcm[index];
        if(value)++nonzero;
        energy[index%owner.output.channels]+=(uint64_t)(value<0?-value:value);
        pcm_hash=(pcm_hash^((uint16_t)value&255))*16777619u;
        pcm_hash=(pcm_hash^((uint16_t)value>>8))*16777619u;
    }
    samples+=count;
}

int main(int argc,char **argv) {
    unsigned channels=argc>1?(unsigned)strtoul(argv[1],NULL,10):2;
    unsigned first=argc>2?(unsigned)strtoul(argv[2],NULL,10):0;
    unsigned requested=argc>3?(unsigned)strtoul(argv[3],NULL,10):UINT32_MAX;
    uint8_t header[8],*raw;
    assert(channels==2 || channels==6);
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);
#endif
    assert(fread(header,1,8,stdin)==8);
    size_t bytes=PartyboardMsmU32(header);assert(bytes<=67108864);
    raw=malloc(bytes);assert(raw && fread(raw,1,bytes,stdin)==bytes);
    assert(PartyboardMusyxSoftwareInit(&owner,raw,bytes));
    unsigned total=owner.info.songs;
    assert(PartyboardMusyxSoftwareShutdown(&owner));
    unsigned limit=total;
    if(requested<total && first<total && requested<total-first)limit=first+requested;
    assert(first<limit);
    for(unsigned song=first;song<limit;++song) {
        assert(PartyboardMusyxSoftwareInit(&owner,raw,bytes));
        assert(PartyboardMusyxSoftwareSetChannels(&owner,channels));
        SDL_AudioSpec spec={SDL_AUDIO_S16,(int)channels,32000};
        owner.output.stream=SDL_CreateAudioStream(&spec,&spec);assert(owner.output.stream);
        owner.output.frequency=32000;owner.output.channels=channels;
        assert(PartyboardMusyxPcmBridgeSetOutput(&owner.bridge,&owner.output));
        assert(PartyboardMusyxSoftwareSong(&owner,song,0)==0);
        SND_SEQID sequence=owner.songs[0].sequence;
        pcm_hash=2166136261u;samples=nonzero=0;memset(energy,0,sizeof energy);
        unsigned ticks=0,loops=0;
        while(ticks<600*60 && owner.songs[0].arrangement.data) {
            tick();++ticks;
            if(owner.songs[0].arrangement.data)loops=sndSeqGetLoopCnt(sequence);
            if(loops)break;
        }
        int ended=owner.songs[0].arrangement.data==NULL;
        fprintf(stderr,"song=%u channels=%u ticks=%u ended=%d loops=%u\n",song,channels,ticks,ended,loops);fflush(stderr);
        assert(ticks>0 && (ended || loops));
        if(!ended)assert(PartyboardMusyxSoftwareFadeSong(&owner,0,90));
        for(unsigned tail=0;tail<60;++tail)tick();
        assert(!owner.songs[0].arrangement.data && !sndSeqGetValid(sequence));
        assert(owner.driver.completed_blocks==(uint64_t)(ticks+60)*200/60);
        assert(owner.periodic_calls==owner.driver.completed_blocks/3);
        assert(!owner.driver.device_errors && !owner.driver.dropped_blocks && samples>0 && nonzero>0);
        if(channels==6)assert(energy[2]==0 && energy[3]==0 && energy[4]==energy[5]);
        printf("{\"song\":%u,\"channels\":%u,\"ticks\":%u,\"ended\":%d,\"loops\":%u,\"tailTicks\":60,\"blocks\":%llu,\"periodic\":%llu,\"samples\":%llu,\"nonzero\":%llu,\"pcm\":%u,\"energy\":[%llu,%llu,%llu,%llu,%llu,%llu]}\n",
            song,channels,ticks,ended,loops,(unsigned long long)owner.driver.completed_blocks,(unsigned long long)owner.periodic_calls,
            (unsigned long long)samples,(unsigned long long)nonzero,pcm_hash,(unsigned long long)energy[0],(unsigned long long)energy[1],
            (unsigned long long)energy[2],(unsigned long long)energy[3],(unsigned long long)energy[4],(unsigned long long)energy[5]);fflush(stdout);
        assert(PartyboardMusyxSoftwareShutdown(&owner));
        assert(!owner.active && !sndActive && !synthVoice && !dspVoice);
    }
    free(raw);return 0;
}
