/* The linked portable SDK is2.0.0. Native dol retains1.5.4 definitions for
 * legacy console sources; this software-backend translation unit must use
 * the library's actual structures, just like the browser and SDK fixtures. */
#undef MUSY_VERSION_MAJOR
#undef MUSY_VERSION_MINOR
#undef MUSY_VERSION_PATCH
#undef MUSY_VERSION
#define MUSY_VERSION_MAJOR 2
#define MUSY_VERSION_MINOR 0
#define MUSY_VERSION_PATCH 0
#include "game/audio.h"
#include "game/memory.h"
#include "game/msm.h"
#include "game/object.h"
#include "game/wipe.h"
#include "game/gamework_data.h"
#include "pdt_player.h"
#include "pcm_output.h"
#include "musyx_software.h"
#include "port/settings.h"
#include <stdlib.h>

static uint8_t *portPdt;
static size_t portPdtBytes;
static PartyboardPdtPlayer portStreamPlayer;
static PartyboardPcmOutput portStreamOutput;
static s32 portStreamHandle = -1;
static s32 portStreamNextHandle;
static PartyboardMusyxSoftware portSynth;
static uint8_t *portMsm;
int HuAudPortApplyOutputSettings(void) {
    const float gain=partyboard_settings_audio_gain();
    const int synth=PartyboardPcmOutputSetGain(&portSynth.output,gain);
    const int stream=PartyboardPcmOutputSetGain(&portStreamOutput,gain);
    return synth && stream;
}
/* Read-only diagnostics shared with the browser platform. Counter4 samples
 * the final processed block of each video tick, not every output sample.
 * Counters saturate at INT32_MAX; no sample/sequence state is exposed. */
int HuAudPortSynthMetric(int metric) {
    uint64_t value;
    switch(metric) {
    case 0:return portSynth.active;
    case 1:value=portSynth.driver.completed_blocks;break;
    case 2:value=portSynth.effect_starts;break;
    case 3:value=portSynth.song_starts;break;
    case 4:value=portSynth.nonzero_samples;break;
    case 5:value=portSynth.positional_starts;break;
    case 6:value=portSynth.periodic_calls;break;
    case 7:value=portSynth.driver.dropped_blocks;break;
    case 8:value=portSynth.driver.device_errors;break;
    case 9:return (int)portSynth.output.channels;
    case 10:return (int)PartyboardPcmOutputDeviceChannels(&portSynth.output);
    case 11:return (int)portSynth.info.voices;
    case 12:return portSynth.output.stream?SDL_GetAudioStreamQueued(portSynth.output.stream):-1;
    case 13:return 1; /* Original mono-surround bus routes to both rear speakers. */
    case 14:return portSynth.output.stream?(int)(SDL_GetAudioStreamGain(portSynth.output.stream)*1000.0f+0.5f):-1;
    case 15:return portStreamOutput.stream?(int)(SDL_GetAudioStreamGain(portStreamOutput.stream)*1000.0f+0.5f):-1;
    default:return -1;
    }
    return value>INT32_MAX?INT32_MAX:(int)value;
}
static int portControlProbe, portProbePositional, portProbeEffect, portProbeSong;
static void PortAudioControlsProbeTick(void) {
    if(!portControlProbe) return;
    uint64_t tick=portSynth.driver.ticks;
    if(tick==1) {
        Vec position={14,3,-7},origin={1,0,0},heading={0,0,-1};
        HuAudFXEmiterUpDate(portProbePositional,&position);HuAudFXListnerUpdate(&origin,&heading);
        HuAudFXPanning(portProbeEffect,32);
        int pitch=HuAudFXPitchSet(portProbeEffect,-100),volume=HuAudFXVolSet(portProbeEffect,64);
        OSReport("Audio controls probe: movement=1 pitch=%d volume=%d\n",pitch,volume);
    }
    if(tick==2) HuAudFXPauseAll(1);
    if(tick==8) HuAudFXPauseAll(0);
    if(tick==9) HuAudFXFadeOut(portProbeEffect,90);
    if(tick==12) HuAudSeqPause(portProbeSong,1,60);
    if(tick==18) HuAudSeqPause(portProbeSong,0,45);
    if(tick==21) HuAudSeqFadeOut(portProbeSong,90);
    if(tick==30) {
        int handle=CharFXPlay(0,0);CharFXStop(0,0);
        OSReport("Audio controls probe: characterHandle=%d stoppedStatus=%d\n",handle,HuAudFXStatusGet(handle));
    }
    if(tick==32) {Vec p={10,2,-5};portProbePositional=HuAudFXEmiterPlay(54,&p);}
    if(tick==33) HuAudFXStop(portProbePositional);
    if(tick==36) {
        HuAudFXListnerKill();HuAudFXAllStop();HuAudSeqAllStop();
        OSReport("Audio controls probe complete: listener=%d positionalStarts=%llu periodicCalls=%llu\n",
            portSynth.listener_active,(unsigned long long)portSynth.positional_starts,(unsigned long long)portSynth.periodic_calls);
        portControlProbe=0;
    }
}
static int PortSynthEffectDescriptor(int handle,PartyboardMsmContainerEffect *descriptor) {
    if(!portSynth.active) return 0;
    for(unsigned i=0;i<64;++i) if(portSynth.effects[i].handle==handle)
        return PartyboardMsmContainerGetEffect(descriptor,&portSynth.container,(unsigned)portSynth.effects[i].effect);
    return 0;
}
static void PortSynthShutdown(void) {
    if(portSynth.active) {
        OSReport("MusyX software shutdown: effects=%llu songs=%llu blocks=%llu nonzeroLastBlocks=%llu positional=%llu periodic=%llu\n",
            (unsigned long long)portSynth.effect_starts,(unsigned long long)portSynth.song_starts,
            (unsigned long long)portSynth.driver.completed_blocks,(unsigned long long)portSynth.nonzero_samples,
            (unsigned long long)portSynth.positional_starts,(unsigned long long)portSynth.periodic_calls);
        if(!PartyboardMusyxSoftwareShutdown(&portSynth)) { OSReport("MusyX shutdown ownership failure\n"); return; }
    }
    free(portMsm);portMsm=NULL;
}
static int PortSynthInit(void) {
    DVDFileInfo file; s32 result; u32 bytes;
    if(!DVDOpen("/sound/mpgcsnd.msm",&file)) { OSReport("MusyX container open failed\n");return 0; }
    bytes=file.length;
    if(bytes<96 || bytes>64*1024*1024) { DVDClose(&file);return 0; }
    portMsm=malloc(bytes);
    if(!portMsm) { DVDClose(&file);return 0; }
    result=DVDReadPrio(&file,portMsm,(s32)bytes,0,2);DVDClose(&file);
    if(result!=(s32)bytes) { OSReport("MusyX container read failed: result=%d expected=%u\n",result,bytes);PortSynthShutdown();return 0; }
    if(!PartyboardMusyxSoftwareInit(&portSynth,portMsm,bytes)) { OSReport("MusyX owner initialization failed: stage=%d SDKactive=%d bytes=%u\n",portSynth.last_error,sndActive,bytes);PortSynthShutdown();return 0; }
    unsigned channels=partyboard_settings_audio_channels();
    PartyboardMusyxSoftwareSetChannels(&portSynth,channels);
    if(PartyboardPcmOutputOpenChannels(&portSynth.output,32000,channels)) {
        HuAudPortApplyOutputSettings();
        PartyboardMusyxPcmBridgeSetOutput(&portSynth.bridge,&portSynth.output);
    }
    else OSReport("MusyX output unavailable: %s\n",SDL_GetError());
    OSReport("MusyX software ready: voices=%u groups=%u provider=%s\n",portSynth.info.voices,portSynth.loaded,PARTYBOARD_SRC_COEFFICIENT_PROVIDER);
    return 1;
}
unsigned HuAudPortStreamSourceChannels(void) {
    return portStreamOutput.stream ? portStreamOutput.channels : 0;
}
unsigned HuAudPortStreamDeviceChannels(void) {
    return PartyboardPcmOutputDeviceChannels(&portStreamOutput);
}
int HuAudPortStreamQueuedBytes(void) {
    return portStreamOutput.stream ? SDL_GetAudioStreamQueued(portStreamOutput.stream) : -1;
}

