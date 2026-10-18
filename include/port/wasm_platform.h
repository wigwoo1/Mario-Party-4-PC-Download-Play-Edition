#pragma once
#ifdef __cplusplus
extern "C" {
#endif
// One platform per Wasm instance. Asset verification and mounting belong to JS.
// Returns 0 on success, -1 on invalid lifecycle, -2 on renderer failure,
// -3 on disc open failure. Does not initialize or advance the game engine.
int PartyBoardBrowserPreparePlatform(void);
// Renderer work only, before the first game frame; no shader coverage promise.
int PartyBoardBrowserAdvancePreparedRenderer(void);
unsigned int PartyBoardBrowserPipelineCount(int created);
// Caller must first stop/await engine execution. Idempotent after release.
int PartyBoardBrowserShutdownPlatform(void);
#ifdef __cplusplus
}
#endif
