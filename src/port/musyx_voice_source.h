#ifndef PARTYBOARD_MUSYX_VOICE_SOURCE_H
#define PARTYBOARD_MUSYX_VOICE_SOURCE_H
#include "musyx/dspvoice.h"
#include "musyx_sample_memory.h"
#include "musyx_sample_source.h"

/* Open the exact sample selected by the original voice initializer. Caller
 * must retain registered payload and host-endian parameters while reading. */
static inline int PartyboardMusyxVoiceSourceOpen(PartyboardMsmSample *destination,
        const DSPvoice *voice, const PartyboardMusyxSampleMemory *memory) {
    size_t available;
    if (!destination || !voice || !memory) return 0;
    available = PartyboardMusyxSampleMemoryAvailable(memory, voice->smp_info.addr);
    if (!available) return 0;
    return PartyboardMusyxSampleSourceOpen(destination, &voice->smp_info, available);
}
#endif