void HuAudPortStreamShutdown(void) {
    portControlProbe=0;
    PortSynthShutdown();
    PartyboardPcmOutputClose(&portStreamOutput);
    portStreamPlayer.active = 0;
    portStreamPlayer.failed = 0;
    free(portPdt);
    portPdt = NULL;
    portPdtBytes = 0;
    portStreamHandle = -1;
}

static int PortPdtLoad(void) {
    DVDFileInfo file;
    uint8_t *data;
    s32 result;
    u32 length;
    if (portPdt) return 1;
    if (!DVDOpen("/sound/mpgcstr.pdt", &file)) return 0;
    if (file.length < 32 || file.length > 64 * 1024 * 1024) { DVDClose(&file); return 0; }
    length = file.length;
    data = (uint8_t *)malloc(length);
    if (!data) { DVDClose(&file); return 0; }
    result = DVDReadPrio(&file, data, (s32)length, 0, 2);
    DVDClose(&file);
    if (result != (s32)length) { free(data); return 0; }
    portPdt = data;
    portPdtBytes = length;
    return 1;
}

void HuAudPortStreamTick(void) {
    int16_t pcm[6400];
    size_t produced;
    HuAudPortApplyOutputSettings();
    if(portSynth.active) {
        PortAudioControlsProbeTick();
        if(!PartyboardMusyxSoftwareTick(&portSynth)) {
            OSReport("MusyX software render failed: voice=%u\n",portSynth.scheduler.failed_voice);PortSynthShutdown();
        } else {
            if(portSynth.driver.device_errors && portSynth.output.stream) {
                OSReport("MusyX device output failed: %s\n",SDL_GetError());
                PartyboardMusyxPcmBridgeSetOutput(&portSynth.bridge,NULL);PartyboardPcmOutputClose(&portSynth.output);
            }
            if(getenv("PARTYBOARD_AUDIO_DIAGNOSTICS") && portSynth.driver.ticks%60==0)
                OSReport("MusyX software progress: blocks=%llu effects=%llu songs=%llu nonzeroLastBlocks=%llu\n",
                    (unsigned long long)portSynth.driver.completed_blocks,(unsigned long long)portSynth.effect_starts,
                    (unsigned long long)portSynth.song_starts,(unsigned long long)portSynth.nonzero_samples);
        }
    }
    if (!portStreamPlayer.active) {
        if (portStreamOutput.stream && SDL_GetAudioStreamQueued(portStreamOutput.stream) <= 0)
            PartyboardPcmOutputClose(&portStreamOutput);
        return;
    }
    produced = PartyboardPdtPlayerTick(&portStreamPlayer, 60, pcm, 3200);
    if (produced && portStreamOutput.stream) {
        int result = PartyboardPcmOutputSubmitStereo(&portStreamOutput, pcm, produced);
        if (result < 0) {
            OSReport("Stream output failed: %s\n", SDL_GetError());
            PartyboardPcmOutputClose(&portStreamOutput);
        }
        /* Local backpressure drops audio rather than changing game timing. */
    }
    if (!portStreamPlayer.active && portStreamOutput.stream) PartyboardPcmOutputFinish(&portStreamOutput);
}

