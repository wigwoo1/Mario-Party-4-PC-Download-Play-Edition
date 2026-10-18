#ifndef PARTYBOARD_PCM_OUTPUT_H
#define PARTYBOARD_PCM_OUTPUT_H
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>

typedef struct {
    SDL_AudioStream *stream;
    unsigned frequency;
    unsigned channels;
    int audio_initialized;
} PartyboardPcmOutput;

/* Presentation gain belongs to the SDL stream: never change decoded samples,
 * authored effect/sequence volumes, positional attenuation or synthesis time.
 * SDL also applies the gain to queued audio when it is converted for playback. */
static inline int PartyboardPcmOutputSetGain(PartyboardPcmOutput *output,float gain) {
    if(!output || !isfinite(gain) || gain<0.0f || gain>1.0f) return 0;
    if(!output->stream) return 1;
    return SDL_GetAudioStreamGain(output->stream)==gain || SDL_SetAudioStreamGain(output->stream,gain);
}

static inline void PartyboardPcmOutputClose(PartyboardPcmOutput *output) {
    if (output->stream) SDL_DestroyAudioStream(output->stream);
    if (output->audio_initialized) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    output->stream = NULL;
    output->frequency = 0;
    output->channels = 0;
    output->audio_initialized = 0;
}

/* Destination must initially be zeroed. SDL performs device-rate conversion.
 * A successful open means a device stream exists, not audible acceptance. */
static inline int PartyboardPcmOutputOpenChannels(PartyboardPcmOutput *output, unsigned frequency,
                                                unsigned channels) {
    SDL_AudioSpec spec;
    if (output->stream || output->audio_initialized || frequency < 8000 || frequency > 192000
            || (channels != 2 && channels != 6)) return 0;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return 0;
    output->audio_initialized = 1;
    spec.format = SDL_AUDIO_S16;
    spec.channels = (int)channels;
    spec.freq = (int)frequency;
    output->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!output->stream || !SDL_ResumeAudioStreamDevice(output->stream)) {
        PartyboardPcmOutputClose(output);
        return 0;
    }
    output->frequency = frequency;
    output->channels = channels;
    return 1;
}
/* Six-channel source support does not prove the selected playback device
 * preserves six channels. Stereo callers retain their existing format. */
static inline int PartyboardPcmOutputOpen(PartyboardPcmOutput *output, unsigned frequency) {
    return PartyboardPcmOutputOpenChannels(output,frequency,2);
}
/* Query the live destination, not the source format. Zero means unavailable,
 * including standalone streams without a playback device. A six-channel
 * result still requires speaker mapping/audible verification. */
static inline unsigned PartyboardPcmOutputDeviceChannels(const PartyboardPcmOutput *output) {
    SDL_AudioSpec spec;
    SDL_AudioDeviceID device;
    if(!output || !output->stream) return 0;
    device=SDL_GetAudioStreamDevice(output->stream);
    if(!device || !SDL_GetAudioDeviceFormat(device,&spec,NULL) || spec.channels<=0) return 0;
    return (unsigned)spec.channels;
}

/* -1: device/error; 0: queue full; 1: copied. Real-time synthesis continues
 * when output drops; this query does not control its clock or decoder state.
 * Keep at most250ms of source PCM queued. */
static inline int PartyboardPcmOutputCapacity(const PartyboardPcmOutput *output, size_t frames) {
    int queued;
    size_t limit;
    size_t bytes_per_frame;
    if (!output->stream || (output->channels != 2 && output->channels != 6)) return -1;
    queued = SDL_GetAudioStreamQueued(output->stream);
    if (queued < 0) return -1;
    limit = output->frequency / 4;
    bytes_per_frame = output->channels * sizeof(int16_t);
    /* Include a partial queued frame conservatively in the queue budget. */
    size_t queued_frames=((size_t)queued+bytes_per_frame-1)/bytes_per_frame;
    if (queued_frames > limit || frames > limit - queued_frames) return 0;
    return 1;
}

static inline int PartyboardPcmOutputSubmit(PartyboardPcmOutput *output,
                                           const int16_t *interleaved, size_t frames) {
    int capacity = PartyboardPcmOutputCapacity(output, frames);
    if (capacity != 1) return capacity;
    if (!frames) return 1;
    if (!interleaved) return -1;
    return SDL_PutAudioStreamData(output->stream, interleaved,
        (int)(frames * output->channels * sizeof(int16_t))) ? 1 : -1;
}

/* Stereo music occupies front left/right in the six-channel SDL layout.
 * It has no authored center, LFE or rear content; keep those channels silent. */
static inline int PartyboardPcmOutputSubmitStereo(PartyboardPcmOutput *output,
                                                  const int16_t *stereo, size_t frames) {
    int16_t mapped[3200*6];
    int capacity;
    if (!output || frames>3200 || (frames && !stereo)) return -1;
    capacity=PartyboardPcmOutputCapacity(output,frames);
    if(capacity!=1) return capacity;
    if(output->channels==2 || !frames) return PartyboardPcmOutputSubmit(output,stereo,frames);
    for(size_t frame=0;frame<frames;++frame) {
        mapped[frame*6]=stereo[frame*2];
        mapped[frame*6+1]=stereo[frame*2+1];
        for(unsigned channel=2;channel<6;++channel) mapped[frame*6+channel]=0;
    }
    return PartyboardPcmOutputSubmit(output,mapped,frames);
}

/* Flush the finite stream's resampler tail before reporting it drained. */
static inline int PartyboardPcmOutputFinish(PartyboardPcmOutput *output) {
    return output->stream && SDL_FlushAudioStream(output->stream);
}
#endif
