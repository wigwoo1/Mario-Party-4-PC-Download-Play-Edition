#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "../src/port/musyx_effect_parameters.h"
int main(void) {
    PartyboardMsmContainerEffect effect={0};
    SND_PARAMETER parameters[5],saved[5];
    effect.volume=100;effect.pan=20;effect.pitch_bend=-100;
    assert(PartyboardMusyxEffectParameters(parameters,&effect,64,64,0,23,45));
    assert(parameters[0].ctrl==SND_MIDICTRL_VOLUME && parameters[0].paraData.value14==50*129);
    assert(parameters[1].ctrl==SND_MIDICTRL_PANNING && parameters[1].paraData.value14==20*129);
    assert(parameters[2].ctrl==SND_MIDICTRL_REVERB && parameters[2].paraData.value7==23);
    assert(parameters[3].ctrl==SND_MIDICTRL_CHORUS && parameters[3].paraData.value7==45);
    assert(parameters[4].ctrl==SND_MIDICTRL_PITCHBEND && parameters[4].paraData.value14==8092);
    assert(PartyboardMusyxEffectParameters(parameters,&effect,127,0,INT_MIN,0,0));
    assert(parameters[1].paraData.value14==0 && parameters[4].paraData.value14==0);
    effect.pan=127;
    assert(PartyboardMusyxEffectParameters(parameters,&effect,127,127,INT_MAX,127,127));
    assert(parameters[1].paraData.value14==16383 && parameters[4].paraData.value14==16383);
    memcpy(saved,parameters,sizeof(saved));effect.group=0xffff;
    assert(!PartyboardMusyxEffectParameters(parameters,&effect,127,64,0,0,0));
    assert(!memcmp(saved,parameters,sizeof(saved)));
    effect.group=0;
    assert(!PartyboardMusyxEffectParameters(parameters,&effect,128,64,0,0,0));
    assert(!memcmp(saved,parameters,sizeof(saved)));
    puts("MSM effect parameter fidelity PASS");return 0;
}
