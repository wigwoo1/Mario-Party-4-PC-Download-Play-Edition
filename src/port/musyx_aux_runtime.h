#ifndef PARTYBOARD_MUSYX_AUX_RUNTIME_H
#define PARTYBOARD_MUSYX_AUX_RUNTIME_H
#include "msm_container.h"
#include "musyx/musyx.h"
#include "musyx/sal.h"
#include "musyx/synth.h"
#include <string.h>

/* Owned adaptation of the existing MSM aux dispatch. All authored presets in
 * the private USA Rev1 container are high reverb or delay; standard reverb is
 * also supported by its tested SDK implementation. Unimplemented chorus must
 * reject, never silently become another effect. Caller retains the mapped
 * container and runs selection/render/shutdown on the synthesis thread.
 * Destination starts zeroed; live runtime ownership must not be copied. */
typedef struct {
    unsigned type;
    union { SND_AUX_REVERBHI high; SND_AUX_REVERBSTD standard; SND_AUX_DELAY delay; } work;
} PartyboardMusyxAuxInstance;
typedef struct {
    unsigned initialized, studio;
    int selected[2];
    PartyboardMusyxAuxInstance *instance[2];
} PartyboardMusyxAuxRuntime;

static inline SND_AUX_CALLBACK PartyboardMusyxAuxCallback(const PartyboardMusyxAuxInstance *instance) {
    if(!instance) return NULL;
    switch(instance->type) {
    case 0: return sndAuxCallbackReverbHI;
    case 1: return sndAuxCallbackReverbSTD;
    case 3: return sndAuxCallbackDelay;
    default: return NULL;
    }
}
static inline void *PartyboardMusyxAuxUser(PartyboardMusyxAuxInstance *instance) {
    return instance ? (void*)&instance->work : NULL;
}
static inline void PartyboardMusyxAuxDestroy(PartyboardMusyxAuxInstance *instance) {
    if(!instance) return;
    switch(instance->type) {
    case 0: sndAuxCallbackShutdownReverbHI(&instance->work.high); break;
    case 1: sndAuxCallbackShutdownReverbSTD(&instance->work.standard); break;
    case 3: sndAuxCallbackShutdownDelay(&instance->work.delay); break;
    }
    salFree(instance);
}
static inline float PartyboardMusyxAuxFloat(uint32_t bits) {
    float value; memcpy(&value, &bits, sizeof value); return value;
}
static inline int PartyboardMusyxAuxBuffersReady(const DSPstudioinfo *studio) {
    unsigned slot;
    for(slot = 0; slot < 3; ++slot) if(!studio->auxA[slot] || !studio->auxB[slot]) return 0;
    return 1;
}
static inline PartyboardMusyxAuxInstance *PartyboardMusyxAuxCreate(const PartyboardMsmContainerAux *preset) {
    PartyboardMusyxAuxInstance *instance;
    int ready = 0;
    unsigned channel;
    if(!preset || (preset->type != 0 && preset->type != 1 && preset->type != 3)) return NULL;
    instance = (PartyboardMusyxAuxInstance*)salMalloc(sizeof *instance);
    if(!instance) return NULL;
    memset(instance, 0, sizeof *instance); instance->type = preset->type;
    if(preset->type == 0) {
        SND_AUX_REVERBHI *r = &instance->work.high;
        r->coloration = PartyboardMusyxAuxFloat(preset->parameter[1]); r->mix = PartyboardMusyxAuxFloat(preset->parameter[2]);
        r->time = PartyboardMusyxAuxFloat(preset->parameter[3]); r->damping = PartyboardMusyxAuxFloat(preset->parameter[4]);
        r->preDelay = PartyboardMusyxAuxFloat(preset->parameter[5]); r->crosstalk = PartyboardMusyxAuxFloat(preset->parameter[6]);
        ready = sndAuxCallbackPrepareReverbHI(r);
    } else if(preset->type == 1) {
        SND_AUX_REVERBSTD *r = &instance->work.standard;
        r->coloration = PartyboardMusyxAuxFloat(preset->parameter[1]); r->mix = PartyboardMusyxAuxFloat(preset->parameter[2]);
        r->time = PartyboardMusyxAuxFloat(preset->parameter[3]); r->damping = PartyboardMusyxAuxFloat(preset->parameter[4]);
        r->preDelay = PartyboardMusyxAuxFloat(preset->parameter[5]); ready = sndAuxCallbackPrepareReverbSTD(r);
    } else {
        SND_AUX_DELAY *d = &instance->work.delay;
        for(channel = 0; channel < 3; ++channel) {
            d->delay[channel] = preset->parameter[channel]; d->feedback[channel] = preset->parameter[channel + 3];
            d->output[channel] = preset->parameter[channel + 6];
        }
        ready = sndAuxCallbackPrepareDelay(d);
    }
    if(!ready) { PartyboardMusyxAuxDestroy(instance); return NULL; }
    return instance;
}
static inline int PartyboardMusyxAuxOwnsBindings(const PartyboardMusyxAuxRuntime *runtime) {
    const DSPstudioinfo *s;
    if(!runtime || !runtime->initialized || !sndActive || runtime->studio >= salMaxStudioNum
            || runtime->studio >= 8) return 0;
    s = &dspStudio[runtime->studio];
    return s->state == 1 && s->type == SND_STUDIO_TYPE_STD && PartyboardMusyxAuxBuffersReady(s)
        && s->auxAHandler == PartyboardMusyxAuxCallback(runtime->instance[0])
        && s->auxBHandler == PartyboardMusyxAuxCallback(runtime->instance[1])
        && s->auxAUser == PartyboardMusyxAuxUser(runtime->instance[0])
        && s->auxBUser == PartyboardMusyxAuxUser(runtime->instance[1]);
}
static inline int PartyboardMusyxAuxRuntimeInit(PartyboardMusyxAuxRuntime *runtime, unsigned studio) {
    const DSPstudioinfo *s;
    if(!runtime || runtime->initialized || !sndActive || studio >= salMaxStudioNum || studio >= 8) return 0;
    s = &dspStudio[studio];
    if(s->state != 1 || s->type != SND_STUDIO_TYPE_STD || !PartyboardMusyxAuxBuffersReady(s) || s->auxAHandler || s->auxBHandler
            || s->auxAUser || s->auxBUser) return 0;
    memset(runtime, 0, sizeof *runtime); runtime->initialized = 1; runtime->studio = studio;
    runtime->selected[0] = runtime->selected[1] = -2;
    return 1;
}
/* -1 retains a channel's selection; -2 disables it. Like original MSM, a
 * successful call rebuilds both selected effects and resets their tails even
 * when a retained index is unchanged. Prepare both before detaching/freeing
 * either old owner. Callback user pointers reference stable heap instances. */
