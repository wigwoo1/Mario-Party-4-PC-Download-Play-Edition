#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#ifndef EXPECTED_RELOAD_STATE
#define EXPECTED_RELOAD_STATE 42
#endif
#ifdef TEST_EXPLICIT_RESET
extern int PartyBoardOverlayStateCapture(void *);
extern int PartyBoardOverlayStateRestore(void *);
#endif

int main(void) {
    volatile int engine_guard = 91;
    void *handle = dlopen("overlay-state.wasm", RTLD_NOW | RTLD_LOCAL);
    assert(handle);
    int (*read_state)(void) = dlsym(handle, "read_state");
    void (*write_state)(int) = dlsym(handle, "write_state");
    assert(read_state && write_state);
    assert(read_state() == 7);
    assert(engine_guard == 91);
#ifdef TEST_EXPLICIT_RESET
    assert(PartyBoardOverlayStateCapture(handle) == 1);
    assert(PartyBoardOverlayStateCapture(handle) == 0);
#endif
    write_state(42);
    assert(dlclose(handle) == 0);
    handle = dlopen("overlay-state.wasm", RTLD_NOW | RTLD_LOCAL);
    assert(handle);
    read_state = dlsym(handle, "read_state");
    assert(read_state);
    // These modules are linked into MAIN_MODULE as resident dependencies.
    // Do not assume dlclose/dlopen resets their static game state.
    printf("Overlay reopened with state %d (expected %d)\n", read_state(), EXPECTED_RELOAD_STATE);
    assert(read_state() == EXPECTED_RELOAD_STATE);
#ifdef TEST_EXPLICIT_RESET
    assert(PartyBoardOverlayStateRestore(handle) == 1);
    assert(read_state() == 7);
    assert(engine_guard == 91);
    assert(PartyBoardOverlayStateRestore(NULL) == 0);
    puts("PASS: explicit module-memory restore reset fixture static state.");
#endif
    assert(dlclose(handle) == 0);
    puts("PASS: overlay reload state matches the tested lifecycle expectation.");
    return 0;
}
