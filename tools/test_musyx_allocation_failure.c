#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#include <SDL3/SDL.h>

static void *owned[4096];
static unsigned alive,attempts,fail_at,failures;
static int reject_allocation(void) {
    ++attempts;
    if(fail_at && attempts==fail_at){++failures;return 1;}
    return 0;
}
static void *remember(void *pointer) {
    if(pointer){unsigned i;for(i=0;i<4096 && owned[i];++i){}assert(i<4096);owned[i]=pointer;++alive;}
    return pointer;
}
static void *tracked_malloc(size_t bytes) {
    return reject_allocation()?NULL:remember(malloc(bytes));
}
static void *tracked_calloc(size_t count,size_t bytes) {
    return reject_allocation()?NULL:remember(calloc(count,bytes));
}
static void tracked_free(void *pointer) {
    if(!pointer)return;
    for(unsigned i=0;i<4096;++i)if(owned[i]==pointer){owned[i]=NULL;assert(alive);--alive;break;}
    /* The original SDK's healthy initialization used its ordinary allocator.
       It may release those baseline blocks through the subsequently set hook. */
    free(pointer);
}
#define malloc(bytes) tracked_malloc(bytes)
#define calloc(count,bytes) tracked_calloc(count,bytes)
#define free(pointer) tracked_free(pointer)
#include "../src/port/musyx_software.h"
#undef malloc
#undef calloc
#undef free

static PartyboardMusyxSoftware owner;
static unsigned dynamic_group,aux_a,aux_b;
static uint8_t *raw;
static size_t raw_bytes;

static void initialize(unsigned operation) {
    assert(!alive);attempts=fail_at=failures=0;
    assert(PartyboardMusyxSoftwareInit(&owner,raw,raw_bytes));
    salHooks.malloc=tracked_malloc;salHooks.free=tracked_free;
    if(operation==1){assert(PartyboardMusyxSoftwareSong(&owner,0,0)==0);PartyboardMusyxSoftwareStopSong(&owner,0);}
    if(operation==2)assert(PartyboardMusyxAuxRuntimeSelect(&owner.aux,&owner.container,(int)aux_a,(int)aux_b));
    attempts=failures=0;
}
static int operate(unsigned operation) {
    if(operation==0)return PartyboardMusyxSoftwareLoadIndex(&owner,dynamic_group);
    if(operation==1)return PartyboardMusyxSoftwareSong(&owner,0,0)==0;
    return PartyboardMusyxAuxRuntimeSelect(&owner.aux,&owner.container,(int)aux_a,(int)aux_b);
}
static void finish(void) {
    fail_at=0;
    assert(PartyboardMusyxSoftwareShutdown(&owner));
    assert(!owner.active && !sndActive && !dspVoice && !synthVoice && !alive);
}

int main(void) {
    uint8_t header[8];
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);
#endif
    assert(fread(header,1,8,stdin)==8);raw_bytes=PartyboardMsmU32(header);assert(raw_bytes<=67108864);
    raw=malloc(raw_bytes);assert(raw && fread(raw,1,raw_bytes,stdin)==raw_bytes);
    initialize(0);
    dynamic_group=UINT32_MAX;
    for(unsigned index=0;index<owner.info.groups;++index){
        PartyboardMsmContainerGroup group;int base=0;
        assert(PartyboardMsmContainerGetGroup(&group,&owner.container,index));
        for(unsigned i=0;i<owner.loaded;++i)if(owner.indices[i]==index)base=1;
        if(!base && group.id!=65535 && group.data_bytes && group.sample_bytes){dynamic_group=index;break;}
    }
    aux_a=aux_b=UINT32_MAX;
    for(unsigned index=0;index<owner.container.section[PARTYBOARD_MSM_AUX].bytes/40;++index){
        PartyboardMsmContainerAux preset;assert(PartyboardMsmContainerGetAux(&preset,&owner.container,index));
        if(aux_a==UINT32_MAX && preset.type==0)aux_a=index;
        if(aux_b==UINT32_MAX && preset.type==3)aux_b=index;
    }
    assert(dynamic_group!=UINT32_MAX && aux_a!=UINT32_MAX && aux_b!=UINT32_MAX);finish();
    for(unsigned operation=0;operation<3;++operation){
        initialize(operation);assert(operate(operation));unsigned allocation_points=attempts;assert(allocation_points>0 && allocation_points<256);finish();
        for(unsigned failure_point=1;failure_point<=allocation_points;++failure_point){
            initialize(operation);
            unsigned baseline_alive=alive,baseline_loaded=owner.loaded;
            PartyboardMusyxAuxInstance *old_a=owner.aux.instance[0],*old_b=owner.aux.instance[1];
            attempts=failures=0;fail_at=failure_point;
            assert(!operate(operation));assert(failures==1);fail_at=0;
            assert(alive==baseline_alive && owner.loaded==baseline_loaded && owner.active && sndActive);
            assert(PartyboardMusyxAuxOwnsBindings(&owner.aux));
            assert(owner.aux.instance[0]==old_a && owner.aux.instance[1]==old_b);
            if(operation==1)assert(!owner.songs[0].arrangement.data);
            assert(PartyboardMusyxSoftwareTick(&owner) && !owner.driver.failed);
            assert(operate(operation));finish();
        }
        printf("{\"operation\":%u,\"allocationFailurePoints\":%u,\"rollbackAndRetryPassed\":true}\n",operation,allocation_points);
    }
    free(raw);return 0;
}
