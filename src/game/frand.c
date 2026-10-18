#include "dolphin.h"
#include "version.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
EM_JS(double, PartyBoardRandomTraceBound, (int field), {
    const value = field ? Module.partyBoardRandomTraceTo : Module.partyBoardRandomTraceFrom;
    return Number.isInteger(value) && value >= 0 && value <= 0xffffffff ? value : -1;
});
#endif

static u32 frand_seed;

#ifdef TARGET_PC
static u32 replay_seed;
static u32 session_seed;
static int replay_seed_active;
extern s32 rnd_seed;
void PartyBoardRandomSeed(u32 seed) {
    replay_seed_active = 1;
    session_seed = seed;
    replay_seed = seed ? seed : 0xD826BC89;
    frand_seed = replay_seed;
    rnd_seed = (s32)seed;
}
SHARED_SYM u32 PartyBoardRandomState(void) { return frand_seed; }
int PartyBoardFixedSeed(u32 *seed) {
    if (!replay_seed_active) return 0;
    *seed = session_seed;
    return 1;
}
#endif

extern s32 rand8(void);

static inline u32 frandom(u32 param)
{
    s32 rand2, rand3;

    if (param == 0) {
#ifdef TARGET_PC
        if (replay_seed_active) {
            param = replay_seed;
        } else
#endif
        {
        param = rand8();
        param = param ^ (s64)OSGetTime();
        param ^= 0xD826BC89;
        }
    }

    rand2 = param / (u32)0x1F31D;
    rand3 = param - (rand2 * 0x1F31D);
    param = rand2 * 0xB14;
    param =  param - rand3 * 0x41A7;
    return param;
}

u32 frand(void) {
    return frand_seed = frandom(frand_seed);
}

f32 frandf(void) {
    u32 value = frand();
    f32 ret;
    value &= 0x7FFFFFFF;
    ret = (f32)value/2147483648;
    return ret;
}

u32 frandmod(u32 arg0) {
    u32 ret;
    frand_seed = frandom(frand_seed);
#ifdef TARGET_PC
    if (arg0 == 0) {
        ret = (frand_seed & 0x7FFFFFFF);
        return ret;
    }
#endif
    ret = (frand_seed & 0x7FFFFFFF)%arg0;
    return ret;
}
