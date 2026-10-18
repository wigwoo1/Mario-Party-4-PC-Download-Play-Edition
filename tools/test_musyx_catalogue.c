#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#include "../src/port/musyx_software.h"

static PartyboardMusyxSoftware owner;
static uint32_t render(unsigned ticks,unsigned *nonzero) {
    uint32_t hash=2166136261u;
    *nonzero=0;
    for(unsigned tick=0;tick<ticks;++tick) {
        int16_t pcm[1280];
        uint64_t before=owner.driver.completed_blocks;
        if(!PartyboardMusyxSoftwareTick(&owner)) {
            fprintf(stderr,"catalogue tick failed: tick=%u voice=%u\n",tick,owner.scheduler.failed_voice);
            exit(9);
        }
        size_t frames=(size_t)(owner.driver.completed_blocks-before)*160;
        assert(frames<=640 && PartyboardPcmOutputFinish(&owner.output));
        assert(SDL_GetAudioStreamData(owner.output.stream,pcm,(int)(frames*4))==(int)(frames*4));
        for(size_t sample=0;sample<frames*2;++sample) {
            if(pcm[sample])++*nonzero;
            hash=(hash^((uint16_t)pcm[sample]&255))*16777619u;
            hash=(hash^((uint16_t)pcm[sample]>>8))*16777619u;
        }
    }
    return hash;
}

int main(int argc,char **argv) {
    uint8_t header[8],*raw;
    size_t bytes;
    unsigned mode=argc>1?(unsigned)strtoul(argv[1],NULL,10):0;
    unsigned first=argc>2?(unsigned)strtoul(argv[2],NULL,10):0;
    unsigned requested=argc>3?(unsigned)strtoul(argv[3],NULL,10):UINT32_MAX;
    SDL_AudioSpec spec={SDL_AUDIO_S16,2,32000};
    assert(mode<=2);
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);
#endif
    assert(fread(header,1,8,stdin)==8);
    bytes=PartyboardMsmU32(header);assert(bytes<=67108864);
    raw=malloc(bytes);assert(raw && fread(raw,1,bytes,stdin)==bytes);
    assert(PartyboardMusyxSoftwareInit(&owner,raw,bytes));
    owner.output.stream=SDL_CreateAudioStream(&spec,&spec);assert(owner.output.stream);
    owner.output.frequency=32000;owner.output.channels=2;
    assert(PartyboardMusyxPcmBridgeSetOutput(&owner.bridge,&owner.output));
    unsigned count=mode==0?owner.info.songs:owner.info.sound_effects;
    unsigned ticks=mode==0?120:12,limit=count;
    if(requested<count && first<count && requested<count-first)limit=first+requested;
    assert(first<=limit);
    for(unsigned index=first;index<limit;++index) {
        int handle=-1,skip=0;
        unsigned nonzero;
        uint64_t blocks=owner.driver.completed_blocks;
        fprintf(stderr,"catalogue mode=%u entry=%u\n",mode,index);fflush(stderr);
        assert(PartyboardMusyxSoftwareScene(&owner,0));
        if(mode==0) {
            PartyboardMsmContainerSong descriptor;
            assert(PartyboardMsmContainerGetSong(&descriptor,&owner.container,index));
            skip=descriptor.group==65535 || !descriptor.bytes;
            if(!skip)handle=PartyboardMusyxSoftwareSong(&owner,index,0);
        }else{
            PartyboardMsmContainerEffect descriptor;
            assert(PartyboardMsmContainerGetEffect(&descriptor,&owner.container,index));
            skip=descriptor.group==65535 || descriptor.effect==65535;
            if(!skip && mode==1)handle=PartyboardMusyxSoftwareEffect(&owner,index,127,64,0,-1,-1,-1);
            if(!skip && mode==2) {
                SND_FVECTOR origin={0},heading={0,0,-1},position={10,2,-5};
                assert(PartyboardMusyxSoftwareListener(&owner,&origin,&heading,100,343,0,25,25));
                handle=PartyboardMusyxSoftwarePositional(&owner,index,&position,127,-1,-1);
            }
        }
        uint32_t hash=render(ticks,&nonzero);
        printf("{\"mode\":%u,\"entry\":%u,\"skip\":%d,\"started\":%d,\"blocks\":%llu,\"nonzero\":%u,\"pcm\":%u}\n",
            mode,index,skip,handle>=0,(unsigned long long)(owner.driver.completed_blocks-blocks),nonzero,hash);
        fflush(stdout);
    }
    assert(!owner.driver.device_errors && !owner.driver.dropped_blocks);
    assert(PartyboardMusyxSoftwareShutdown(&owner));
    assert(!owner.active && !sndActive && !synthVoice && !dspVoice);
    free(raw);return 0;
}
