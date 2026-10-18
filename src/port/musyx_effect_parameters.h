#ifndef PARTYBOARD_MUSYX_EFFECT_PARAMETERS_H
#define PARTYBOARD_MUSYX_EFFECT_PARAMETERS_H
#include <string.h>
#include "musyx/musyx.h"
#include "msm_container.h"
/* Non-positional msmSePlay branch. Positional listener/emitter dispatch must
 * use the original emitter API separately. Caller owns the five parameters. */
static inline int PartyboardMusyxEffectParameters(SND_PARAMETER destination[5],
        const PartyboardMsmContainerEffect *effect,
        unsigned volume, unsigned pan, int pitch, unsigned reverb, unsigned chorus) {
    SND_PARAMETER result[5] = {0};
    int combined_pan;
    int64_t combined_pitch;
    if(!destination || !effect || effect->group==0xffffu
            || effect->volume<0 || effect->volume>127
            || effect->pan<0 || effect->pan>127
            || volume>127 || pan>127 || reverb>127 || chorus>127) return 0;
    combined_pan=(int)pan+effect->pan-64;
    if(combined_pan<0) combined_pan=0;
    if(combined_pan>127) combined_pan=127;
    combined_pitch=(int64_t)pitch+effect->pitch_bend+8192;
    if(combined_pitch<0) combined_pitch=0;
    if(combined_pitch>16383) combined_pitch=16383;
    result[0].ctrl=SND_MIDICTRL_VOLUME;
    result[0].paraData.value14=(u16)((volume*(unsigned)effect->volume/127)*129);
    result[1].ctrl=SND_MIDICTRL_PANNING;
    result[1].paraData.value14=(u16)(combined_pan*129);
    result[2].ctrl=SND_MIDICTRL_REVERB;
    result[2].paraData.value7=(u8)reverb;
    result[3].ctrl=SND_MIDICTRL_CHORUS;
    result[3].paraData.value7=(u8)chorus;
    result[4].ctrl=SND_MIDICTRL_PITCHBEND;
    result[4].paraData.value14=(u16)combined_pitch;
    memcpy(destination,result,sizeof(result));
    return 1;
}
/* Caller has registered the effect's synthesis group and chosen the
 * non-positional branch. The original API consumes the parameters immediately. */
static inline uint32_t PartyboardMusyxEffectStartNonPositional(
        const PartyboardMsmContainerEffect *effect, unsigned volume,
        unsigned pan, int pitch, unsigned reverb, unsigned chorus, uint8_t studio) {
    SND_PARAMETER parameters[5];
    SND_PARAMETER_INFO info;
    uint32_t voice;
    if(!effect || effect->effect>0xffffu || effect->span>127
            || !PartyboardMusyxEffectParameters(parameters,effect,volume,pan,pitch,reverb,chorus))
        return 0xffffffffu;
    info.numPara=5;info.paraArray=parameters;
    voice=sndFXStartParaInfo((SND_FXID)effect->effect,0xff,0xff,studio,&info);
    if(voice!=0xffffffffu) sndFXSurroundPanning(voice,(u8)effect->span);
    return voice;
}
#endif