static int HuSePlay(int seId, MSM_SEPARAM *param);

extern s16 omSysExitReq;

s32 charVoiceGroupStat[8];
static s32 sndFXBuf[64][2];

static s16 Hu3DAudVol;
static s16 sndGroupBak;
static s32 auxANoBak;
static s32 auxBNoBak;
static s8 HuAuxAVol;
static s8 HuAuxBVol;
float Snd3DBackSurDisOffset;
float Snd3DFrontSurDisOffset;
float Snd3DStartDisOffset;
float Snd3DSpeedOffset;
float Snd3DDistOffset;
s32 musicOffF;
u8 fadeStat;

static char *lbl_8012E9AC[] = {
    "sound/MPNGC02.son",
    "sound/MPNGC16.son",
    ""
};


void HuAudInit(void)
{
    s16 i;

    HuAudPortStreamShutdown();

    if(!PortSynthInit()) OSReport("MusyX software initialization failed; streamed music remains available\n");
    for (i = 0; i < 64; i++) {
        sndFXBuf[i][0] = -1;
    }
    for (i = 0; i < 8; i++) {
        charVoiceGroupStat[i] = 0;
    }
    sndGroupBak = -1;
    auxANoBak = auxBNoBak = -1;
    fadeStat = 0;
    musicOffF = 0;
    HuAuxAVol=HuAuxBVol=-1;
    /* Explicit muted integration probes call the same public game APIs. */
    if(getenv("PARTYBOARD_AUDIO_CONTROLS_PROBE") && portSynth.active) {
        Vec origin={0},heading={0,0,-1},position={10,2,-5};
        HuAudFXListnerSetEX(&origin,&heading,100,343,0,25,25);
        portProbePositional=HuAudFXEmiterPlay(54,&position);portProbeEffect=HuAudFXPlay(2200);portProbeSong=HuAudSeqPlay(0);
        portControlProbe=1;
        OSReport("Audio controls probe start: positional=%d effect=%d song=%d listener=%d\n",
            portProbePositional,portProbeEffect,portProbeSong,portSynth.listener_active);
    } else if(getenv("PARTYBOARD_AUDIO_PROBE") && portSynth.active) {
        OSReport("MusyX game API probe: effect=%d song=%d\n",HuAudFXPlay(0),HuAudSeqPlay(0));
    }
}

s32 HuAudStreamPlay(char *name, BOOL flag) {
    return 0;
}

void HuAudStreamVolSet(s16 vol) {
    // AISetStreamVolLeft(vol);
    // AISetStreamVolRight(vol);
    Hu3DAudVol = vol;
}

void HuAudStreamPauseOn(void) {
    // AISetStreamPlayState(0);
}

void HuAudStreamPauseOff(void) {
    // AISetStreamPlayState(1);
}

void HuAudStreamFadeOut(s32 arg0) {
}

void HuAudAllStop(void) {
    HuAudSeqAllStop();
    HuAudFXAllStop();
    HuAudSStreamAllStop();
}

void HuAudFadeOut(s32 speed) {
    HuAudFXAllStop();
    HuAudSeqAllFadeOut(speed);
    HuAudSStreamAllFadeOut(speed);
}

int HuAudFXPlay(int seId)
{
    return HuSePlay(seId, NULL);
}

int HuAudFXPlayVol(int seId, s16 vol) {
    if (omSysExitReq != 0) {
        return 0;
    }
    return HuAudFXPlayVolPan(seId, vol, MSM_PAN_CENTER);
}

int HuAudFXPlayVolPan(int seId, s16 vol, s16 pan)
{
    MSM_SEPARAM seParam;

    if (omSysExitReq != 0) {
        return 0;
    }
    seParam.flag = MSM_SEPARAM_VOL|MSM_SEPARAM_PAN;
    seParam.vol = vol;
    seParam.pan = pan;
    return HuSePlay(seId, &seParam);
}

void HuAudFXStop(int seNo) {
    PartyboardMusyxSoftwareStopEffect(&portSynth,seNo,0);
}

void HuAudFXAllStop(void) {
    PartyboardMusyxSoftwareStopEffects(&portSynth,-1);
}

void HuAudFXFadeOut(int seNo, s32 speed) {
    PartyboardMusyxSoftwareStopEffect(&portSynth,seNo,speed);
}

