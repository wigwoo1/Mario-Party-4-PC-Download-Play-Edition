#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
extern int game_main(void);
#ifdef TEST_OVERLAY_RESET
extern int PartyBoardOverlayStateCapture(void *);
extern int PartyBoardOverlayStateRestore(void *);
#endif
// Keep the real engine reachable; never execute it without initialization.
static int (*volatile engine_entry)(void) = game_main;
int main(void) {
    assert(engine_entry != NULL);
    void *module = dlopen("/bootDll.wasm", RTLD_NOW | RTLD_LOCAL);
    if (!module) fprintf(stderr, "Boot overlay load failed: %s\n", dlerror());
    assert(module);
    assert(dlsym(module, "ObjectSetup") != NULL);
#ifdef TEST_OVERLAY_RESET
    assert(PartyBoardOverlayStateCapture(module) == 1);
    assert(PartyBoardOverlayStateRestore(module) == 1);
    puts("Boot overlay private state captured and restored; setup was not executed.");
#endif
    assert(dlclose(module) == 0);
    puts("Boot overlay link passed: real engine imports resolved and ObjectSetup found; entry was not executed.");
    return 0;
}
