/* Production HuAud API regression; metadata fixture only, no device proof. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include "../src/port/audio.c"

s16 omSysExitReq;
PlayerConfig GWPlayerCfg[4];
/* Fixed preference for this metadata fixture; real configuration is exercised
 * by the separate native/browser settings and actual-disc browser tests. */
unsigned partyboard_settings_audio_channels(void) { return 2; }
static float outputGain=1.0f;
float partyboard_settings_audio_gain(void) { return outputGain; }
BOOL DVDOpen(const char *path, DVDFileInfo *file) { (void)path; (void)file; return FALSE; }
BOOL DVDClose(DVDFileInfo *file) { (void)file; return TRUE; }
s32 DVDReadPrio(DVDFileInfo *file, void *data, s32 size, s32 offset, s32 priority) {
    (void)file; (void)data; (void)size; (void)offset; (void)priority; return -1;
}
void OSReport(const char *format, ...) {
    va_list args; va_start(args, format); vprintf(format, args); va_end(args);
}

int main(int argc, char **argv) {
    s32 first, second;
    /* Real adapter applies local gain to BOTH output owners, without requiring
     * a loaded game or a playback device. Missing owners remain valid. */
    {
        const SDL_AudioSpec spec={SDL_AUDIO_S16,2,32000};
        portSynth.output.stream=SDL_CreateAudioStream(&spec,&spec);
        portStreamOutput.stream=SDL_CreateAudioStream(&spec,&spec);
        assert(portSynth.output.stream&&portStreamOutput.stream);
        for(unsigned step=0;step<3;++step) {
            outputGain=step==0?0.37f:step==1?0.0f:1.0f;
            assert(HuAudPortApplyOutputSettings());
            assert(SDL_GetAudioStreamGain(portSynth.output.stream)==outputGain);
            assert(SDL_GetAudioStreamGain(portStreamOutput.stream)==outputGain);
        }
        PartyboardPcmOutputClose(&portSynth.output);
        PartyboardPcmOutputClose(&portStreamOutput);
        assert(HuAudPortApplyOutputSettings());
        puts("PASS production adapter gain applies to synthesizer and streamed music, mute/unmute and absent devices");
    }
    if (argc == 2 && !strcmp(argv[1], "--private-pdt")) {
        unsigned id, verified = 0;
        portPdt = (uint8_t *)malloc(64 * 1024 * 1024);
        assert(portPdt);
        portPdtBytes = fread(portPdt, 1, 64 * 1024 * 1024, stdin);
        assert(feof(stdin) && !ferror(stdin));
        for (id = 0; id < PartyboardPdtU16(portPdt + 2); ++id) {
            unsigned ticks = 0;
            uint64_t expected;
            int looping;
            first = HuAudSStreamPlay((s16)id);
            if (first == MSM_STREAM_NONE) continue;
            ++verified;
            expected = ((uint64_t)(portStreamPlayer.source.channels[0].bytes / 8) * 14 * 60
                        + portStreamPlayer.source.frequency - 1) / portStreamPlayer.source.frequency;
            looping = portStreamPlayer.source.channels[0].looping;
            while (HuAudSStreamStatGet(first) == MSM_STREAM_PLAY && ticks < expected) {
                HuAudPortStreamTick();
                ++ticks;
            }
            assert(ticks == expected);
            if (looping) {
                assert(HuAudSStreamStatGet(first) == MSM_STREAM_PLAY);
                HuAudSStreamStop(first);
            }
            assert(HuAudSStreamStatGet(first) == MSM_STREAM_DONE);
            assert(!portStreamPlayer.failed);
            printf("Actual PDT stream%u: completion/loop-stop PASS %u ticks\n", id, ticks);
        }
        assert(verified == 15);
        HuAudPortStreamShutdown();
        assert(!portPdt && !portStreamOutput.stream);
        printf("Production actual PDT API PASS: %u streams\n", verified);
        return 0;
    }
    portPdt = (uint8_t *)calloc(164, 1);
    assert(portPdt);
    portPdtBytes = 164;
    portPdt[1] = 1; portPdt[3] = 1;
    portPdt[19] = 32; portPdt[23] = 36; portPdt[27] = 68; portPdt[31] = 100;
    portPdt[35] = 68; portPdt[68] = 2; portPdt[69] = 127;
    portPdt[74] = 125; portPdt[79] = 128; portPdt[83] = 2; portPdt[87] = 100;
    first = HuAudSStreamPlay(0);
    assert(first > 0 && HuAudSStreamStatGet(first) == MSM_STREAM_PLAY);
    assert(HuAudSStreamPlay(-1) == MSM_STREAM_NONE);
    assert(HuAudSStreamStatGet(first) == MSM_STREAM_PLAY);
    assert(HuAudSStreamPlay(0) == MSM_STREAM_NONE);
    assert(HuAudSStreamStatGet(first) == MSM_STREAM_PLAY);
    HuAudSStreamStop(first);
    second = HuAudSStreamPlay(0);
    assert(second > first && HuAudSStreamStatGet(first) == MSM_STREAM_DONE);
    HuAudSStreamStop(first);
    assert(HuAudSStreamStatGet(second) == MSM_STREAM_PLAY);
    HuAudSStreamFadeOut(second, 30);
    HuAudPortStreamTick();
    HuAudPortStreamTick();
    assert(HuAudSStreamStatGet(second) == MSM_STREAM_DONE);
    HuAudPortStreamShutdown();
    assert(!portPdt && !portPdtBytes && !portStreamOutput.stream);
    HuAudPortStreamShutdown();
    assert(HuAudSStreamPlay(0) == MSM_STREAM_NONE);
    assert(HuAudFXStatusGet(-1) == 0);
    assert(HuAudFXStatusGet(5) == 0);
    assert(HuAudFXPlay(0) == MSM_ERR_INITFAIL);
    puts("Production HuAud stream API regression PASS");
    return 0;
}
