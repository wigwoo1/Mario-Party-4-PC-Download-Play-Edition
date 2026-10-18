#include <dolphin.h>
#include "port/input_trace.h"
#include "port/simulation_clock.h"
#include <assert.h>
#include <stdio.h>

s32 rnd_seed;
u32 boardRandSeed;
s32 VCounter;
static int tick_calls;
u32 OSGetTick(void) { ++tick_calls; return 987654321u; }
extern void BoardRandInit(void);
extern u32 BoardRand(void);
extern float BoardRandFloat(void);
static int time_calls;
static int rand8_calls;
OSTime OSGetTime(void) { ++time_calls; return 123456789; }
s32 rand8(void) { ++rand8_calls; return 99; }
extern u32 frand(void);
extern u32 frandmod(u32);

int main(void) {
    const u32 expected[] = {4294261402u, 3273992258u, 3280574171u, 32544728u, 2776570138u};
    int i, run;
    assert(PartyBoardGameTick() == 987654321u && tick_calls == 1);
    frand();
    assert(time_calls == 1 && rand8_calls == 1); // Ordinary startup retains its behavior.
    BoardRandInit();
    assert(time_calls == 2 && boardRandSeed == 123456789);
    for (run = 0; run < 2; ++run) {
        PartyBoardRandomSeed(42);
        assert(rnd_seed == 42);
        for (i = 0; i < 5; ++i) assert(frand() == expected[i]);
    }
    PartyBoardRandomSeed(0);
    VCounter = 0;
    assert(PartyBoardGameTick() == 0);
#if VERSION_NTSC
    VCounter = 180;
#else
    VCounter = 150;
#endif
    assert(OSTicksToMilliseconds(PartyBoardGameTick()) == 3000);
    VCounter = 10000000;
#if VERSION_NTSC
    assert(PartyBoardGameTick() == (u32)((10000000ULL * OS_TIMER_CLOCK) / 60));
#else
    assert(PartyBoardGameTick() == (u32)((10000000ULL * OS_TIMER_CLOCK) / 50));
#endif
    assert(tick_calls == 1); // Seeded waits never sample the hardware clock.
    assert(PartyBoardRandomState() != 0 && rnd_seed == 0);
    PartyBoardRandomSeed(2147483647u); // Recurrence reaches zero: never reseed from time.
    assert(frand() == 0);
    assert(frand() == 0);
    PartyBoardRandomSeed(42);
    assert(frandmod(0) == (expected[0] & 0x7fffffffu));
    for (run = 0; run < 2; ++run) {
        u32 expected_board = 42u ^ 0x9E3779B9u;
        PartyBoardRandomSeed(42);
        frand(); // Board initialization must not depend on the frand stream position.
        BoardRandInit();
        assert(boardRandSeed == expected_board);
        for (i = 0; i < 100; ++i) {
            expected_board = (u32)((u64)expected_board * 1664525u + 1013904223u);
            assert(BoardRand() == expected_board);
        }
        for (i = 0; i < 100; ++i) {
            float value = BoardRandFloat();
            assert(value >= 0.0f && value < 1.0f);
        }
    }
    PartyBoardRandomSeed(0);
    BoardRandInit();
    assert(boardRandSeed == 0x9E3779B9u);
    assert(time_calls == 2 && rand8_calls == 1);
    puts("Random seed tests passed");
}
