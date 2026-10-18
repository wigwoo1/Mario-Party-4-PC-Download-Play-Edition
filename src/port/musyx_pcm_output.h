#ifndef PARTYBOARD_MUSYX_PCM_OUTPUT_H
#define PARTYBOARD_MUSYX_PCM_OUTPUT_H
#include "musyx_block_driver.h"
#include "musyx_studio_process.h"
#include "pcm_output.h"

/* Connect the tested simulation-clock studio stage to the existing SDL PCM
 * adapter. Caller owns stable driver/studio/output objects and their shutdown;
 * this bridge allocates nothing and never opens or waits on a device. Output
 * may be absent or replaced at block boundaries without resetting synthesis.
 * Six-channel mode preserves dry front L/R and sends the original delayed mono
 * surround bus to both rears. Un-authored center/LFE remain silent. Physical
 * speaker mapping and independently authored six-channel content are not implied. */
typedef struct {
    PartyboardMusyxBlockDriver *driver;
    PartyboardMusyxStudioProcessor *processor;
    PartyboardPcmOutput *output;
    uint64_t last_output_block;
} PartyboardMusyxPcmBridge;

static inline int PartyboardMusyxPcmBridgeAligned(const PartyboardMusyxPcmBridge *bridge) {
    return bridge && bridge->driver && bridge->processor && !bridge->driver->busy
        && !bridge->driver->failed && PartyboardMusyxControlsReady(bridge->driver->scheduler)
        && bridge->processor->studios == bridge->driver->scheduler->studios
        && bridge->processor->blocks == bridge->driver->completed_blocks
        && bridge->driver->mixed_blocks == bridge->driver->completed_blocks
        && bridge->last_output_block == bridge->driver->completed_blocks;
}
static inline int PartyboardMusyxPcmBridgeInit(PartyboardMusyxPcmBridge *bridge,
        PartyboardMusyxBlockDriver *driver, PartyboardMusyxStudioProcessor *processor,
        PartyboardPcmOutput *output) {
    PartyboardMusyxPcmBridge next = {0};
    if(!bridge || bridge->driver || !driver || !processor) return 0;
    next.driver = driver; next.processor = processor; next.output = output;
    next.last_output_block = driver->completed_blocks;
    if(!PartyboardMusyxPcmBridgeAligned(&next)) return 0;
    *bridge = next; return 1;
}
static inline int PartyboardMusyxPcmBridgeSetOutput(PartyboardMusyxPcmBridge *bridge,
        PartyboardPcmOutput *output) {
    if(!PartyboardMusyxPcmBridgeAligned(bridge)) return 0;
    bridge->output = output; return 1;
}
/* Called by the driver after a new studio block and before SDK control time.
 * Full queues, a missing stream and local output errors are output-only. They
 * must never rewind voices, postpone controls or retry the same PCM block. */
static inline int PartyboardMusyxPcmBridgeSubmit(void *context, int32_t *buses, unsigned studios) {
    PartyboardMusyxPcmBridge *bridge = (PartyboardMusyxPcmBridge*)context;
    PartyboardPcmOutput *output;
    if(!bridge || !buses || !bridge->driver || !bridge->processor || !bridge->driver->busy
            || bridge->last_output_block == UINT64_MAX
            || studios != bridge->processor->studios
            || bridge->processor->blocks != bridge->driver->mixed_blocks
            || bridge->processor->blocks != bridge->last_output_block + 1) return -1;
    bridge->last_output_block = bridge->processor->blocks;
    output = bridge->output;
    if(!output || !output->stream) return 0;
    if(output->frequency != 32000 || (output->channels != 2 && output->channels != 6)) return -1;
    return output->channels==6
        ? PartyboardPcmOutputSubmit(output,bridge->processor->speakers,160)
        : PartyboardPcmOutputSubmit(output,bridge->processor->stereo,160);
}
static inline int PartyboardMusyxPcmBridgeTick(PartyboardMusyxPcmBridge *bridge, int32_t *scratch) {
    if(!scratch || !PartyboardMusyxPcmBridgeAligned(bridge)) return 0;
    return PartyboardMusyxBlockDriverTickProcessed(bridge->driver, scratch,
        PartyboardMusyxStudioProcess, bridge->processor, PartyboardMusyxPcmBridgeSubmit, bridge);
}
#endif