void HuAudFXPanning(int seNo, s16 pan) {
    MSM_SEPARAM seParam;

    if (omSysExitReq == 0) {
        seParam.flag = MSM_SEPARAM_PAN;
        seParam.pan = pan;
        uint32_t voice=PartyboardMusyxSoftwareVoice(&portSynth,seNo);
        PartyboardMsmContainerEffect descriptor;
        PartyboardMusyxSoftwareEffectSlot *effect=PartyboardMusyxSoftwareFindEffect(&portSynth,seNo);
        if(effect && !effect->positional && voice!=UINT32_MAX && pan>=0 && pan<=127 && PortSynthEffectDescriptor(seNo,&descriptor)) {
            int value=pan+descriptor.pan-64;if(value<0)value=0;if(value>127)value=127;
            sndFXPanning(voice,(u8)value);
        }
    }
}

void HuAudFXListnerSet(Vec *pos, Vec *heading, float sndDist, float sndSpeed)
{
    if(omSysExitReq) {
      return;
    }
    HuAudFXListnerSetEX(pos, heading,
        sndDist + Snd3DDistOffset,
        sndSpeed + Snd3DSpeedOffset,
        Snd3DStartDisOffset,
        Snd3DFrontSurDisOffset + (0.25 * sndDist + Snd3DStartDisOffset),
        Snd3DBackSurDisOffset + (0.25 * sndDist + Snd3DStartDisOffset));
}

void HuAudFXListnerSetEX(Vec *pos, Vec *heading, float sndDist, float sndSpeed, float startDis, float frontSurDis, float backSurDis)
{
    MSM_SELISTENER listener;
    if(omSysExitReq) {
      return;
    }
    listener.flag = MSM_LISTENER_STARTDIS|MSM_LISTENER_FRONTSURDIS|MSM_LISTENER_BACKSURDIS;
    listener.startDis = startDis + Snd3DStartDisOffset;
    listener.frontSurDis = frontSurDis + Snd3DFrontSurDisOffset;
    listener.backSurDis = backSurDis + Snd3DBackSurDisOffset;
    if(pos && heading) {
        SND_FVECTOR p={pos->x,pos->y,pos->z},h={heading->x,heading->y,heading->z};
        PartyboardMusyxSoftwareListener(&portSynth,&p,&h,sndDist+Snd3DDistOffset,sndSpeed+Snd3DSpeedOffset,
            listener.startDis,listener.frontSurDis,listener.backSurDis);
    }
    OSReport("//////////////////////////////////\n");
    OSReport("sndDist %f\n", sndDist);
    OSReport("sndSpeed %f\n", sndSpeed);
    OSReport("startDis %f\n", listener.startDis);
    OSReport("frontSurDis %f\n", listener.frontSurDis);
    OSReport("backSurDis %f\n", listener.backSurDis);
    OSReport("//////////////////////////////////\n");
}

void HuAudFXListnerUpdate(Vec *pos, Vec *heading)
{
    if (omSysExitReq == 0) {
        if(pos && heading) {
            SND_FVECTOR p={pos->x,pos->y,pos->z},h={heading->x,heading->y,heading->z};
            PartyboardMusyxSoftwareUpdateListener(&portSynth,&p,&h);
        }
    }
}

int HuAudFXEmiterPlay(int seId, Vec *pos)
{
    MSM_SEPARAM seParam;
    if(omSysExitReq || !pos) {
      return 0;
    }
    seParam.flag = MSM_SEPARAM_POS;
    seParam.pos.x = pos->x;
    seParam.pos.y = pos->y;
    seParam.pos.z = pos->z;
    return HuSePlay(seId, &seParam);
}

void HuAudFXEmiterUpDate(int seNo, Vec *pos)
{
    MSM_SEPARAM param;
    if(omSysExitReq || !pos) {
        return;
    }
    param.flag = MSM_SEPARAM_POS;
    param.pos.x = pos->x;
    param.pos.y = pos->y;
    param.pos.z = pos->z;
    SND_FVECTOR p={pos->x,pos->y,pos->z};
    PartyboardMusyxSoftwareMoveEffect(&portSynth,seNo,&p);
}

void HuAudFXListnerKill(void) {
    PartyboardMusyxSoftwareRemoveListener(&portSynth);
}

void HuAudFXPauseAll(s32 pause) {
    if(portSynth.active) for(unsigned i=0;i<64;++i)
        PartyboardMusyxSoftwarePauseEffect(&portSynth,portSynth.effects[i].handle,pause,100);
}

s32 HuAudFXStatusGet(int seNo) {
    PartyboardMusyxSoftwareEffectSlot *effect=PartyboardMusyxSoftwareFindEffect(&portSynth,seNo);
    return effect ? effect->status : MSM_SE_DONE;
}

s32 HuAudFXPitchSet(int seNo, s16 pitch)
{
    MSM_SEPARAM param;
    if(omSysExitReq) {
        return 0;
    }
    param.flag = MSM_SEPARAM_PITCH;
    param.pitch = pitch;
    uint32_t voice=PartyboardMusyxSoftwareVoice(&portSynth,seNo);
    PartyboardMusyxSoftwareEffectSlot *effect=PartyboardMusyxSoftwareFindEffect(&portSynth,seNo);
    if(effect && effect->positional) return 0; /* Original MSM leaves3D pitch to the emitter. */
    if(voice==UINT32_MAX) return MSM_ERR_INVALIDID;
    PartyboardMsmContainerEffect descriptor;
    if(!PortSynthEffectDescriptor(seNo,&descriptor)) return MSM_ERR_INVALIDID;
    int value=pitch+descriptor.pitch_bend+8192;if(value<0)value=0;if(value>16383)value=16383;
    sndFXPitchBend(voice,(u16)value);return 0;
}

