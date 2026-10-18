#ifndef PARTYBOARD_RANDOM_CALL_TRACE_H
#define PARTYBOARD_RANDOM_CALL_TRACE_H

/* Include after game/frand.h only in sources being diagnosed. Disabled unless
 * a frame range is supplied; never changes the random stream or return value. */
#ifdef TARGET_PC
#include <stdio.h>
#include <stdlib.h>
SHARED_SYM extern u32 GlobalCounter;
SHARED_SYM extern u32 PartyBoardRandomState(void);
#ifdef __EMSCRIPTEN__
extern double PartyBoardRandomTraceBound(int field);
#endif

static inline int PartyBoardRandomTraceEnabled(void)
{
    static int initialized, enabled;
    static unsigned long first, last;
    if (!initialized) {
#ifdef __EMSCRIPTEN__
        double start = PartyBoardRandomTraceBound(0);
        double end = PartyBoardRandomTraceBound(1);
        initialized = 1;
        if (start >= 0) {
            first = (unsigned long)start;
            last = end >= 0 ? (unsigned long)end : first;
            enabled = last >= first;
        }
#else
        const char *start = getenv("PARTYBOARD_RNG_TRACE_FROM");
        const char *end = getenv("PARTYBOARD_RNG_TRACE_TO");
        char *tail;
        initialized = 1;
        if (start && *start) {
            first = strtoul(start, &tail, 10);
            enabled = !*tail;
            last = first;
            if (end && *end) {
                last = strtoul(end, &tail, 10);
                enabled = enabled && !*tail && last >= first;
            }
        }
#endif
    }
    return enabled && GlobalCounter >= first && GlobalCounter <= last;
}
static inline void PartyBoardTraceRandomCall(const char *kind, const char *file,
    unsigned int line, u32 before, u32 after)
{
    static u32 traceFrame;
    static unsigned int count;
    if (!PartyBoardRandomTraceEnabled()) return;
    if (traceFrame != GlobalCounter) { traceFrame = GlobalCounter; count = 0; }
    if (count++ >= 128) return;
    fprintf(stderr, "RNG call frame=%u kind=%s file=%s line=%u before=%u after=%u\n",
        GlobalCounter, kind, file, line, before, after);
    /* Native stderr may be buffered while redirected by the probe harness. */
    fflush(stderr);
}
static inline u32 PartyBoardTracedFrand(const char *file, unsigned int line)
{
    u32 before = PartyBoardRandomState();
    u32 result = frand();
    PartyBoardTraceRandomCall("frand", file, line, before, PartyBoardRandomState());
    return result;
}
static inline u32 PartyBoardTracedFrandmod(u32 range, const char *file, unsigned int line)
{
    u32 before = PartyBoardRandomState();
    u32 result = frandmod(range);
    PartyBoardTraceRandomCall("frandmod", file, line, before, PartyBoardRandomState());
    return result;
}
#define frand() PartyBoardTracedFrand(__FILE__, __LINE__)
#define frandmod(range) PartyBoardTracedFrandmod((range), __FILE__, __LINE__)
#endif
#endif
