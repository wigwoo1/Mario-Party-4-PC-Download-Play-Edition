/* Offline complete-clip diagnostic. Uses the same owner/output path as the
 * catalogue fixture, with no game, network, device playback or engine changes. */
#define main catalogue_main
#include "test_musyx_catalogue.c"
#undef main
#ifdef PARTYBOARD_VOICE_LIFETIME_TRACE
static SND_MESSAGE_CALLBACK original_message;
static unsigned trace_index=UINT32_MAX,trace_effect,trace_tick;
static u32 trace_message(u32 message,u32 value) {
    unsigned index=value&0xff;
    if(index==trace_index) {
        DSPvoice *v=&owner.voices[index];
        fprintf(stderr,"VOICE_MESSAGE fx=%u tick=%u index=%u msg=%u cookie=%u synthId=%u sample=%u state=%u pos=%u length=%u offset=%u codec=%u\n",trace_effect,trace_tick,index,message,value,synthVoice[index].id,v->smp_id,v->state,v->playInfo.posHi,v->smp_info.length,v->smp_info.offset,v->smp_info.compType);
    }
    return original_message(message,value);
}
#endif

int main(int argc,char **argv) {
    uint8_t header[8],*raw;
    SDL_AudioSpec spec={SDL_AUDIO_S16,2,32000};
    unsigned ticks=argc>2?(unsigned)strtoul(argv[2],NULL,10):720;
    unsigned song=argc>3?(unsigned)strtoul(argv[3],NULL,10):0;
    assert(song<=2);
    assert(argc>=2 && ticks>=12 && ticks<=1200);
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);
#endif
    assert(fread(header,1,8,stdin)==8);
    size_t bytes=PartyboardMsmU32(header);assert(bytes>=96 && bytes<=67108864);
    raw=malloc(bytes);assert(raw && fread(raw,1,bytes,stdin)==bytes);
    assert(PartyboardMusyxSoftwareInit(&owner,raw,bytes));
#ifdef PARTYBOARD_VOICE_LIFETIME_TRACE
    original_message=salMessageCallback;assert(original_message);hwSetMesgCallback(trace_message);
#endif
    owner.output.stream=SDL_CreateAudioStream(&spec,&spec);assert(owner.output.stream);
    owner.output.frequency=32000;owner.output.channels=2;
    assert(PartyboardMusyxPcmBridgeSetOutput(&owner.bridge,&owner.output));
    if(song==2)assert(PartyboardMusyxSoftwareSong(&owner,44,0)>=0);
    for(unsigned effect=109;effect<=116;++effect) {
        char filename[2048];assert(snprintf(filename,sizeof filename,"%s/selection-%u.pcm",argv[1],effect)>0);
        FILE *file=fopen(filename,"wb");assert(file);
        if(song!=2)assert(PartyboardMusyxSoftwareScene(&owner,0));
        if(song==1)assert(PartyboardMusyxSoftwareSong(&owner,44,0)>=0);
        int handle=PartyboardMusyxSoftwareEffect(&owner,effect,127,64,0,-1,-1,-1);
        assert(handle>=0);
#ifdef PARTYBOARD_VOICE_LIFETIME_TRACE
        trace_effect=effect;trace_tick=0;
        uint32_t internal=vidGetInternalId(PartyboardMusyxSoftwareVoice(&owner,handle));
        assert(internal!=UINT32_MAX);trace_index=internal&0xff;
        fprintf(stderr,"VOICE_START fx=%u index=%u internal=%u oldPos=%u oldLength=%u\n",effect,trace_index,internal,owner.voices[trace_index].playInfo.posHi,owner.voices[trace_index].smp_info.length);
#endif
        uint64_t frame=0,last_nonzero=0,nonzero=0;
        uint32_t opening_hash=2166136261u;
        int first_inactive=-1,peak=0;
        unsigned max_source_length=0,source_rate=0;
        for(unsigned tick=0;tick<ticks;++tick) {
#ifdef PARTYBOARD_VOICE_LIFETIME_TRACE
            trace_tick=tick+1;
#endif
            int16_t pcm[1280];uint64_t before=owner.driver.completed_blocks;
            assert(PartyboardMusyxSoftwareTick(&owner));
            size_t frames=(size_t)(owner.driver.completed_blocks-before)*160;
            assert(frames<=640 && PartyboardPcmOutputFinish(&owner.output));
            assert(SDL_GetAudioStreamData(owner.output.stream,pcm,(int)(frames*4))==(int)(frames*4));
            assert(fwrite(pcm,4,frames,file)==frames);
            for(size_t i=0;i<frames*2;++i) {
                int magnitude=pcm[i]<0?-(int)pcm[i]:(int)pcm[i];if(magnitude>peak)peak=magnitude;
                if(pcm[i]){++nonzero;last_nonzero=frame+i/2;}
                if(tick<12){opening_hash=(opening_hash^((uint16_t)pcm[i]&255))*16777619u;opening_hash=(opening_hash^((uint16_t)pcm[i]>>8))*16777619u;}
            }
            for(unsigned i=0;i<owner.info.voices;++i)
                if(owner.voices[i].state && owner.voices[i].smp_info.length>max_source_length){max_source_length=owner.voices[i].smp_info.length;source_rate=owner.voices[i].smp_info.info&0xffffff;}
            if(first_inactive<0 && !PartyboardMusyxSoftwareFindEffect(&owner,handle))first_inactive=(int)tick+1;
#ifdef PARTYBOARD_VOICE_LIFETIME_TRACE
            if(trace_index!=UINT32_MAX && (tick<2 || first_inactive==(int)tick+1)) {
                DSPvoice *v=&owner.voices[trace_index];
                fprintf(stderr,"VOICE_TICK fx=%u tick=%u index=%u active=%d sample=%u state=%u pos=%u length=%u cookie=%u\n",effect,tick+1,trace_index,first_inactive<0,v->smp_id,v->state,v->playInfo.posHi,v->smp_info.length,v->mesgCallBackUserValue);
            }
            if(first_inactive>=0)trace_index=UINT32_MAX;
#endif
            frame+=frames;
        }
        assert(!fclose(file));
        printf("{\"effect\":%u,\"menuSong\":%u,\"ticks\":%u,\"frames\":%llu,\"openingHash\":%u,\"firstInactiveTick\":%d,\"lastNonzeroFrame\":%llu,\"nonzeroSamples\":%llu,\"peak\":%d,\"maxSourceLength\":%u,\"sourceRate\":%u}\n",effect,song?44:0,ticks,(unsigned long long)frame,opening_hash,first_inactive,(unsigned long long)last_nonzero,(unsigned long long)nonzero,peak,max_source_length,source_rate);
    }
    assert(!owner.driver.device_errors && !owner.driver.dropped_blocks);
    assert(PartyboardMusyxSoftwareShutdown(&owner));free(raw);return 0;
}