s32 HuAudFXVolSet(int seNo, s16 vol)
{
    MSM_SEPARAM param;

    if(omSysExitReq) {
        return 0;
    }
    param.flag = MSM_SEPARAM_VOL;
    param.vol = vol;
    return vol>=0 && PartyboardMusyxSoftwareVolume(&portSynth,seNo,(unsigned)vol) ? 0 : MSM_ERR_INVALIDID;
}

s32 HuAudSeqPlay(s16 musId) {
    s32 channel = 0;

    if (musicOffF != 0 || omSysExitReq != 0) {
        return 0;
    }
    return musId<0 ? MSM_ERR_INVALIDID : PartyboardMusyxSoftwareSong(&portSynth,(unsigned)musId,0);
}

void HuAudSeqStop(s32 musNo) {
    if (musicOffF != 0 || omSysExitReq != 0) {
        return;
    }
    if(musNo>=0) PartyboardMusyxSoftwareStopSong(&portSynth,(unsigned)musNo);
}

void HuAudSeqFadeOut(s32 musNo, s32 speed) {
    if (musicOffF == 0) {
        if(musNo>=0) PartyboardMusyxSoftwareFadeSong(&portSynth,(unsigned)musNo,speed);
    }
}

void HuAudSeqAllFadeOut(s32 speed) {
    s16 i;

    for (i = 0; i < 4; i++) {
        HuAudSeqFadeOut(i,speed);
    }
}

void HuAudSeqAllStop(void) {
    for(unsigned i=0;i<4;++i) PartyboardMusyxSoftwareStopSong(&portSynth,i);
}

void HuAudSeqPauseAll(s32 pause) {
    for(s32 i=0;i<4;++i) HuAudSeqPause(i,pause,100);
}

void HuAudSeqPause(s32 musNo, s32 pause, s32 speed) {
    if (musicOffF != 0 || omSysExitReq != 0) {
        return;
    }
    if(musNo>=0) PartyboardMusyxSoftwarePauseSong(&portSynth,(unsigned)musNo,pause,speed);
}

s32 HuAudSeqMidiCtrlGet(s32 musNo, s8 channel, s8 ctrl) {
    if (musicOffF != 0 || omSysExitReq != 0) {
        return 0;
    }
    return portSynth.active && musNo>=0 && musNo<4 && channel>=0 && channel<16 && ctrl>=0
        && portSynth.songs[musNo].arrangement.data ? sndSeqGetMidiCtrl(portSynth.songs[musNo].sequence,(u8)channel,(u8)ctrl) : 0;
}

s32 HuAudSStreamPlay(s16 streamId) {
    if (musicOffF != 0 || omSysExitReq != 0) {
        return MSM_STREAM_NONE;
    }
    /* All actual PDT packs are stereo and occupy both original stream slots.
     * Keep the playing voice when the caller requests another without stop. */
    if (portStreamPlayer.active) return MSM_STREAM_NONE;
    if (streamId < 0 || !PortPdtLoad()
        || !PartyboardPdtPlayerStart(&portStreamPlayer, portPdt, portPdtBytes, (unsigned)streamId)) return MSM_STREAM_NONE;
    PartyboardPcmOutputClose(&portStreamOutput);
    unsigned outputChannels=partyboard_settings_audio_channels();
    if (!PartyboardPcmOutputOpenChannels(&portStreamOutput, portStreamPlayer.source.frequency,outputChannels))
        OSReport("Stream device unavailable: %s\n", SDL_GetError());
    HuAudPortApplyOutputSettings();
    if (portStreamNextHandle == 0x7fffffff) portStreamNextHandle = 0;
    portStreamHandle = ++portStreamNextHandle;
    return portStreamHandle;
}

void HuAudSStreamStop(s32 seNo) {
    if (seNo == portStreamHandle) {
        portStreamPlayer.active = 0;
        PartyboardPcmOutputClose(&portStreamOutput);
    }
}

void HuAudSStreamFadeOut(s32 seNo, s32 speed) {
    if (seNo != portStreamHandle) return;
    if (speed < 15) { HuAudSStreamStop(seNo); return; }
    PartyboardPdtPlayerFade(&portStreamPlayer, (unsigned)(((uint64_t)(speed / 15) * 15 * 60 + 999) / 1000));
}

void HuAudSStreamAllFadeOut(s32 speed) {
    HuAudSStreamFadeOut(portStreamHandle, speed);
}

void HuAudSStreamAllStop(void) {
    HuAudSStreamStop(portStreamHandle);
}

s32 HuAudSStreamStatGet(s32 seNo) {
    if (seNo != portStreamHandle) return MSM_STREAM_DONE;
    if (portStreamPlayer.failed) return MSM_STREAM_DVDERROR;
    return portStreamPlayer.active ? MSM_STREAM_PLAY : MSM_STREAM_DONE;
}

