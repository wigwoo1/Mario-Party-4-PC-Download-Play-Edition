#include <assert.h>
#include <stdio.h>
#include "../src/port/pcm_output.h"

/* Test actual SDL conversion, including gain changed after data was queued.
 * No playback device or decoded game asset is needed. */
int main(void) {
    for(unsigned channels=2;channels<=6;channels+=4) {
        SDL_AudioSpec input={SDL_AUDIO_S16,(int)channels,32000};
        SDL_AudioSpec converted={SDL_AUDIO_F32,(int)channels,32000};
        PartyboardPcmOutput output={0};
        int16_t samples[48],unchanged[48];
        float result[48];
        for(unsigned i=0;i<48;++i) samples[i]=unchanged[i]=(int16_t)((int)i*600-14000);
        output.stream=SDL_CreateAudioStream(&input,&converted);assert(output.stream);
        output.channels=channels;output.frequency=32000;
        for(unsigned step=0;step<4;++step) {
            const float gain=step==0?0.5f:step==1?0.0f:step==2?1.0f:0.37f;
            assert(PartyboardPcmOutputSubmit(&output,samples,48/channels)==1);
            assert(SDL_GetAudioStreamQueued(output.stream)==sizeof(samples));
            assert(PartyboardPcmOutputSetGain(&output,gain));
            assert(SDL_GetAudioStreamQueued(output.stream)==sizeof(samples));
            assert(SDL_GetAudioStreamData(output.stream,result,sizeof(result))==sizeof(result));
            for(unsigned i=0;i<48;++i) {
                assert(fabsf(result[i]-samples[i]/32768.0f*gain)<0.00001f);
                assert(samples[i]==unchanged[i]);
            }
            assert(SDL_GetAudioStreamQueued(output.stream)==0);
        }
        assert(!PartyboardPcmOutputSetGain(&output,-0.1f));
        assert(!PartyboardPcmOutputSetGain(&output,1.1f));
        assert(!PartyboardPcmOutputSetGain(&output,NAN));
        assert(!PartyboardPcmOutputSetGain(&output,INFINITY));
        assert(SDL_GetAudioStreamGain(output.stream)==0.37f);
        PartyboardPcmOutputClose(&output);
        assert(PartyboardPcmOutputSetGain(&output,0.0f));
        assert(!PartyboardPcmOutputSetGain(NULL,0.0f));
        printf("PASS actual SDL %u-channel volume, mute/unmute, queued data, source preservation and invalid gain\n",channels);
    }
}
