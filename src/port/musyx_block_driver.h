#ifndef PARTYBOARD_MUSYX_BLOCK_DRIVER_H
#define PARTYBOARD_MUSYX_BLOCK_DRIVER_H
#include "musyx_voice_scheduler.h"
#include "musyx/hardware.h"
#include "musyx/seq.h"
#include "musyx/s3d.h"
#include "musyx/stream.h"

extern u8 salFrame, salAuxFrame;
void hwSetTimeOffset(u8 offset);

static inline int PartyboardMusyxControlsReady(const PartyboardMusyxVoiceScheduler *scheduler) {
    return scheduler && scheduler->voices && scheduler->bound_voices == dspVoice
        && sndActive && synthVoice && synthInfo.numSamples == 32
        && synthInfo.voiceNum == scheduler->voices && synthInfo.studioNum == scheduler->studios
        && salNumVoices == scheduler->voices && salMaxStudioNum == scheduler->studios;
}

/* Same control order as original hardware.c snd_handle_irq, after the block's
 * DSP/studio work. The caller owns single-threaded synthesis; this is not an
 * audio callback and does not emulate the console interrupt/mutex backend. */
static inline int PartyboardMusyxAdvanceControls(const PartyboardMusyxVoiceScheduler *scheduler) {
    unsigned voice, period;
    if(!PartyboardMusyxControlsReady(scheduler)) return 0;
    salHandleAuxProcessing();
    salFrame ^= 1;
    salAuxFrame = (salAuxFrame + 1) % 3;
    for(voice = 0; voice < scheduler->voices; ++voice)
        memset(dspVoice[voice].changed, 0, sizeof dspVoice[voice].changed);
    for(period = 0; period < 5; ++period) {
        hwSetTimeOffset((u8)period);
        seqHandle(256);
        synthHandle(256);
    }
    hwSetTimeOffset(0);
    s3dHandle();
    streamHandle();
    vsSampleUpdates();
    return 1;
}

/* Process studio buses into a device/PCM sink before the original aux/control
 * phase. 1=accepted, 0=discarded/backpressure, negative=device error. Neither
 * discard nor error may determine whether synthesis/control time advances.
 * Caller must implement studio processing and final output, not skip them. */
typedef int (*PartyboardMusyxBlockSink)(void *context, int32_t *buses, unsigned studios);
/* Processing failures must stop the synthesis graph. They are not device
 * backpressure/errors and must not enter the device-only counters. */
typedef int (*PartyboardMusyxBlockProcessor)(void *context, int32_t *buses, unsigned studios);
typedef struct {
    PartyboardMusyxVoiceScheduler *scheduler;
    unsigned ticks_per_second, remainder;
    uint64_t ticks, mixed_blocks, completed_blocks, control_periods;
    uint64_t accepted_blocks, dropped_blocks, device_errors;
    int busy, failed;
} PartyboardMusyxBlockDriver;

/* Destination starts zeroed. The simulation tick rate stays fixed for this
 * lifetime. At60Hz, the exact rational schedule is3,3,4 blocks per three ticks,
 * not a rounded sample quota or a device-clock-driven update. */
static inline int PartyboardMusyxBlockDriverInit(PartyboardMusyxBlockDriver *driver,
        PartyboardMusyxVoiceScheduler *scheduler, unsigned ticks_per_second) {
    if(!driver || driver->scheduler || !PartyboardMusyxControlsReady(scheduler)
            || !ticks_per_second || ticks_per_second > 1000) return 0;
    memset(driver, 0, sizeof *driver);
    driver->scheduler = scheduler; driver->ticks_per_second = ticks_per_second;
    return 1;
}
/* Scratch holds scheduler->studios *160*9 integer samples. The driver itself
 * does not allocate, sleep or query clocks/wait on devices. Callbacks and sinks
 * retain their own resource contracts. A synthesis/graph failure
 * latches, preventing a retry from duplicating an already consumed partial
 * tick. The backend must then shut down explicitly, not submit a failed block.
 * Device-only failures remain local diagnostics while sound status progresses. */
static inline int PartyboardMusyxBlockDriverTickProcessed(PartyboardMusyxBlockDriver *driver,
        int32_t *scratch, PartyboardMusyxBlockProcessor process, void *process_context,
        PartyboardMusyxBlockSink sink, void *context) {
    unsigned blocks, block;
    if(!driver || !scratch || driver->busy || driver->failed || !PartyboardMusyxControlsReady(driver->scheduler)
            || !driver->ticks_per_second || driver->ticks_per_second > 1000
            || driver->ticks == UINT64_MAX || driver->mixed_blocks > UINT64_MAX - 200
            || driver->completed_blocks > UINT64_MAX - 200
            || driver->control_periods > UINT64_MAX - 1000
            || driver->accepted_blocks > UINT64_MAX - 200
            || driver->dropped_blocks > UINT64_MAX - 200
            || driver->device_errors > UINT64_MAX - 200) return 0;
    blocks = (driver->remainder + 200) / driver->ticks_per_second;
    driver->remainder = (driver->remainder + 200) % driver->ticks_per_second;
    driver->busy = 1;
    for(block = 0; block < blocks; ++block) {
        int result;
        streamCorrectLoops();
        if(!PartyboardMusyxVoiceSchedulerRender(driver->scheduler, scratch)) goto fail;
        ++driver->mixed_blocks;
        if(process && !process(process_context, scratch, driver->scheduler->studios)) goto fail;
        result = sink ? sink(context, scratch, driver->scheduler->studios) : 0;
        if(result > 0) ++driver->accepted_blocks;
        else { ++driver->dropped_blocks; if(result < 0) ++driver->device_errors; }
        if(!PartyboardMusyxAdvanceControls(driver->scheduler)) goto fail;
        ++driver->completed_blocks; driver->control_periods += 5;
    }
    ++driver->ticks; driver->busy = 0;
    return 1;
fail:
    driver->failed = 1; driver->busy = 0;
    return 0;
}
static inline int PartyboardMusyxBlockDriverTick(PartyboardMusyxBlockDriver *driver,
        int32_t *scratch, PartyboardMusyxBlockSink sink, void *context) {
    return PartyboardMusyxBlockDriverTickProcessed(driver, scratch, NULL, NULL, sink, context);
}
#endif
