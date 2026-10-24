/* Actual-data SDK voice-allocation failure, rollback and retry; no device. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#include "../src/port/musyx_software.h"

static void *owned[4096];
static unsigned calls,alive,fail_first;
static void *allocate(size_t bytes) {
    ++calls;
    if(fail_first && calls==1) return NULL;
    void *pointer=malloc(bytes);
    if(pointer){unsigned i=0;while(i<4096 && owned[i])++i;assert(i<4096);owned[i]=pointer;++alive;}
    return pointer;
}
static void release(void *pointer) {
    if(!pointer)return;
    unsigned i=0;while(i<4096 && owned[i]!=pointer)++i;assert(i<4096 && alive);
    owned[i]=NULL;--alive;free(pointer);
}
static PartyboardMusyxSoftware owner;
static uint32_t play(void) {
    uint32_t hash=2166136261u;
    assert(PartyboardMusyxSoftwareSong(&owner,0,0)==0);
    for(unsigned tick=0;tick<12;++tick){
        assert(PartyboardMusyxSoftwareTick(&owner));
        for(unsigned sample=0;sample<320;++sample)
            hash=(hash^(uint16_t)owner.processor.stereo[sample])*16777619u;
    }
    assert(owner.driver.completed_blocks==40 && owner.nonzero_samples>0);
    return hash;
}
static void close_owner(void) {
    assert(PartyboardMusyxSoftwareShutdown(&owner));
    assert(!owner.active && !sndActive && !synthVoice && !dspVoice && !dspSurround && !alive);
    assert(!salNumVoices && !salMaxStudioNum);
}
int main(void) {
    uint8_t header[8];
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY);
#endif
    assert(fread(header,1,8,stdin)==8);
    size_t bytes=PartyboardMsmU32(header);assert(bytes && bytes<=67108864);
    uint8_t *raw=malloc(bytes);assert(raw && fread(raw,1,bytes,stdin)==bytes);
    calls=fail_first=0;
    assert(PartyboardMusyxSoftwareInitWithAllocator(&owner,raw,bytes,allocate,release));
    const uint32_t baseline=play();close_owner();
    for(unsigned iteration=0;iteration<8;++iteration){
        calls=0;fail_first=1;
        assert(!PartyboardMusyxSoftwareInitWithAllocator(&owner,raw,bytes,allocate,release));
        assert(calls==1 && !alive && owner.last_error==10);
        assert(!owner.active && !sndActive && !synthVoice && !dspVoice && !dspSurround);
        assert(!salNumVoices && !salMaxStudioNum && !synthInfo.voiceNum);
        assert(PartyboardMusyxSoftwareShutdown(&owner));
        calls=fail_first=0;
        assert(PartyboardMusyxSoftwareInitWithAllocator(&owner,raw,bytes,allocate,release));
        assert(play()==baseline);close_owner();
    }
    free(raw);
    printf("{\"voiceAllocationFailureRetryCycles\":8,\"pcmHash\":%u,\"blocksPerHealthyRun\":40,\"trackedSdkAllocationsRemaining\":%u,\"deviceOpened\":false}\n",baseline,alive);
    return 0;
}
