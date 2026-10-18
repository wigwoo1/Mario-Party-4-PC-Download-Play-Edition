/* Uses real process.c and memory.c; only the outer system heap and logging are stubs. */
#include "game/process.h"
#include "game/memory.h"
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
static int heaps, destroyed, stage, child_done, ticks;
static int sleeper_started, survivor_ran;
static void sleeper(void) {
    ++sleeper_started;
    HuPrcSleep(1000);
    abort(); /* A killed process must never resume its entry function. */
}
static void never_started(void) { abort(); }
static void survivor(void) { ++survivor_ran; HuPrcEnd(); abort(); }
void *HuMemDirectMalloc(HeapID heap, size_t size) { (void)heap; ++heaps; return malloc(size); }
void HuMemDirectFree(void *ptr) { --heaps; free(ptr); }
void OSReport(const char *fmt, ...) { va_list args; va_start(args,fmt); vfprintf(stderr,fmt,args); va_end(args); }
static void destructor(void) { ++destroyed; }
static void child(void) {
    volatile int marker=12345;
    HuPrcDestructorSet(destructor);
    HuPrcVSleep();
    assert(marker==12345);
    child_done=1;
    HuPrcEnd();
    abort();
}
static void parent(void) {
    Process *self=HuPrcCurrentGet();
    HuPrcDestructorSet(destructor);
    stage=1;
    HuPrcSleep(2);
    assert(ticks==4);
    stage=2;
    assert(HuPrcChildCreate(child,10,32768,128,self));
    HuPrcChildWatch();
    assert(child_done);
    stage=3;
    HuPrcEnd();
    abort();
}
int main(void) {
    HuPrcInit();
    assert(HuPrcCreate(parent,20,32768,128));
    ticks=1; HuPrcCall(1); assert(stage==1);
    HuPrcAllPause(1); HuPrcCall(100); assert(stage==1);
    HuPrcAllPause(0);
    ticks=2; HuPrcCall(1); assert(stage==1);
    ticks=3; HuPrcCall(1); assert(stage==1);
    ticks=4; HuPrcCall(1); assert(stage==2);
    ticks=5; HuPrcCall(1); assert(child_done && stage==2);
    ticks=6; HuPrcCall(1); assert(stage==3);
    HuPrcCall(1); assert(heaps==0 && destroyed==2);
    /* Kill a sleeping, paused family; its suspended stacks must be discarded. */
    Process *root=HuPrcCreate(sleeper,30,32768,128);
    Process *kid=HuPrcChildCreate(sleeper,20,32768,128,root);
    Process *grandkid=HuPrcChildCreate(sleeper,10,32768,128,kid);
    HuPrcDestructorSet2(root,destructor);
    HuPrcDestructorSet2(kid,destructor);
    HuPrcDestructorSet2(grandkid,destructor);
    HuPrcCall(1); assert(sleeper_started==3);
    HuPrcAllPause(1);
    assert(HuPrcKill(root)==0);
    assert(root->child==NULL && kid->parent==NULL && grandkid->parent==NULL);
    assert(HuPrcKill(root)==-1);
    assert(HuPrcCreate(survivor,5,32768,128));
    HuPrcCall(1);
    assert(heaps==0 && destroyed==5 && survivor_ran==1);
    /* A process killed before its first timeslice must not enter its function. */
    root=HuPrcCreate(never_started,10,32768,128);
    HuPrcDestructorSet2(root,destructor);
    assert(HuPrcKill(root)==0);
    HuPrcCall(1); HuPrcCall(1);
    assert(heaps==0 && destroyed==6);
    puts("Scheduler contract passed: sleep, pause, child wait, destructors and heap cleanup.");
    puts("Scheduler kill contract passed: paused sleeping family, duplicate kill, unstarted process and successor.");
    return 0;
}
