#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <emscripten.h>
#include "libco.h"

static cothread_t parent;
static int resumed;

void OSReport(const char *message, ...)
{
    va_list args;
    va_start(args, message);
    vprintf(message, args);
    fflush(stdout);
    va_end(args);
}

static void child(void)
{
    OSReport("Fiber stdio: before async sleep\n");
    emscripten_sleep(10);
    OSReport("Fiber stdio: after async sleep\n");
    resumed = 1;
    co_switch(parent);
    abort();
}

int main(void)
{
    parent = co_active();
    cothread_t fiber = co_create(65536, child);
    assert(fiber);
    co_switch(fiber);
    assert(resumed);
    co_delete(fiber);
    OSReport("PASS: stdio survived Asyncify rewind through an Emscripten fiber\n");
    return 0;
}
