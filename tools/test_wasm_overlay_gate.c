#include "game/object.h"
#include <assert.h>
#include <stdio.h>
// Logging only; runtime implementation under test is the actual objdll.c.
void OSReport(const char *format, ...) { (void)format; }
static int starts, stops;
static s32 backend(s16 overlay, s32 start) {
    if (overlay != DLL_bootDll) return 0;
    if (start) starts++; else stops++;
    return 1;
}
int main(void) {
    omDllData *slot = (omDllData*)1;
    assert(omDLLStart(0, 0) == -1);
    assert(omDLLStart(-1, 1) == -1);
    assert(omDLLLink(&slot, 0, 1) == NULL && slot == NULL);
    assert(omDLLLink(NULL, 0, 1) == NULL);
    for (int i=0; i<OM_DLL_MAX; ++i) assert(omDLLinfoTbl[i] == NULL);
    puts("Browser overlay gate passed: unavailable loader rejects without allocating a slot or running console REL code.");
    FileListEntry files[DLL_MAX] = {0};
    files[DLL_bootDll].name = "boot";
    omDLLInit(files);
    omDLLBrowserSetBackend(backend);
    assert(omDLLStart(-1, 0) == -1 && omDLLStart(DLL_MAX, 0) == -1);
    assert(omDLLStart(DLL_modeseldll, 0) == -1);
    int loaded = omDLLStart(DLL_bootDll, 0);
    assert(loaded >= 0 && starts == 1 && omDLLinfoTbl[loaded]);
    assert(omDLLStart(DLL_bootDll, 0) == -1 && starts == 1);
    omDLLBrowserSetBackend(NULL); // Must not replace an active backend.
    omDLLEnd(loaded, 1);
    assert(stops == 1 && !omDLLinfoTbl[loaded]);
    loaded = omDLLStart(DLL_bootDll, 0);
    assert(loaded >= 0 && starts == 2);
    omDLLEnd(loaded, 0);
    assert(stops == 2 && !omDLLinfoTbl[loaded]);
    omDLLBrowserSetBackend(NULL);
    assert(omDLLStart(DLL_bootDll, 0) == -1);
    puts("Browser backend dispatch passed: opt-in setup/reset, duplicate rejection and re-entry.");
}
