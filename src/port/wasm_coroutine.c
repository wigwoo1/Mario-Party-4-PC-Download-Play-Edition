/* Experimental single-thread libco adapter for the browser game probes.
 * Compile instead of libco.c with Emscripten -sASYNCIFY. */
#include "libco.h"
#include <emscripten/fiber.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <emscripten.h>
#include "wasm_stack_guard.h"

typedef struct WasmCoroutine {
    emscripten_fiber_t fiber;
    void *c_stack, *async_stack;
    void *c_allocation, *async_allocation;
    unsigned c_size;
    void (*entry)(void);
} WasmCoroutine;
static WasmCoroutine primary;
static WasmCoroutine *current;
enum { MinimumStack = 65536, AsyncStack = 65536 };

static void check_stacks(WasmCoroutine *thread) {
    unsigned c_damage = thread->c_allocation ? PartyBoardStackGuardDamage(thread->c_allocation, thread->c_size) : 0;
    unsigned async_damage = thread->async_allocation ? PartyBoardStackGuardDamage(thread->async_allocation, AsyncStack) : 0;
    if (c_damage || async_damage) {
        EM_ASM({ console.error('Browser coroutine stack guard damaged: ' + JSON.stringify({thread:$0,cDamage:$1,asyncDamage:$2,cSize:$3})); }, thread, c_damage, async_damage, thread->c_size);
        abort();
    }
}

cothread_t co_active(void) {
    if (!current) {
        primary.async_allocation = aligned_alloc(16, AsyncStack + 2 * PartyBoardStackGuardBytes);
        if (!primary.async_allocation) abort();
        PartyBoardStackGuardInit(primary.async_allocation, AsyncStack);
        primary.async_stack = (unsigned char *)primary.async_allocation + PartyBoardStackGuardBytes;
        emscripten_fiber_init_from_current_context(&primary.fiber, primary.async_stack, AsyncStack);
        current = &primary;
    }
    return current;
}
static void enter(void *argument) {
    WasmCoroutine *thread = argument;
    thread->entry();
    abort(); /* Returning a fiber entry would otherwise end the entire program. */
}
cothread_t co_create(unsigned int size, void (*entry)(void)) {
    if (!entry) return NULL;
    co_active();
    WasmCoroutine *thread = calloc(1, sizeof(*thread));
    if (!thread) return NULL;
    if (size < MinimumStack) size = MinimumStack;
    /* Fiber initialization uses the supplied end address verbatim. The Wasm
     * ABI requires 16-byte stack alignment, beyond malloc's guarantee here. */
    if (size > UINT32_MAX - 15u - 2 * PartyBoardStackGuardBytes) { free(thread); return NULL; }
    size = (size + 15u) & ~15u;
    thread->c_allocation = aligned_alloc(16, size + 2 * PartyBoardStackGuardBytes);
    thread->async_allocation = aligned_alloc(16, AsyncStack + 2 * PartyBoardStackGuardBytes);
    if (!thread->c_allocation || !thread->async_allocation) {
        free(thread->c_allocation); free(thread->async_allocation); free(thread); return NULL;
    }
    thread->c_size = size;
    PartyBoardStackGuardInit(thread->c_allocation, size);
    PartyBoardStackGuardInit(thread->async_allocation, AsyncStack);
    thread->c_stack = (unsigned char *)thread->c_allocation + PartyBoardStackGuardBytes;
    thread->async_stack = (unsigned char *)thread->async_allocation + PartyBoardStackGuardBytes;
    thread->entry = entry;
    emscripten_fiber_init(&thread->fiber, enter, thread, thread->c_stack, size,
                          thread->async_stack, AsyncStack);
    return thread;
}
void co_switch(cothread_t handle) {
    WasmCoroutine *target = handle, *previous = co_active();
    assert(target);
    check_stacks(previous);
    check_stacks(target);
    if (target == previous) return;
    current = target;
    emscripten_fiber_swap(&previous->fiber, &target->fiber);
    check_stacks(previous);
    assert(current == previous);
}
void co_delete(cothread_t handle) {
    WasmCoroutine *thread = handle;
    if (!thread) return;
    assert(thread != &primary && thread != current);
    check_stacks(thread);
    free(thread->c_allocation); free(thread->async_allocation); free(thread);
}
/* Unused by the game scheduler. Caller-provided stack storage is unsupported. */
cothread_t co_derive(void *memory, unsigned int size, void (*entry)(void)) {
    (void)memory; (void)size; (void)entry; return NULL;
}
int co_serializable(void) { return 0; }