static inline int PartyboardMusyxAuxRuntimeSelect(PartyboardMusyxAuxRuntime *runtime,
        const PartyboardMsmContainer *container, int aux_a, int aux_b) {
    int selected[2] = {aux_a, aux_b};
    PartyboardMusyxAuxInstance *next[2] = {NULL, NULL}, *old[2];
    unsigned channel;
    if(!PartyboardMusyxAuxOwnsBindings(runtime)) return 0;
    for(channel = 0; channel < 2; ++channel) {
        PartyboardMsmContainerAux preset;
        if(selected[channel] == -1) selected[channel] = runtime->selected[channel];
        if(selected[channel] < -2) goto failed;
        if(selected[channel] == -2) continue;
        if(!PartyboardMsmContainerGetAux(&preset, container, (size_t)selected[channel])) goto failed;
        next[channel] = PartyboardMusyxAuxCreate(&preset);
        if(!next[channel]) goto failed;
    }
    old[0] = runtime->instance[0]; old[1] = runtime->instance[1];
    sndSetAuxProcessingCallbacks((u8)runtime->studio,
        PartyboardMusyxAuxCallback(next[0]), PartyboardMusyxAuxUser(next[0]), 0xff, 0,
        PartyboardMusyxAuxCallback(next[1]), PartyboardMusyxAuxUser(next[1]), 0xff, 0);
    for(channel = 0; channel < 2; ++channel) {
        runtime->instance[channel] = next[channel]; runtime->selected[channel] = selected[channel];
    }
    PartyboardMusyxAuxDestroy(old[1]); PartyboardMusyxAuxDestroy(old[0]);
    return 1;
failed:
    PartyboardMusyxAuxDestroy(next[1]); PartyboardMusyxAuxDestroy(next[0]);
    return 0;
}
static inline int PartyboardMusyxAuxRuntimeShutdown(PartyboardMusyxAuxRuntime *runtime) {
    if(!runtime) return 0;
    if(!runtime->initialized) return 1;
    if(!PartyboardMusyxAuxOwnsBindings(runtime)) return 0;
    sndSetAuxProcessingCallbacks((u8)runtime->studio, NULL, NULL, 0xff, 0, NULL, NULL, 0xff, 0);
    PartyboardMusyxAuxDestroy(runtime->instance[1]); PartyboardMusyxAuxDestroy(runtime->instance[0]);
    memset(runtime, 0, sizeof *runtime);
    return 1;
}
#endif