SHARED_SYM SNDGRPTBL sndGrpTable[] = {
    { DLL_bootdll, MSM_GRPSET_NONE, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_instdll, MSM_GRPSET_NONE, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m401dll, MSM_GRPSET_MG401, MSM_AUXA_DEFAULT, 2, 64, 64 },
    { DLL_m402dll, MSM_GRPSET_MG402, MSM_AUXA_DEFAULT, 3, 48, 32 },
    { DLL_m403dll, MSM_GRPSET_MG403, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m404dll, MSM_GRPSET_MG404, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m405dll, MSM_GRPSET_MG405, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, 64, 32 },
    { DLL_m406dll, MSM_GRPSET_MG406, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m407dll, MSM_GRPSET_MG407, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m408dll, MSM_GRPSET_MG408, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m409dll, MSM_GRPSET_MG409, MSM_AUXA_DEFAULT, 4, -1, -1 },
    { DLL_m410dll, MSM_GRPSET_MG410, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m411dll, MSM_GRPSET_MG411, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m412dll, MSM_GRPSET_MG412, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m413dll, MSM_GRPSET_MG413, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m414dll, MSM_GRPSET_MG414, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m415dll, MSM_GRPSET_MG415, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m416dll, MSM_GRPSET_MG416, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m417dll, MSM_GRPSET_MG417, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m418dll, MSM_GRPSET_MG418, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, 64, 64 },
    { DLL_m419dll, MSM_GRPSET_MG419, MSM_AUXA_DEFAULT, 6, -1, -1 },
    { DLL_m420dll, MSM_GRPSET_MG420, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m421dll, MSM_GRPSET_MG421, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m422dll, MSM_GRPSET_MG422, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m423dll, MSM_GRPSET_MG423, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m424dll, MSM_GRPSET_MG424, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m425dll, MSM_GRPSET_MG425, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m426dll, MSM_GRPSET_MG426, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m427dll, MSM_GRPSET_MG427, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, 64, 72 },
    { DLL_m428dll, MSM_GRPSET_MG428, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m429dll, MSM_GRPSET_MG429, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m430dll, MSM_GRPSET_MG430, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m431dll, MSM_GRPSET_MG431, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m432dll, MSM_GRPSET_MG432, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, 48, 32 },
    { DLL_m433dll, MSM_GRPSET_MG433, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m434dll, MSM_GRPSET_MG434, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m435dll, MSM_GRPSET_MG435, MSM_AUXA_DEFAULT, 9, 32, 64 },
    { DLL_m436dll, MSM_GRPSET_MG436, MSM_AUXA_DEFAULT, 10, 32, 64 },
    { DLL_m437dll, MSM_GRPSET_MG437, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, 32, 64 },
    { DLL_m438dll, MSM_GRPSET_MG438, MSM_AUXA_DEFAULT, 11, -1, -1 },
    { DLL_m439dll, MSM_GRPSET_MG439, MSM_AUXA_DEFAULT, 12, 48, 32 },
    { DLL_m440dll, MSM_GRPSET_MG440, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m441dll, MSM_GRPSET_MG441, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m442dll, MSM_GRPSET_MG442, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m443dll, MSM_GRPSET_MG443, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m444dll, MSM_GRPSET_MG444, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m445dll, MSM_GRPSET_MG445, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m446dll, MSM_GRPSET_MG446, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m447dll, MSM_GRPSET_MG447, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m448dll, MSM_GRPSET_MG448, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m449dll, MSM_GRPSET_MG449, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m450dll, MSM_GRPSET_MG450, MSM_AUXA_DEFAULT, 13, 64, 64 },
    { DLL_m451dll, MSM_GRPSET_MG451, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m453dll, MSM_GRPSET_MG453, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m455dll, MSM_GRPSET_MG455, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m456dll, MSM_GRPSET_MG456, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m457dll, MSM_GRPSET_MG457, MSM_AUXA_DEFAULT, 14, 64, 32 },
    { DLL_m458dll, MSM_GRPSET_MG458, MSM_AUXA_DEFAULT, 15, 64, 32 },
    { DLL_m459dll, MSM_GRPSET_MG459, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m460dll, MSM_GRPSET_MG460, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m461dll, MSM_GRPSET_MG461, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m462dll, MSM_GRPSET_MG462, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_m463dll, MSM_GRPSET_MG463, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_mentdll, MSM_GRPSET_NONE, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_mgmodedll, MSM_GRPSET_CMN, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_modeseldll, MSM_GRPSET_INIT, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_modeltestdll, MSM_GRPSET_MG401, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_msetupdll, MSM_GRPSET_INIT, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_mstorydll, MSM_GRPSET_MSTORY, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_mstory2dll, MSM_GRPSET_MSTORY, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_mstory3dll, MSM_GRPSET_MSTORY3, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_nisdll, MSM_GRPSET_BD1, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_option, MSM_GRPSET_NONE, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_present, MSM_GRPSET_NONE, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_resultdll, MSM_GRPSET_NONE, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_safdll, MSM_GRPSET_INIT, MSM_AUXA_DEFAULT, 2, 127, 127 },
    { DLL_selmenuDLL, MSM_GRPSET_INIT, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_w01dll, MSM_GRPSET_BD1, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_w02dll, MSM_GRPSET_BD2, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_w03dll, MSM_GRPSET_BD3, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_w04dll, MSM_GRPSET_BD4, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_w05dll, MSM_GRPSET_BD5, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_w06dll, MSM_GRPSET_BD6, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_w10dll, MSM_GRPSET_BD10, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_w20dll, MSM_GRPSET_BD20, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_w21dll, MSM_GRPSET_BD21, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_mpexdll, 4, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_ztardll, MSM_GRPSET_NONE, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_e3setupDLL, MSM_GRPSET_INIT, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_staffdll, MSM_GRPSET_NONE, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, -1, -1 },
    { DLL_NONE, MSM_GRPSET_NONE, MSM_AUXA_DEFAULT, MSM_AUXB_DEFAULT, 0, 0 }
};

