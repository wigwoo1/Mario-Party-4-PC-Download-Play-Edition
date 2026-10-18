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
    uint32_t hash=2166136261u; *nonzero=0;
    for(unsigned tick=0;tick<ticks;++tick) {
        int16_t pcm[1280]; unsigned before=(unsigned)owner.driver.completed_blocks,frames;
        if(!PartyboardMusyxSoftwareTick(&owner)) {
            fprintf(stderr,"owner tick failed tick%u voice%u\n",tick,owner.scheduler.failed_voice); exit(9);
        }
        frames=((unsigned)owner.driver.completed_blocks-before)*160;
        assert(PartyboardPcmOutputFinish(&owner.output));
        assert(SDL_GetAudioStreamData(owner.output.stream,pcm,(int)(frames*4))==(int)(frames*4));
        for(unsigned i=0;i<frames*2;++i) {
            if(pcm[i]) ++*nonzero;
            hash=(hash^((uint16_t)pcm[i]&255))*16777619u; hash=(hash^((uint16_t)pcm[i]>>8))*16777619u;
        }
    }
    return hash;
}
int main(void) {
    uint8_t header[8],*raw; size_t bytes; uint32_t fx_hash[2],song_hash[2]; unsigned fx_nonzero=0,song_nonzero=0;
    SDL_AudioSpec spec={SDL_AUDIO_S16,2,32000};
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);
#endif
    assert(fread(header,1,8,stdin)==8);bytes=PartyboardMsmU32(header);assert(bytes<=67108864);
    raw=malloc(bytes);assert(raw && fread(raw,1,bytes,stdin)==bytes);
    assert(!PartyboardMusyxSoftwareInit(&owner,raw,16) && !owner.active);
    for(unsigned pass=0;pass<2;++pass) {
        assert(PartyboardMusyxSoftwareInit(&owner,raw,bytes));
        assert(!PartyboardMusyxSoftwareInit(&owner,raw,bytes));
        assert(owner.loaded==owner.base_loaded && owner.aux.selected[0]==owner.info.aux_a);
        owner.output.stream=SDL_CreateAudioStream(&spec,&spec);assert(owner.output.stream);
        owner.output.frequency=32000;owner.output.channels=2;
        assert(PartyboardMusyxPcmBridgeSetOutput(&owner.bridge,&owner.output));
        assert(PartyboardMusyxSoftwareEffect(&owner,UINT32_MAX,127,64,0,-1,-1,-1)==-1);
        assert(PartyboardMusyxSoftwareSong(&owner,UINT32_MAX,0)==-1);
        assert(PartyboardMusyxSoftwareEffect(&owner,0,127,64,0,-1,-1,-1)>0);
        fx_hash[pass]=render(60,&fx_nonzero);assert(fx_nonzero>100);
        assert(PartyboardMusyxSoftwareScene(&owner,18));
        assert(PartyboardMusyxSoftwareSong(&owner,0,0)==0);
        song_hash[pass]=render(120,&song_nonzero);assert(song_nonzero>100);
        assert(owner.effect_starts==1 && owner.song_starts==1 && owner.driver.completed_blocks==600);
        assert(PartyboardMusyxSoftwareScene(&owner,18));
        assert(!owner.songs[0].arrangement.data && owner.loaded<=owner.base_loaded+1);
        assert(PartyboardMusyxSoftwareShutdown(&owner));
        assert(!sndActive && !synthVoice && !dspVoice && !owner.active && !owner.loaded);
        assert(PartyboardMusyxSoftwareShutdown(&owner));
    }
    assert(fx_hash[0]==fx_hash[1] && song_hash[0]==song_hash[1]);
    printf("{\"passes\":2,\"fxHash\":%u,\"songHash\":%u,\"fxNonzero\":%u,\"songNonzero\":%u,\"blocksPerPass\":600}\n",
        fx_hash[0],song_hash[0],fx_nonzero,song_nonzero);
    free(raw);return 0;
}
