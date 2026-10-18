#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#include "../src/port/musyx_software.h"
static PartyboardMusyxSoftware owner;
static uint8_t *raw;
static size_t raw_bytes;
static int16_t prefill[8000*6];
static uint32_t hash_word(uint32_t hash,uint32_t value){return (hash^value)*16777619u;}
static uint32_t run(unsigned kind,unsigned channels,unsigned mode){
    SDL_AudioSpec spec={SDL_AUDIO_S16,(int)channels,32000};
    SND_FVECTOR origin={0},heading={0,0,-1},position={0,0,kind==3?30:-30};
    uint64_t energy[6]={0};uint32_t pcm_hash=2166136261u,state=2166136261u;
    assert(PartyboardMusyxSoftwareInit(&owner,raw,raw_bytes));
    assert(!PartyboardMusyxSoftwareSetChannels(&owner,4));
    assert(PartyboardMusyxSoftwareSetChannels(&owner,channels));
    assert(((synthFlags&2)!=0)==(channels==6));
    assert(PartyboardMusyxAuxRuntimeSelect(&owner.aux,&owner.container,-2,-2));
    if(mode!=2){
        owner.output.stream=SDL_CreateAudioStream(&spec,&spec);assert(owner.output.stream);
        owner.output.frequency=32000;owner.output.channels=channels;
        assert(PartyboardMusyxPcmBridgeSetOutput(&owner.bridge,&owner.output));
        if(mode==1)assert(SDL_PutAudioStreamData(owner.output.stream,prefill,8000*(int)channels*2));
    }
    int handle;
    if(kind<2)handle=PartyboardMusyxSoftwareEffect(&owner,0,127,64,0,0,0,kind?127:0);
    else{
        assert(PartyboardMusyxSoftwareListener(&owner,&origin,&heading,100,343,0,25,25));
        handle=PartyboardMusyxSoftwarePositional(&owner,0,&position,127,0,0);
    }
    assert(handle>0);
    for(unsigned tick=0;tick<60;++tick){
        uint64_t before=owner.driver.completed_blocks;
        assert(PartyboardMusyxSoftwareTick(&owner));
        assert(owner.driver.completed_blocks==(uint64_t)(tick+1)*200/60);
        state=hash_word(state,(uint32_t)synthRealTime);
        state=hash_word(state,(uint32_t)owner.periodic_calls);
        PartyboardMusyxSoftwareEffectSlot *effect=PartyboardMusyxSoftwareFindEffect(&owner,handle);
        state=hash_word(state,effect?(uint32_t)effect->status:1);
        if(mode==0){
            int16_t pcm[640*6];unsigned frames=(unsigned)(owner.driver.completed_blocks-before)*160;
            int bytes=(int)(frames*channels*2);
            assert(PartyboardPcmOutputFinish(&owner.output));
            assert(SDL_GetAudioStreamData(owner.output.stream,pcm,bytes)==bytes);
            for(unsigned i=0;i<frames*channels;++i){
                energy[i%channels]+=(unsigned)(pcm[i]<0?-(int)pcm[i]:pcm[i]);
                pcm_hash=hash_word(pcm_hash,(uint16_t)pcm[i]);
                if(channels==6&&(i%6==2||i%6==3))assert(pcm[i]==0);
            }
        }
    }
    if(mode==0){
        assert(owner.driver.accepted_blocks==200&&!owner.driver.dropped_blocks);
        uint64_t front=energy[0]+energy[1],rear=energy[4]+energy[5];
        if(channels==2)assert(front>100);
        else{
            assert(energy[4]==energy[5]);assert(!energy[2]&&!energy[3]);
            if(kind==0||kind==2)assert(front>rear&&front>100);
            else assert(rear>front&&rear>100);
        }
    }else assert(owner.driver.dropped_blocks==200&&!owner.driver.accepted_blocks);
    assert(!owner.driver.device_errors&&owner.driver.completed_blocks==200);
    assert(PartyboardMusyxSoftwareShutdown(&owner));assert(!owner.active&&!sndActive&&!synthVoice&&!dspVoice);
    printf("{\"case\":%u,\"channels\":%u,\"mode\":%u,\"state\":%u,\"pcm\":%u,\"energy\":[%llu,%llu,%llu,%llu,%llu,%llu],\"blocks\":200}\n",
        kind,channels,mode,state,pcm_hash,(unsigned long long)energy[0],(unsigned long long)energy[1],(unsigned long long)energy[2],
        (unsigned long long)energy[3],(unsigned long long)energy[4],(unsigned long long)energy[5]);
    return state;
}
int main(void){
    uint8_t header[8];
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);
#endif
    assert(fread(header,1,8,stdin)==8);raw_bytes=PartyboardMsmU32(header);assert(raw_bytes<=67108864);
    raw=malloc(raw_bytes);assert(raw&&fread(raw,1,raw_bytes,stdin)==raw_bytes);
    for(unsigned kind=0;kind<4;++kind){
        uint32_t baseline=run(kind,2,0);
        for(unsigned channels=2;channels<=6;channels+=4)for(unsigned mode=0;mode<3;++mode){
            if(channels==2&&mode==0)continue;
            assert(run(kind,channels,mode)==baseline);
        }
    }
    free(raw);return 0;
}
