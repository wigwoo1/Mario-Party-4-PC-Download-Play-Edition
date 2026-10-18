#ifndef PARTYBOARD_MSM_CONTAINER_H
#define PARTYBOARD_MSM_CONTAINER_H
#include "msm_dsp_sample.h"

enum { PARTYBOARD_MSM_INFO = 1, PARTYBOARD_MSM_AUX = 2, PARTYBOARD_MSM_GROUPS = 3,
       PARTYBOARD_MSM_SONGS = 4, PARTYBOARD_MSM_EFFECTS = 5,
       PARTYBOARD_MSM_GROUP_DATA = 6, PARTYBOARD_MSM_SAMPLES = 7 };
typedef struct {
    const uint8_t *data;
    size_t bytes;
    struct { uint32_t offset, bytes; } section[10];
} PartyboardMsmContainer;
typedef struct {
    unsigned id;
    int stack, subgroup;
    const uint8_t *data, *samples;
    size_t data_bytes, sample_bytes;
} PartyboardMsmContainerGroup;
typedef struct {
    unsigned voices, music, effects, groups, songs, sound_effects;
    unsigned music_channels, stack_a, stack_b, surround;
    int aux_a, aux_b;
    uint32_t minimum_memory, sample_memory, group_buffer_a, group_buffer_b, dummy_music_bytes;
    unsigned base_count;
    uint8_t base_groups[23];
} PartyboardMsmContainerInfo;

typedef struct {
    unsigned type;
    /* Host-endian words. Reverb words 1..6 are IEEE float bits; chorus
     * and delay words are integers. Preserve bits until callback setup. */
    uint32_t parameter[9];
} PartyboardMsmContainerAux;

typedef struct {
    unsigned group, effect;
    int volume, pan, pitch_bend, emitter, emitter_component;
    unsigned span, reverb, chorus;
} PartyboardMsmContainerEffect;

typedef struct {
    unsigned group, song;
    uint32_t offset, bytes;
    int song_group, volume;
} PartyboardMsmContainerSong;

static inline int PartyboardMsmContainerGetSong(PartyboardMsmContainerSong *destination,
        const PartyboardMsmContainer *container, size_t index) {
    PartyboardMsmContainerSong result = {0};
    const uint8_t *entry;
    if (!destination || !container || !container->data
            || container->section[PARTYBOARD_MSM_SONGS].bytes % 16
            || index >= container->section[PARTYBOARD_MSM_SONGS].bytes / 16) return 0;
    entry = container->data + container->section[PARTYBOARD_MSM_SONGS].offset + index * 16;
    result.group = PartyboardMsmU16(entry); result.song = PartyboardMsmU16(entry + 2);
    result.offset = PartyboardMsmU32(entry + 4); result.bytes = PartyboardMsmU32(entry + 8);
    result.song_group = entry[12] < 128 ? entry[12] : (int)entry[12] - 256;
    result.volume = entry[13] < 128 ? entry[13] : (int)entry[13] - 256;
    *destination = result;
    return 1;
}

static inline int PartyboardMsmContainerGetEffect(PartyboardMsmContainerEffect *destination,
        const PartyboardMsmContainer *container, size_t index) {
    PartyboardMsmContainerEffect result = {0};
    const uint8_t *entry;
    unsigned pitch;
    if (!destination || !container || !container->data
            || container->section[PARTYBOARD_MSM_EFFECTS].bytes % 16
            || index >= container->section[PARTYBOARD_MSM_EFFECTS].bytes / 16) return 0;
    entry = container->data + container->section[PARTYBOARD_MSM_EFFECTS].offset + index * 16;
    result.group = PartyboardMsmU16(entry); result.effect = PartyboardMsmU16(entry + 2);
    result.volume = entry[4] < 128 ? entry[4] : (int)entry[4] - 256;
    result.pan = entry[5] < 128 ? entry[5] : (int)entry[5] - 256;
    pitch = PartyboardMsmU16(entry + 6);
    result.pitch_bend = pitch < 32768 ? (int)pitch : (int)pitch - 65536;
    result.span = entry[8]; result.reverb = entry[9]; result.chorus = entry[10];
    result.emitter = entry[11] < 128 ? entry[11] : (int)entry[11] - 256;
    result.emitter_component = entry[12] < 128 ? entry[12] : (int)entry[12] - 256;
    *destination = result;
    return 1;
}

static inline int PartyboardMsmContainerGetAux(PartyboardMsmContainerAux *destination,
        const PartyboardMsmContainer *container, size_t index) {
    PartyboardMsmContainerAux result = {0};
    const uint8_t *entry;
    unsigned i, count;
    if (!destination || !container || !container->data
            || container->section[PARTYBOARD_MSM_AUX].bytes % 40
            || index >= container->section[PARTYBOARD_MSM_AUX].bytes / 40) return 0;
    entry = container->data + container->section[PARTYBOARD_MSM_AUX].offset + index * 40;
    result.type = entry[0];
    if (result.type > 3) return 0;
    count = result.type < 2 ? 7 : result.type == 2 ? 3 : 9;
    for (i = 0; i < count; ++i) result.parameter[i] = PartyboardMsmU32(entry + 4 + i * 4);
    *destination = result;
    return 1;
}

/* Decode serialized metadata explicitly; never cast console structures into
 * host structs. Caller retains the original private mapped file. */
