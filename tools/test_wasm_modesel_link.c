#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
extern int game_main(void);
static int (*volatile engine_entry)(void) = game_main;
int main(void) {
    assert(engine_entry);
    void *module = dlopen("/modeseldll.wasm", RTLD_NOW | RTLD_LOCAL);
    if (!module) fprintf(stderr, "Mode-select load failed: %s\n", dlerror());
    assert(module);
    assert(dlsym(module, "ObjectSetup"));
    assert(dlclose(module) == 0);
    puts("Mode-select link passed: actual engine imports resolved; setup was not executed.");
    return 0;
}
