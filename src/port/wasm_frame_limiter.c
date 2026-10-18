#include <emscripten.h>
#include <math.h>
extern unsigned int PartyBoardBrowserTakeRetraces(void);

/* Presentation pacing only. Simulation ticks remain owned by the game. */
void frame_limiter(void)
{
    static double deadline;
    static unsigned int completedFrames, lateRebases;
    static double totalSleepMs, maxSleepMs;
    const double period = 1000.0 / 60.0;
    double now = emscripten_get_now();
    const unsigned int retraces = PartyBoardBrowserTakeRetraces();
    /* Ordinary overruns have already consumed their presentation interval.
       Only a long interruption gets a fresh delay before resuming. The
       deadline update below still prevents a burst after an overdue frame. */
    if (deadline == 0.0 || now > deadline + 4.0 * period) {
        if (deadline != 0.0) ++lateRebases;
        deadline = now + period;
    }
    if (retraces > 1) deadline += (retraces - 1) * period;
    const double sleepStart = now;
    const int slept = now < deadline;
    while (now < deadline) {
        emscripten_sleep((unsigned int)ceil(deadline - now));
        now = emscripten_get_now();
    }
    deadline = (now > deadline + period ? now : deadline) + period;
    const double sleepMs = slept ? now - sleepStart : 0.0;
    totalSleepMs += sleepMs;
    if (sleepMs > maxSleepMs) maxSleepMs = sleepMs;
    /* Diagnostic wall time only; excluded from simulation and checkpoints.
       Publish once per 120 presentations to bound observer overhead. */
    if (++completedFrames % 120 == 0) {
        EM_ASM({globalThis.partyBoardFramePacingTiming = ({
            completedFrames: $0, totalSleepMs: $1, maxSleepMs: $2,
            lateRebases: $3
        });}, completedFrames, totalSleepMs, maxSleepMs, lateRebases);
    }
}
