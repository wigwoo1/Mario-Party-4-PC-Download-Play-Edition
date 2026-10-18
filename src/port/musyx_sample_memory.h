#ifndef PARTYBOARD_MUSYX_SAMPLE_MEMORY_H
#define PARTYBOARD_MUSYX_SAMPLE_MEMORY_H
#include <stdint.h>
#include <stddef.h>
/* Track caller-owned encoded buffers so original voice SAMPLE_INFO pointers
 * can resolve a real available byte extent without guessing from format. */
typedef struct {
    struct { const uint8_t *base; size_t bytes; unsigned references; uint64_t generation; } buffer[128];
    uint64_t next_generation;
} PartyboardMusyxSampleMemory;
static inline int PartyboardMusyxSampleMemoryAdd(PartyboardMusyxSampleMemory *memory,
        const uint8_t *base, size_t bytes) {
    size_t i, free_slot = 128;
    uintptr_t address = (uintptr_t)base;
    if (!base || !bytes || bytes > UINTPTR_MAX - address) return 0;
    for (i = 0; i < 128; ++i) {
        uintptr_t current = (uintptr_t)memory->buffer[i].base;
        if (!memory->buffer[i].references) { if (free_slot == 128) free_slot = i; continue; }
        if (current == address && memory->buffer[i].bytes == bytes) {
            if (memory->buffer[i].references == UINT32_MAX) return 0;
            ++memory->buffer[i].references;
            return 1;
        }
        if (address < current + memory->buffer[i].bytes && current < address + bytes) return 0;
    }
    if (free_slot == 128 || memory->next_generation == UINT64_MAX) return 0;
    memory->buffer[free_slot].base = base;
    memory->buffer[free_slot].bytes = bytes;
    memory->buffer[free_slot].references = 1;
    memory->buffer[free_slot].generation = ++memory->next_generation;
    return 1;
}
static inline int PartyboardMusyxSampleMemoryRemove(PartyboardMusyxSampleMemory *memory,
        const uint8_t *base) {
    size_t i;
    for (i = 0; i < 128; ++i) {
        if (memory->buffer[i].references && memory->buffer[i].base == base) {
            if (!--memory->buffer[i].references) {
                memory->buffer[i].base = NULL; memory->buffer[i].bytes = 0;
            }
            return 1;
        }
    }
    return 0;
}
static inline size_t PartyboardMusyxSampleMemoryAvailable(const PartyboardMusyxSampleMemory *memory,
        const void *sample) {
    size_t i;
    uintptr_t address = (uintptr_t)sample;
    for (i = 0; i < 128; ++i) {
        uintptr_t base = (uintptr_t)memory->buffer[i].base;
        if (memory->buffer[i].references && address >= base && address - base < memory->buffer[i].bytes)
            return memory->buffer[i].bytes - (size_t)(address - base);
    }
    return 0;
}
/* Retains identity for duplicate references, changes after final removal. */
static inline uint64_t PartyboardMusyxSampleMemoryGeneration(const PartyboardMusyxSampleMemory *memory,
        const void *sample) {
    size_t i;
    uintptr_t address=(uintptr_t)sample;
    if(!memory) return 0;
    for(i=0;i<128;++i) {
        uintptr_t base=(uintptr_t)memory->buffer[i].base;
        if(memory->buffer[i].references && address>=base
                && address-base<memory->buffer[i].bytes) return memory->buffer[i].generation;
    }
    return 0;
}
#endif
