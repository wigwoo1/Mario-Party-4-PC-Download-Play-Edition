/* Real SDL source-queue checks; opens a device only with --device-probe. */
#include "../src/port/pcm_output.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(int argc, char **argv) {
    PartyboardPcmOutput output = {0};
    assert(!PartyboardPcmOutputDeviceChannels(NULL));
    assert(!PartyboardPcmOutputDeviceChannels(&output));
    for(unsigned channels=2;channels<=6;channels+=4) {
        SDL_AudioSpec spec={SDL_AUDIO_S16,(int)channels,32000};
        int16_t samples[8000*6]={0};
        int16_t recovered[8000*6]={0};
        size_t sample_count=8000*channels;
        for(size_t sample=0;sample<sample_count;++sample)
            samples[sample]=(int16_t)((int)(sample%30000)-15000);
        output.stream=SDL_CreateAudioStream(&spec,&spec);
        assert(output.stream);
        output.frequency=32000;output.channels=channels;
        assert(!PartyboardPcmOutputDeviceChannels(&output));
        assert(PartyboardPcmOutputCapacity(&output,8000)==1);
        assert(PartyboardPcmOutputSubmit(&output,samples,8000)==1);
        assert(SDL_GetAudioStreamQueued(output.stream)==(int)(8000*channels*2));
        assert(!PartyboardPcmOutputCapacity(&output,1));
        assert(!PartyboardPcmOutputSubmit(&output,samples,1));
        assert(!PartyboardPcmOutputSubmitStereo(&output,samples,1));
        assert(SDL_GetAudioStreamQueued(output.stream)==(int)(8000*channels*2));
        assert(PartyboardPcmOutputFinish(&output));
        assert(SDL_GetAudioStreamData(output.stream,recovered,(int)(sample_count*sizeof(int16_t)))
            ==(int)(sample_count*sizeof(int16_t)));
        assert(!memcmp(samples,recovered,sample_count*sizeof(int16_t)));
        assert(SDL_ClearAudioStream(output.stream));
        assert(PartyboardPcmOutputCapacity(&output,8000)==1);
        assert(PartyboardPcmOutputSubmit(&output,NULL,1)==-1);
        assert(PartyboardPcmOutputSubmitStereo(&output,samples,3201)==-1);
        assert(PartyboardPcmOutputSubmitStereo(&output,NULL,1)==-1);
        assert(!SDL_GetAudioStreamQueued(output.stream));
        assert(PartyboardPcmOutputSubmitStereo(&output,samples,3200)==1);
        assert(PartyboardPcmOutputFinish(&output));
        assert(SDL_GetAudioStreamData(output.stream,recovered,(int)(3200*channels*2))==(int)(3200*channels*2));
        for(size_t frame=0;frame<3200;++frame) {
            assert(recovered[frame*channels]==samples[frame*2]);
            assert(recovered[frame*channels+1]==samples[frame*2+1]);
            for(unsigned channel=2;channel<channels;++channel) assert(!recovered[frame*channels+channel]);
        }
        PartyboardPcmOutputClose(&output);
        assert(!output.channels && !output.frequency && !output.stream);
    }
    if (argc == 2 && (!strcmp(argv[1], "--device-probe") || !strcmp(argv[1], "--surround-device-probe"))) {
        unsigned channels=!strcmp(argv[1],"--surround-device-probe") ? 6 : 2;
        int16_t silence[3200*6] = {0};
        SDL_AudioSpec device_spec;
        int device_frames;
        int before, after;
        if (!PartyboardPcmOutputOpenChannels(&output, 32000,channels)) {
            fprintf(stderr, "Device open failed: %s\n", SDL_GetError());
            return 4;
        }
        if(!SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(output.stream),&device_spec,&device_frames)) {
            fprintf(stderr,"Device format unavailable: %s\n",SDL_GetError());
            PartyboardPcmOutputClose(&output);
            return 8;
        }
        printf("Source channels=%u, default device channels=%d frequency=%d buffer frames=%d; no audible or speaker mapping acceptance\n",
               channels,device_spec.channels,device_spec.freq,device_frames);
        assert(PartyboardPcmOutputDeviceChannels(&output)==(unsigned)device_spec.channels);
        if (PartyboardPcmOutputSubmit(&output, silence, 3200) != 1) {
            fprintf(stderr, "PCM submit failed: %s\n", SDL_GetError());
            PartyboardPcmOutputClose(&output);
            return 5;
        }
        before = SDL_GetAudioStreamQueued(output.stream);
        if (!PartyboardPcmOutputFinish(&output)) {
            PartyboardPcmOutputClose(&output);
            return 7;
        }
        SDL_Delay(500);
        after = SDL_GetAudioStreamQueued(output.stream);
        printf("Default device source queue bytes before=%d after=%d; silence only, no audible acceptance\n", before, after);
        PartyboardPcmOutputClose(&output);
        return before > 0 && after == 0 ? 0 : 6;
    }
    /* Keep all adapter calls compiled while avoiding device access. */
    if (argc == 1000) {
        int16_t samples[2] = {0};
        if (!PartyboardPcmOutputOpen(&output, 32000)) return 1;
        if (PartyboardPcmOutputSubmit(&output, samples, 1) < 0) return 2;
        PartyboardPcmOutputClose(&output);
    }
    puts("SDL stereo/six-channel source queue size, capacity and cleanup PASS; no device acceptance");
    return PartyboardPcmOutputCapacity(&output, 1) == -1 ? 0 : 3;
}
