#ifndef PARTYBOARD_MUSYX_GROUP_RUNTIME_H
#define PARTYBOARD_MUSYX_GROUP_RUNTIME_H
#include "musyx_group.h"
#include "musyx_sample_memory.h"

/* Caller owns encoded samples, and must use Pop in reverse push order.
 * An unsuccessful push leaves no registrations or owned conversions behind. */
static inline int PartyboardMusyxGroupPush(PartyboardMusyxGroup *group,
        PartyboardMusyxSampleMemory *memory, const uint8_t *raw, size_t bytes,
        uint8_t *samples, size_t sample_bytes, uint16_t group_id) {
    PartyboardMusyxGroup result = {0};
    if (!group || !memory || group->project || group->pool || group->directory)
        return 0;
    if (!PartyboardMusyxGroupConvert(&result, raw, bytes, samples, sample_bytes)) return 0;
    if (!PartyboardMusyxSampleMemoryAdd(memory, samples, sample_bytes)) {
        PartyboardMusyxGroupRelease(&result);
        return 0;
    }
    if (!sndPushGroup(result.project, group_id, samples, result.directory, result.pool)) {
        PartyboardMusyxSampleMemoryRemove(memory, samples);
        PartyboardMusyxGroupRelease(&result);
        return 0;
    }
    *group = result;
    return 1;
}

/* Original pop kills group voices before converter/sample registrations end.
 * Caller may free encoded samples only after this succeeds. */
static inline int PartyboardMusyxGroupPop(PartyboardMusyxGroup *group,
        PartyboardMusyxSampleMemory *memory, const uint8_t *samples) {
    if (!group || !memory || !group->project || !samples) return 0;
    if (!PartyboardMusyxSampleMemoryAvailable(memory, samples)) return 0;
    if (!sndPopGroup()) return 0;
    PartyboardMusyxSampleMemoryRemove(memory, samples);
    PartyboardMusyxGroupRelease(group);
    return 1;
}
#endif
