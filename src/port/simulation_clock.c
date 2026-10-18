#include "port/simulation_clock.h"
#include "port/input_trace.h"

u32 PartyBoardGameTick(void) {
    extern s32 VCounter;
    u32 seed;
    if (!PartyBoardFixedSeed(&seed)) return OSGetTick();
    return (u32)(((u64)(u32)VCounter * OS_TIMER_CLOCK) / REFRESH_RATE);
}
