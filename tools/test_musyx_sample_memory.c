#include <assert.h>
#include "../src/port/musyx_sample_memory.h"
int main(void) {
    PartyboardMusyxSampleMemory memory = {0};
    uint8_t data[64];
    uint64_t generation;
    assert(PartyboardMusyxSampleMemoryAdd(&memory, data, sizeof(data)));
    generation=PartyboardMusyxSampleMemoryGeneration(&memory,data+33);
    assert(generation);
    assert(PartyboardMusyxSampleMemoryAvailable(&memory, data + 33) == 31);
    assert(!PartyboardMusyxSampleMemoryAvailable(&memory, data + 64));
    assert(!PartyboardMusyxSampleMemoryAdd(&memory, data + 1, 32));
    assert(PartyboardMusyxSampleMemoryAdd(&memory, data, sizeof(data)));
    assert(PartyboardMusyxSampleMemoryRemove(&memory, data));
    assert(PartyboardMusyxSampleMemoryGeneration(&memory,data)==generation);
    assert(PartyboardMusyxSampleMemoryAvailable(&memory, data) == 64);
    assert(PartyboardMusyxSampleMemoryRemove(&memory, data));
    assert(!PartyboardMusyxSampleMemoryAvailable(&memory, data));
    assert(!PartyboardMusyxSampleMemoryRemove(&memory, data));
    assert(!PartyboardMusyxSampleMemoryGeneration(&memory,data));
    assert(PartyboardMusyxSampleMemoryAdd(&memory,data,sizeof data));
    assert(PartyboardMusyxSampleMemoryGeneration(&memory,data)!=generation);
    assert(PartyboardMusyxSampleMemoryRemove(&memory,data));
    memory.next_generation=UINT64_MAX;
    assert(!PartyboardMusyxSampleMemoryAdd(&memory,data,sizeof data));
    return 0;
}
