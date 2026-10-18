#include <assert.h>
#include <stdio.h>

/* Link the production limiter with emscripten_get_now/test_now and
   emscripten_sleep/test_sleep substitutions to isolate scheduling policy. */
static double clockMs;
static unsigned int retraces, sleepCalls;
double test_now(void) { return clockMs; }
void test_sleep(unsigned int milliseconds) {
    assert(milliseconds > 0);
    clockMs += milliseconds;
    ++sleepCalls;
}
unsigned int PartyBoardBrowserTakeRetraces(void) {
    unsigned int result = retraces;
    retraces = 0;
    return result;
}
void frame_limiter(void);

int main(void) {
    frame_limiter();
    assert(clockMs >= 16.0 && clockMs <= 18.0);
    clockMs += 45.0;
    double start = clockMs;
    unsigned int previousSleeps = sleepCalls;
    frame_limiter();
    assert(clockMs == start && sleepCalls == previousSleeps);
    start = clockMs;
    for (int i = 0; i < 6; ++i) frame_limiter();
    assert(clockMs - start >= 99.0 && clockMs - start <= 102.0);
    clockMs += 100.0;
    start = clockMs;
    frame_limiter();
    assert(clockMs - start >= 16.0 && clockMs - start <= 18.0);
    retraces = 2;
    start = clockMs;
    frame_limiter();
    assert(clockMs - start >= 32.0 && clockMs - start <= 35.0);
    puts("PASS production limiter policy: ordinary overrun, paced recovery, long pause and two retraces.");
    return 0;
}
