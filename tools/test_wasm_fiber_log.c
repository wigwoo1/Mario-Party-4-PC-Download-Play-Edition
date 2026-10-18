#include "libco.h"
#include "game/memory.h"
#include <dolphin/os.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <emscripten/stack.h>
extern uintptr_t PartyBoardBrowserHeapFailureWord(unsigned index);
static cothread_t parent;
static void *heap;
static int complete;
static void child(void) {
    assert(emscripten_stack_get_current() % 16 == 0);
    OSReport("Fiber log: first %d\n", 1);
    OSReport("Fiber log: second line\n");
    void *a=HuMemMemoryAlloc(heap,128,0), *b=HuMemMemoryAlloc(heap,256,0);
    assert(a && b);
    HuMemHeapDump(heap,-1);
    HuMemMemoryFree(a,0); HuMemMemoryFree(b,0);
    HuMemHeapDump(heap,-1);
    assert(HuMemMemoryAlloc(heap,65536,0x1234) == NULL);
    assert(PartyBoardBrowserHeapFailureWord(0) == 1);
    assert(PartyBoardBrowserHeapFailureWord(1) == (uintptr_t)heap);
    assert(PartyBoardBrowserHeapFailureWord(2) == 65536);
    assert(PartyBoardBrowserHeapFailureWord(8) == 0x1234);
    assert(PartyBoardBrowserHeapFailureWord(9) == 0);
    complete=1;
    co_switch(parent);
    abort();
}
int main(void) {
    heap=aligned_alloc(32,65536); assert(heap);
    HuMemHeapInit(heap,65536);
    OSReport("Fiber log: parent before switch\n");
    parent=co_active();
    const unsigned sizes[] = {8192, 65537};
    for (unsigned i=0; i<2; ++i) {
        complete=0;
        cothread_t fiber=co_create(sizes[i],child); assert(fiber);
        co_switch(fiber); assert(complete);
        co_delete(fiber);
    }
    free(heap);
    puts("Fiber logger passed: repeated OSReport and actual heap traversal on a coroutine.");
}
