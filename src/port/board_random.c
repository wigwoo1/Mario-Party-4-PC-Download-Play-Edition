#include "dolphin.h"
#include "port/input_trace.h"
#include <string.h>

extern u32 boardRandSeed;

void BoardRandInit(void) {
    u32 seed;
    if (PartyBoardFixedSeed(&seed)) {
        // A separate deterministic stream, independent of frand's current state.
        boardRandSeed = seed ^ 0x9E3779B9u;
    } else {
        boardRandSeed = (u32)OSGetTime();
    }
}

u32 BoardRand(void) {
    boardRandSeed = boardRandSeed * 0x19660Du + 0x3C6EF35Fu;
    return boardRandSeed;
}

u32 BoardRandMod(u32 value) {
    return (BoardRand() & 0x7FFFFFFFu) % value;
}

float BoardRandFloat(void) {
    const u32 bits = (BoardRand() & 0x7FFFFFu) | 0x3F800000u;
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value - 1.0f;
}