static inline int PartyboardMsmContainerOpen(PartyboardMsmContainer *destination,
        const uint8_t *data, size_t bytes) {
    PartyboardMsmContainer result = {0};
    unsigned i;
    if (!destination || !data || bytes < 96 || PartyboardMsmU32(data + 4) != 2) return 0;
    result.data = data; result.bytes = bytes;
    for (i = 0; i < 10; ++i) {
        uint32_t offset = PartyboardMsmU32(data + 8 + i * 8);
        uint32_t length = PartyboardMsmU32(data + 12 + i * 8);
        if (offset > bytes || length > bytes - offset) return 0;
        result.section[i].offset = offset; result.section[i].bytes = length;
    }
    if (result.section[PARTYBOARD_MSM_INFO].bytes < 64
            || result.section[PARTYBOARD_MSM_GROUPS].bytes % 32) return 0;
    *destination = result;
    return 1;
}

static inline int PartyboardMsmContainerGetInfo(PartyboardMsmContainerInfo *destination,
        const PartyboardMsmContainer *container) {
    PartyboardMsmContainerInfo result = {0};
    const uint8_t *info;
    unsigned i;
    if (!destination || !container || !container->data
            || container->section[PARTYBOARD_MSM_INFO].bytes < 64) return 0;
    info = container->data + container->section[PARTYBOARD_MSM_INFO].offset;
    result.voices = info[0]; result.music = info[1]; result.effects = info[2];
    result.groups = info[3]; result.songs = PartyboardMsmU16(info + 4);
    result.sound_effects = PartyboardMsmU16(info + 6);
    result.music_channels = info[8]; result.stack_a = info[9];
    result.stack_b = info[10]; result.surround = info[11];
    result.aux_a = info[14] < 128 ? info[14] : (int)info[14] - 256;
    result.aux_b = info[15] < 128 ? info[15] : (int)info[15] - 256;
    result.minimum_memory = PartyboardMsmU32(info + 16);
    result.sample_memory = PartyboardMsmU32(info + 20);
    result.group_buffer_a = PartyboardMsmU32(info + 24);
    result.group_buffer_b = PartyboardMsmU32(info + 28);
    result.dummy_music_bytes = PartyboardMsmU32(info + 32);
    result.base_count = info[40];
    if (result.base_count > 23 || result.groups > container->section[PARTYBOARD_MSM_GROUPS].bytes / 32)
        return 0;
    for (i = 0; i < result.base_count; ++i) {
        unsigned j;
        if (info[41 + i] >= result.groups) return 0;
        for (j = 0; j < i; ++j) if (result.base_groups[j] == info[41 + i]) return 0;
        result.base_groups[i] = info[41 + i];
    }
    *destination = result;
    return 1;
}

static inline int PartyboardMsmContainerGetGroup(PartyboardMsmContainerGroup *destination,
        const PartyboardMsmContainer *container, size_t index) {
    PartyboardMsmContainerGroup result = {0};
    const uint8_t *entry;
    uint32_t offset, length;
    if (!destination || !container || !container->data
            || index >= container->section[PARTYBOARD_MSM_GROUPS].bytes / 32) return 0;
    entry = container->data + container->section[PARTYBOARD_MSM_GROUPS].offset + index * 32;
    result.id = PartyboardMsmU16(entry);
    result.stack = entry[2] < 128 ? entry[2] : (int)entry[2] - 256;
    result.subgroup = entry[3] < 128 ? entry[3] : (int)entry[3] - 256;
    offset = PartyboardMsmU32(entry + 4); length = PartyboardMsmU32(entry + 8);
    if (offset > container->section[PARTYBOARD_MSM_GROUP_DATA].bytes
            || length > container->section[PARTYBOARD_MSM_GROUP_DATA].bytes - offset) return 0;
    result.data = container->data + container->section[PARTYBOARD_MSM_GROUP_DATA].offset + offset;
    result.data_bytes = length;
    offset = PartyboardMsmU32(entry + 12); length = PartyboardMsmU32(entry + 16);
    if (offset > container->section[PARTYBOARD_MSM_SAMPLES].bytes
            || length > container->section[PARTYBOARD_MSM_SAMPLES].bytes - offset) return 0;
    result.samples = container->data + container->section[PARTYBOARD_MSM_SAMPLES].offset + offset;
    result.sample_bytes = length;
    *destination = result;
    return 1;
}
/* Locates serialized sequence bytes only. Runtime still must require its
 * synthesis group and arrangement group to be loaded before starting music. */
static inline int PartyboardMsmContainerResolveSong(const uint8_t **destination,
        size_t *destination_bytes, const PartyboardMsmContainer *container,
        const PartyboardMsmContainerSong *song) {
    const uint8_t *base;
    size_t available, offset;
    if (!destination || !destination_bytes || !container || !container->data || !song
            || song->group == 65535 || !song->bytes) return 0;
    if (song->song_group < 0) {
        base = container->data + container->section[8].offset;
        available = container->section[8].bytes;
    } else {
        PartyboardMsmContainerGroup group;
        if (!PartyboardMsmContainerGetGroup(&group, container, (size_t)song->song_group)
                || group.data_bytes < 16) return 0;
        offset = PartyboardMsmU32(group.data + 12);
        if (offset > group.data_bytes) return 0;
        base = group.data + offset; available = group.data_bytes - offset;
    }
    if (song->offset > available || song->bytes > available - song->offset) return 0;
    *destination = base + song->offset; *destination_bytes = song->bytes;
    return 1;
}
#endif
