#ifndef PARTYBOARD_MUSYX_GROUP_H
#define PARTYBOARD_MUSYX_GROUP_H
#include "musyx_sample_directory.h"
#include "musyx_project.h"
typedef struct {
    uint8_t *project;
    uint8_t *pool;
    SDIR_DATA *directory;
} PartyboardMusyxGroup;
/* Only release after original sndPopGroup/unregistration and voice teardown.
 * Caller retains the encoded sample blob independently. */
static inline void PartyboardMusyxGroupRelease(PartyboardMusyxGroup *group) {
    free(group->directory);
    free(group->project);
    free(group->pool);
    memset(group, 0, sizeof(*group));
}
/* Destination must be empty. Failed conversion leaves it unchanged. */
static inline int PartyboardMusyxGroupConvert(PartyboardMusyxGroup *destination,
        const uint8_t *raw, size_t bytes, const uint8_t *samples, size_t sample_bytes) {
    PartyboardMusyxGroup result = {0};
    size_t project, pool;
    if (!destination || destination->project || destination->pool || destination->directory
            || !raw || bytes < 16) return 0;
    pool = PartyboardMsmU32(raw);
    project = PartyboardMsmU32(raw + 4);
    if (pool < 16 || project < 16 || pool > bytes || project > bytes) return 0;
    result.directory = PartyboardMusyxSampleDirectoryConvert(raw, bytes, samples, sample_bytes);
    if (!result.directory) goto failure;
    result.pool = PartyboardMusyxPoolConvert(raw + pool, bytes - pool);
    if (!result.pool) goto failure;
    result.project = PartyboardMusyxProjectConvert(raw + project, bytes - project);
    if (!result.project) goto failure;
    *destination = result;
    return 1;
failure:
    PartyboardMusyxGroupRelease(&result);
    return 0;
}
#endif
