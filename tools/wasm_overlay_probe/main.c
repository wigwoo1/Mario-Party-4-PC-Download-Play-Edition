#include <assert.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <emscripten/version.h>
/* EXPERIMENT ONLY: prefix of the pinned SDK's internal struct dso.
 * Do not use this ABI as a production loader interface. */
#if __EMSCRIPTEN_MAJOR__ != 6 || __EMSCRIPTEN_MINOR__ != 0 || __EMSCRIPTEN_TINY__ != 10
#error Revalidate internal DSO layout for this SDK
#endif
typedef struct { void *event; int flags; uint8_t allocated; void *base; size_t size; } DsoPrefix;
typedef struct { void *base; size_t size; void *bytes; } Snapshot;
static Snapshot capture(void *handle) {
    DsoPrefix *dso=handle;
    assert(dso->allocated && dso->base && dso->size && dso->size < 65536);
    Snapshot s={dso->base,dso->size,malloc(dso->size)};
    assert(s.bytes); memcpy(s.bytes,s.base,s.size); return s;
}
typedef void (*Entry)(void);
typedef uint32_t (*Read)(void);
typedef int (*Draw)(void);
static void *open_module(const char *path) {
    void *handle=dlopen(path,RTLD_NOW|RTLD_LOCAL);
    if(!handle) fprintf(stderr,"%s\n",dlerror());
    assert(handle); return handle;
}
int main(void) {
    void *a=open_module("/side-a.wasm"), *b=open_module("/side-b.wasm");
    Snapshot baseline=capture(a);
    Entry setup_a=(Entry)dlsym(a,"ObjectSetup"), setup_b=(Entry)dlsym(b,"ObjectSetup");
    Read read_a=(Read)dlsym(a,"ReadState"), read_b=(Read)dlsym(b,"ReadState");
    Draw draw_a=(Draw)dlsym(a,"Draw");
    int *data_a=dlsym(a,"initialized_data"), *data_b=dlsym(b,"initialized_data");
    assert(setup_a && setup_b && read_a && read_b && draw_a && data_a && data_b);
    assert(read_a()==0 && read_b()==0 && *data_a==11 && *data_b==22);
    setup_a(); setup_b();
    assert(read_a()==11 && read_b()==22);
    draw_a();
    uint32_t changed=11u*0x41C64E6Du+0x3039u;
    assert(read_a()==changed && read_b()==22);
    *data_a=99; assert(*data_b==22);
    assert(dlclose(a)==0);
    a=open_module("/side-a.wasm");
    read_a=(Read)dlsym(a,"ReadState"); data_a=dlsym(a,"initialized_data");
    assert(read_a && data_a);
    /* Emscripten 6.0.10 dlclose validates the handle; it does not unload. */
    assert(read_a()==changed && *data_a==99);
    Entry mutate=(Entry)dlsym(a,"MutatePrivate");
    Draw check=(Draw)dlsym(a,"CheckPrivate");
    assert(mutate && check && check());
    for(int cycle=0;cycle<20;++cycle) {
        mutate(); assert(!check()); setup_a(); draw_a(); *data_a=99;
        memcpy(baseline.base,baseline.bytes,baseline.size);
        assert(check() && read_a()==0 && *data_a==11);
        assert(read_b()==22 && *data_b==22);
    }
    free(baseline.bytes);
    puts("Overlay reset passed: 20 cycles restore static data, BSS and relocated pointer without changing peer module.");
    puts("Overlay probe passed: same-named functions and data isolated; reload retains modified data/BSS.");
    puts("Overlay loader requires explicit state reset; dlclose is insufficient.");
    assert(dlclose(a)==0 && dlclose(b)==0);
    return 0;
}

