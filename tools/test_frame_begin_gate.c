#include "port/frame_begin_gate.h"
#include <assert.h>
#include <stdio.h>

static int running, pumps, begins, waits, unavailable, stopAtWait, exitAtPump;
static int is_running(void) { return running; }
static int pump(void) { return ++pumps != exitAtPump; }
static int begin(void) { return ++begins > unavailable; }
static void wait_surface(void) { if (++waits == stopAtWait) running = 0; }
static void reset(void) { running=1; pumps=begins=waits=unavailable=stopAtWait=exitAtPump=0; }
int main(void)
{
    reset();
    assert(PartyBoardWaitForRenderFrame(is_running,pump,begin,wait_surface));
    assert(pumps==1 && begins==1 && waits==0);
    /* One accepted online input must survive repeated hidden-surface waits. */
    reset(); unavailable=3;
    int simulationFrames=0;
    if (PartyBoardWaitForRenderFrame(is_running,pump,begin,wait_surface)) ++simulationFrames;
    assert(simulationFrames==1 && pumps==4 && begins==4 && waits==3);
    reset(); unavailable=100; stopAtWait=2;
    assert(!PartyBoardWaitForRenderFrame(is_running,pump,begin,wait_surface));
    assert(begins==2 && waits==2 && pumps==2);
    reset(); unavailable=100; exitAtPump=2;
    assert(!PartyBoardWaitForRenderFrame(is_running,pump,begin,wait_surface));
    assert(begins==1 && waits==1 && pumps==2);
    reset(); running=0;
    assert(!PartyBoardWaitForRenderFrame(is_running,pump,begin,wait_surface));
    assert(begins==0 && waits==0 && pumps==0);
    puts("PASS: frame gate waits without advancing simulation, refreshes visibility and exits on stop/window close.");
    return 0;
}
