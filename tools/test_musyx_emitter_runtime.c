#include <assert.h>
#include <stdio.h>
#include "musyx/synth.h"
#include "musyx/synthdata.h"
#include "../src/port/musyx_emitter_runtime.h"
int main(void) {
    PartyboardMusyxEmitter runtime={0};
    PartyboardMsmContainerEffect descriptor={0};
    SND_FVECTOR position={1,2,3};
    descriptor.emitter=1;descriptor.effect=65534;descriptor.volume=100;
    sndActive=1;dataInitStack();
    assert(PartyboardMusyxEmitterStart(&runtime,&descriptor,&position,100,7,64,23,45));
    assert(runtime.emitter.paraInfo==&runtime.info);
    assert(runtime.info.paraArray==runtime.parameters && runtime.info.numPara==2);
    assert(runtime.parameters[0].paraData.value7==23 && runtime.parameters[1].paraData.value7==45);
    assert(runtime.emitter.pos.x==1 && runtime.emitter.pos.y==2 && runtime.emitter.pos.z==3);
    assert(!PartyboardMusyxEmitterStart(&runtime,&descriptor,&position,100,7,64,23,45));
    assert(PartyboardMusyxEmitterStop(&runtime));
    assert(!runtime.active && !PartyboardMusyxEmitterStop(&runtime));
    puts("Original continuous emitter lifetime PASS");return 0;
}
