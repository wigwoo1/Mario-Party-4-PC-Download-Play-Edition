// Experimental, pinned-SDK adapter. C_STRUCTS comes from Emscripten's generated
// layout metadata; never hard-code private dso offsets. Call capture immediately
// after dlopen, before ObjectSetup. Restore only after overlay teardown and all
// child coroutines have stopped. Resident modules must remain loaded.
addToLibrary({
  $partyboardOverlayStates: new Map(),
  PartyBoardOverlayStateCapture__deps: ['$partyboardOverlayStates'],
  PartyBoardOverlayStateCapture: function(handle) {
    if (!handle || partyboardOverlayStates.has(handle)) return 0;
    var base = {{{ makeGetValue('handle', C_STRUCTS.dso.mem_addr, '*') }}};
    var size = {{{ makeGetValue('handle', C_STRUCTS.dso.mem_size, 'i32') }}};
    if (!base || size <= 0 || size > 64 * 1024 * 1024 || base + size > HEAPU8.length) return 0;
    partyboardOverlayStates.set(handle, {base, size, bytes: HEAPU8.slice(base, base + size)});
    return 1;
  },
  PartyBoardOverlayStateRestore__deps: ['$partyboardOverlayStates'],
  PartyBoardOverlayStateRestore: function(handle) {
    var saved = partyboardOverlayStates.get(handle);
    if (!saved) return 0;
    var base = {{{ makeGetValue('handle', C_STRUCTS.dso.mem_addr, '*') }}};
    var size = {{{ makeGetValue('handle', C_STRUCTS.dso.mem_size, 'i32') }}};
    if (base !== saved.base || size !== saved.size || base + size > HEAPU8.length) return 0;
    HEAPU8.set(saved.bytes, base);
    return 1;
  }
});
