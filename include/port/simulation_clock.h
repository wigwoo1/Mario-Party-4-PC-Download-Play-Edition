#ifndef PARTYBOARD_SIMULATION_CLOCK_H
#define PARTYBOARD_SIMULATION_CLOCK_H
#include <dolphin.h>
#include "version.h"

/* For gameplay waits only. Do not replace device, I/O or profiling clocks. */
SHARED_SYM u32 PartyBoardGameTick(void);
#endif