void HuAudDllSndGrpSet(u16 ovl) {
    SNDGRPTBL *sndGrp;
    s16 grpSet;

    sndGrp = sndGrpTable;
    while (1) {
        if (sndGrp->ovl == ovl) {
            grpSet = sndGrp->grpSet;
            break;
        }
        if (sndGrp->ovl == DLL_NONE) {
            grpSet = 0x12;
            break;
        }
        sndGrp++;
    }
    if (grpSet != -1) {
        OSReport("SOUND ##########################\n");
        HuAudSndGrpSetSet(grpSet);
        if (sndGrp->auxANo != auxANoBak || sndGrp->auxBNo != auxBNoBak) {
            if(portSynth.active) PartyboardMusyxAuxRuntimeSelect(&portSynth.aux,&portSynth.container,sndGrp->auxANo,sndGrp->auxBNo);
            OSReport("Change AUX %d,%d\n", sndGrp->auxANo, sndGrp->auxBNo);
            auxANoBak = sndGrp->auxANo;
            auxBNoBak = sndGrp->auxBNo;
            HuPrcVSleep();
        }
        HuAudAUXVolSet(sndGrp->auxAVol, sndGrp->auxBVol);
        OSReport("##########################\n");
    }
}

void HuAudSndGrpSetSet(s16 dataSize) {
    if(dataSize>=0 && portSynth.active && !PartyboardMusyxSoftwareScene(&portSynth,(unsigned)dataSize))
        OSReport("MusyX scene group unavailable: %d\n",dataSize);
}

void HuAudSndGrpSet(s16 grpId) {
    void *buf;

    if(grpId>=0 && portSynth.active) PartyboardMusyxSoftwareLoadIndex(&portSynth,(unsigned)grpId);
}

void HuAudSndCommonGrpSet(s16 grpId, s32 groupCheck) {
    s16 err;
    OSTick osTick;
    void *buf;
    s16 i;

    for (i = 0; i < 8; i++) {
        charVoiceGroupStat[i] = 0;
    }
    // msmMusStopAll(1, 0);
    // msmSeStopAll(1, 0);
    osTick = OSGetTick();
    // while ((msmMusGetNumPlay(1) != 0 || msmSeGetNumPlay(1) != 0)
    //     && OSTicksToMilliseconds(OSGetTick() - osTick) < 500);
    OSReport("CommonGrpSet %d\n", grpId);
    // if (groupCheck != 0) {
    //     // err = msmSysDelGroupBase(0);
    //     if (err < 0) {
    //         OSReport("Del Group Error %d\n", err);
    //     }
    // }
    // buf = HuMemDirectMalloc(HEAP_DATA, msmSysGetSampSize(grpId));
    // msmSysLoadGroupBase(grpId, buf);
    // HuMemDirectFree(buf);
    sndGroupBak = -1;
}

void HuAudAUXSet(s32 auxA, s32 auxB) {
    if (auxA == -1) {
        auxA = 0;
    }
    if (auxB == -1) {
        auxB = 1;
    }
    if(portSynth.active) PartyboardMusyxAuxRuntimeSelect(&portSynth.aux,&portSynth.container,auxA,auxB);
}

void HuAudAUXVolSet(s8 auxA, s8 auxB) {
    HuAuxAVol = auxA;
    HuAuxBVol = auxB;
}

void HuAudSndCharGrpSet(s16 ovl) {
    SNDGRPTBL *sndGrp;
    OSTick osTick;
    s16 numNotChars;
    s16 grpId;
    s16 temp_r25;
    s16 character;
    
    void *buf;
    s16 i;

    if (ovl != DLL_NONE) {
        sndGrp = sndGrpTable;
        while (1) {
            if (sndGrp->ovl == ovl && sndGrp->grpSet == -1) {
                return;
            }
            if (sndGrp->ovl == DLL_NONE) {
                break;
            }
            sndGrp++;
        }
    }
    for (i = numNotChars = 0; i < 4; i++) {
        character = GWPlayerCfg[i].character;
        if (character < 0 || character >= 8 || character == 0xFF || charVoiceGroupStat[character] != 0) {
            numNotChars++;
        }
    }
    if (numNotChars < 4) {
        for (i = 0; i < 8; i++) {
            charVoiceGroupStat[i] = 0;
        }
        // msmMusStopAll(1, 0);
        // msmSeStopAll(1, 0);
        osTick = OSGetTick();
        // while ((msmMusGetNumPlay(1) != 0 || msmSeGetNumPlay(1) != 0)
        //     && OSTicksToMilliseconds(OSGetTick() - osTick) < 500);
        OSReport("############CharGrpSet\n");
        // temp_r25 = msmSysDelGroupBase(0);
        // if (temp_r25 < 0) {
        //     OSReport("Del Group Error %d\n", temp_r25);
        // } else {
        //     OSReport("Del Group OK\n");
        // }
        for (i = 0; i < 4; i++) {
            character = GWPlayerCfg[i].character;
            if (character >= 0 && character < 8 && character != 0xFF) {
                charVoiceGroupStat[character] = 1;
                grpId = character + 10;
                // buf = HuMemDirectMalloc(HEAP_DATA, msmSysGetSampSize(grpId));
                #if VERSION_NTSC
                // msmSysLoadGroupBase(grpId, buf);
                #else
                temp_r25 = msmSysLoadGroupBase(grpId, buf);
                #endif
                // HuMemDirectFree(buf);
            }
        }
        sndGroupBak = -1;
    }
}

