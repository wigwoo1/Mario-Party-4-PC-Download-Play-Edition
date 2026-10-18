#ifndef PARTYBOARD_MUSYX_EMITTER_RUNTIME_H
#define PARTYBOARD_MUSYX_EMITTER_RUNTIME_H
#include <string.h>
#include "musyx/musyx.h"
#include "msm_container.h"
/* Keep this object at a stable address until Stop. Original continuous
 * emitters retain both parameter pointers and linked-list object addresses. */
typedef struct {
    SND_EMITTER emitter;
    SND_PARAMETER parameters[2];
    SND_PARAMETER_INFO info;
    int active;
} PartyboardMusyxEmitter;
/* One-shot parameters are consumed synchronously by the PC AddEmitter fix;
 * no persistent linked emitter object is registered for this branch. */
static inline uint32_t PartyboardMusyxEmitterStartOneShot(
        const PartyboardMsmContainerEffect *effect,SND_FVECTOR *position,
        float distance,uint16_t group,unsigned volume,unsigned reverb,unsigned chorus) {
    SND_FVECTOR direction={0};
    SND_PARAMETER parameters[2]={0};
    SND_PARAMETER_INFO info;
    if(!effect || !position || effect->group==65535 || effect->effect>=65535
            || effect->volume<0 || effect->volume>127
            || effect->emitter_component<-128 || effect->emitter_component>127
            || volume>127 || reverb>127 || chorus>127 || !(distance>0)) return 0xffffffffu;
    parameters[0].ctrl=SND_MIDICTRL_REVERB;parameters[0].paraData.value7=(u8)reverb;
    parameters[1].ctrl=SND_MIDICTRL_CHORUS;parameters[1].paraData.value7=(u8)chorus;
    info.numPara=2;info.paraArray=parameters;
    return sndAddEmitterParaEx(NULL,position,&direction,distance,
        effect->emitter_component/127.0f,SND_EMITTER_DOPPLERFX,
        (SND_FXID)effect->effect,group,(u8)(volume*(unsigned)effect->volume/127),0,NULL,&info);
}
static inline int PartyboardMusyxEmitterStart(PartyboardMusyxEmitter *runtime,
        const PartyboardMsmContainerEffect *effect,SND_FVECTOR *position,
        float distance,uint16_t group,unsigned volume,unsigned reverb,unsigned chorus) {
    SND_FVECTOR direction={0};
    if(!runtime || runtime->active || !effect || !position || effect->emitter!=1
            || effect->group==65535 || effect->effect>=65535
            || effect->volume<0 || effect->volume>127
            || effect->emitter_component<-128 || effect->emitter_component>127
            || volume>127 || reverb>127 || chorus>127 || !(distance>0)) return 0;
    memset(runtime,0,sizeof(*runtime));
    runtime->parameters[0].ctrl=SND_MIDICTRL_REVERB;
    runtime->parameters[0].paraData.value7=(u8)reverb;
    runtime->parameters[1].ctrl=SND_MIDICTRL_CHORUS;
    runtime->parameters[1].paraData.value7=(u8)chorus;
    runtime->info.numPara=2;runtime->info.paraArray=runtime->parameters;
    sndAddEmitterParaEx(&runtime->emitter,position,&direction,distance,
        effect->emitter_component/127.0f,SND_EMITTER_DOPPLERFX|SND_EMITTER_CONTINOUS,
        (SND_FXID)effect->effect,group,(u8)(volume*(unsigned)effect->volume/127),0,NULL,&runtime->info);
    runtime->active=sndCheckEmitter(&runtime->emitter)!=0;
    return runtime->active;
}
static inline int PartyboardMusyxEmitterStop(PartyboardMusyxEmitter *runtime) {
    if(!runtime || !runtime->active || !sndRemoveEmitter(&runtime->emitter)) return 0;
    memset(runtime,0,sizeof(*runtime));return 1;
}
#endif
