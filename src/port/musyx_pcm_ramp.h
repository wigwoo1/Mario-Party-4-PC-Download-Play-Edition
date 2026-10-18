#ifndef PARTYBOARD_MUSYX_PCM_RAMP_H
#define PARTYBOARD_MUSYX_PCM_RAMP_H
#include "musyx/voice.h"
#include "pcm_mix.h"
#include "musyx_volume_schedule.h"
typedef struct { uint16_t gain[9]; int16_t delta[9]; } PartyboardMusyxPcmRamp;
/* Standard studio: original changed[0] bits 1/2/4 select dry/auxA/auxB.
 * Unchanged groups run residual correction in the following block. Caller
 * retains original last-volume fields and initializes this block's flags. */
static inline void PartyboardMusyxPcmRampScheduleActive(PartyboardMusyxPcmRamp *ramp,
        uint16_t last[9],const uint16_t target[9],unsigned changed_groups,
        unsigned active_groups,uint16_t reset_flags[5]) {
    unsigned channel;
    for(channel=0;channel<9;++channel) {
        uint16_t delta=(uint16_t)ramp->delta[channel];
        if(changed_groups&(1U<<(channel/3)))
            PartyboardMusyxVolumeSchedule(&delta,&last[channel],target[channel]);
        else if(active_groups&(1U<<(channel/3)))
            PartyboardMusyxVolumeResidual(&ramp->gain[channel],&delta,&last[channel],
                    target[channel],reset_flags,(uint16_t)(1U<<channel));
        else delta=0;
        ramp->delta[channel]=(int16_t)delta;
    }
}
static inline void PartyboardMusyxPcmRampSchedule(PartyboardMusyxPcmRamp *ramp,
        uint16_t last[9],const uint16_t target[9],unsigned changed_groups,
        uint16_t reset_flags[5]) {
    PartyboardMusyxPcmRampScheduleActive(ramp,last,target,changed_groups,7,reset_flags);
}
/* MusyX 2.0.0 standard-studio aux enable bits. Preserve unrelated controls;
 * an unchanged disabled group cannot enable itself through residual targets. */
static inline uint16_t PartyboardMusyxPcmRampAuxControl200(
        const PartyboardMusyxPcmRamp *ramp,unsigned changed_groups,uint16_t control) {
    unsigned group;
    for(group=1;group<3;++group) {
        unsigned channel;
        uint16_t bit=(uint16_t)(1U<<(group-1));
        int enabled=0;
        if(!(changed_groups&(1U<<group)) && !(control&bit)) continue;
        for(channel=group*3;channel<group*3+3;++channel)
            if(ramp->gain[channel] || ramp->delta[channel]) enabled=1;
        if(enabled) control|=bit;
        else control&=(uint16_t)~bit;
    }
    return control;
}
static inline uint16_t PartyboardMusyxPcmRampStandardControl200(
        const PartyboardMusyxPcmRamp *ramp,unsigned changed_groups,uint16_t control) {
    unsigned channel,group;
    int needs_delta=(changed_groups&1U)!=0,surround=0;
    for(channel=0;channel<3;++channel) if(ramp->delta[channel]) needs_delta=1;
    for(group=1;group<3;++group) {
        int gain=0,delta=0;
        for(channel=group*3;channel<group*3+3;++channel) {
            if(ramp->gain[channel]) gain=1;
            if(ramp->delta[channel]) delta=1;
        }
        if(changed_groups&(1U<<group)) { if(delta) needs_delta=1; }
        else if((control&(1U<<(group-1))) && (gain || delta)) needs_delta=1;
    }
    control=PartyboardMusyxPcmRampAuxControl200(ramp,changed_groups,control);
    for(channel=2;channel<9;channel+=3)
        if(ramp->gain[channel] || ramp->delta[channel]) surround=1;
    if(needs_delta) control|=8;else control&=(uint16_t)~8;
    if(surround) control|=4;else control&=(uint16_t)~4;
    return control;
}
/* Consume the actual PB mixer values/deltas. Original PB order interleaves
 * dry/aux pairs; software bus order remains dry L/R/S, auxA L/R/S, auxB L/R/S.
 * Scheduler owns delta reset flags and block target/remainder correction. */
static inline void PartyboardMusyxPcmRampLoad(PartyboardMusyxPcmRamp *ramp,
        const _PBMIX *mix) {
    const uint16_t gain[9]={mix->vL,mix->vR,mix->vS,mix->vAuxAL,mix->vAuxAR,
        mix->vAuxAS,mix->vAuxBL,mix->vAuxBR,mix->vAuxBS};
    const uint16_t delta[9]={mix->vDeltaL,mix->vDeltaR,mix->vDeltaS,
        mix->vDeltaAuxAL,mix->vDeltaAuxAR,mix->vDeltaAuxAS,
        mix->vDeltaAuxBL,mix->vDeltaAuxBR,mix->vDeltaAuxBS};
    unsigned channel;
    for(channel=0;channel<9;++channel) {
        ramp->gain[channel]=gain[channel];
        ramp->delta[channel]=(int16_t)delta[channel];
    }
}
/* Original reset mask bit order equals the software bus channel order. */
static inline void PartyboardMusyxPcmRampStore(const PartyboardMusyxPcmRamp *ramp,
        _PBMIX *mix) {
    mix->vL=ramp->gain[0];mix->vR=ramp->gain[1];mix->vS=ramp->gain[2];
    mix->vAuxAL=ramp->gain[3];mix->vAuxAR=ramp->gain[4];mix->vAuxAS=ramp->gain[5];
    mix->vAuxBL=ramp->gain[6];mix->vAuxBR=ramp->gain[7];mix->vAuxBS=ramp->gain[8];
    mix->vDeltaL=(uint16_t)ramp->delta[0];mix->vDeltaR=(uint16_t)ramp->delta[1];
    mix->vDeltaS=(uint16_t)ramp->delta[2];mix->vDeltaAuxAL=(uint16_t)ramp->delta[3];
    mix->vDeltaAuxAR=(uint16_t)ramp->delta[4];mix->vDeltaAuxAS=(uint16_t)ramp->delta[5];
    mix->vDeltaAuxBL=(uint16_t)ramp->delta[6];mix->vDeltaAuxBR=(uint16_t)ramp->delta[7];
    mix->vDeltaAuxBS=(uint16_t)ramp->delta[8];
}
/* Original reset mask bit order equals the software bus channel order. */
static inline void PartyboardMusyxPcmRampReset(PartyboardMusyxPcmRamp *ramp,
        uint16_t flags) {
    unsigned channel;
    for(channel=0;channel<9;++channel)
        if(flags&(1U<<channel)) ramp->delta[channel]=0;
}
static inline void PartyboardMusyxPcmRampMix(PartyboardMusyxPcmRamp *ramp,
        int64_t *bus,const int16_t *mono,size_t frames) {
    size_t frame;
    unsigned channel;
    for(frame=0;frame<frames;++frame) for(channel=0;channel<9;++channel) {
        bus[frame*9+channel]+=(int64_t)mono[frame]*(int16_t)ramp->gain[channel];
        ramp->gain[channel]=(uint16_t)(ramp->gain[channel]+ramp->delta[channel]);
    }
}
static inline void PartyboardMusyxPcmRampMixPeriod(PartyboardMusyxPcmRamp *ramp,
        uint16_t reset_flags,int64_t *bus,const int16_t mono[32]) {
    PartyboardMusyxPcmRampReset(ramp,reset_flags);
    PartyboardMusyxPcmRampMix(ramp,bus,mono,32);
}
#endif
