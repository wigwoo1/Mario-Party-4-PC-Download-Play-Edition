#include <dolphin/vi.h>

/* Logical retraces: callbacks run deterministically before presentation.
 * Wall-clock waiting is deferred until frame_limiter, after GPU submission. */
static u32 retrace_count;
static u32 pending_retraces;
static VIRetraceCallback pre_callback;
static VIRetraceCallback post_callback;

u32 VIGetRetraceCount(void) { return retrace_count; }
u32 VIGetNextField(void) { return 0; } /* Browser output is progressive. */
VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback callback)
{
    VIRetraceCallback previous = pre_callback;
    pre_callback = callback;
    return previous;
}
VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback callback)
{
    VIRetraceCallback previous = post_callback;
    post_callback = callback;
    return previous;
}
void VIWaitForRetrace(void)
{
    ++retrace_count;
    ++pending_retraces;
    if (pre_callback) pre_callback(retrace_count);
    if (post_callback) post_callback(retrace_count);
}
u32 PartyBoardBrowserTakeRetraces(void)
{
    u32 count = pending_retraces;
    pending_retraces = 0;
    return count;
}