s32 PlayerFXPlay(s16 player, s16 seId) {
    s16 charNo = GWPlayerCfg[player].character;

    return CharFXPlay(charNo, seId);
}

s32 PlayerFXPlayPos(s16 player, s16 seId, Vec *pos) {
    s16 charNo = GWPlayerCfg[player].character;

    return CharFXPlayPos(charNo, seId, pos);
}

void PlayerFXStop(s16 player, s16 seId) {
    s16 charNo = GWPlayerCfg[player].character;

    CharFXStop(charNo, seId);
}

s32 CharFXPlay(s16 charNo, s16 seId)
{
    MSM_SEPARAM param={0};
    if(charNo<0 || charNo>=8 || seId<0 || seId>=64) return MSM_ERR_INVALIDID;
    seId+=(charNo<<6);

    // if (omSysExitReq != 0) {
    //     return 0;
    // }
    // seId += (charNo << 6);
    // param.flag = MSM_SEPARAM_NONE;
    // if (HuAuxAVol != -1) {
    //     param.flag |= MSM_SEPARAM_AUXVOLA;
    // }
    // if (HuAuxBVol != -1) {
    //     param.flag |= MSM_SEPARAM_AUXVOLB;
    // }
    // param.auxAVol = HuAuxAVol;
    // param.auxBVol = HuAuxBVol;
    return HuSePlay(seId, &param);
}

s32 CharFXPlayPos(s16 charNo, s16 seId, Vec *pos) {
    MSM_SEPARAM param={0};
    if(!pos || charNo<0 || charNo>=8 || seId<0 || seId>=64) return MSM_ERR_INVALIDID;
    seId+=(charNo<<6);param.flag=MSM_SEPARAM_POS;param.pos=*pos;

    // if (omSysExitReq != 0) {
    //     return 0;
    // }
    // seId += (charNo << 6);
    // param.flag = MSM_SEPARAM_POS;
    // if (HuAuxAVol != -1) {
    //     param.flag |= MSM_SEPARAM_AUXVOLA;
    // }
    // if (HuAuxBVol != -1) {
    //     param.flag |= MSM_SEPARAM_AUXVOLB;
    // }
    // param.auxAVol = HuAuxAVol;
    // param.auxBVol = HuAuxBVol;
    // param.pos.x = pos->x;
    // param.pos.y = pos->y;
    // param.pos.z = pos->z;
    return HuSePlay(seId, &param);
}

void CharFXStop(s16 charNo, s16 seId) {
    if(charNo<0 || charNo>=8 || seId<0 || seId>=64) return;
    PartyboardMusyxSoftwareStopEffects(&portSynth,seId+(charNo<<6));
    // int seNoTbl[MSM_ENTRY_SENO_MAX]; // size unknown (min: 30, max: 33)
    // u16 id;
    // u16 i;

    // seId += (charNo << 6);
    // id = msmSeGetEntryID(seId, seNoTbl);
    // for (i = 0; i < id; i++) {
    //     msmSeStop(seNoTbl[i], 0);
    // }
}

static int HuSePlay(int seId, MSM_SEPARAM *param)
{
    int flags=param?param->flag:0;
    if(!portSynth.active) return MSM_ERR_INITFAIL;
    if(seId<0) return MSM_ERR_INVALIDID;
    if(flags&MSM_SEPARAM_POS) {
        SND_FVECTOR position={param->pos.x,param->pos.y,param->pos.z};
        if(!portSynth.listener_active) return MSM_LISTENER_NO_UPDATA;
        return PartyboardMusyxSoftwarePositional(&portSynth,(unsigned)seId,&position,
            flags&MSM_SEPARAM_VOL?(unsigned)param->vol:127,
            flags&MSM_SEPARAM_AUXVOLA?param->auxAVol:HuAuxAVol,
            flags&MSM_SEPARAM_AUXVOLB?param->auxBVol:HuAuxBVol);
    }
    return PartyboardMusyxSoftwareEffect(&portSynth,(unsigned)seId,
        flags&MSM_SEPARAM_VOL?(unsigned)param->vol:127,flags&MSM_SEPARAM_PAN?(unsigned)param->pan:64,
        flags&MSM_SEPARAM_PITCH?param->pitch:0,
        flags&MSM_SEPARAM_AUXVOLA?param->auxAVol:HuAuxAVol,
        flags&MSM_SEPARAM_AUXVOLB?param->auxBVol:HuAuxBVol,
        flags&MSM_SEPARAM_SPAN?param->span:-1);
}
