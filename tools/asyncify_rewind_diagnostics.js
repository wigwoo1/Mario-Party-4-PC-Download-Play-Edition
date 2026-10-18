// Opt-in observations for the pinned Emscripten runtime. Never calls Wasm.
Module['preRun'] = Module['preRun'] || [];
Module['preRun'].push(function () {
  const original = Asyncify.doRewind;
  const entries = [];
  Module['partyboardRewindDiagnostics'] = entries;
  Asyncify.doRewind = function (pointer) {
    if (globalThis.testAsyncifyRewindDiagnostics !== true)
      return original.apply(this, arguments);
    // V8 otherwise truncates the original Wasm trap at ten frames, before the
    // actual game caller below allocator/logging functions is visible.
    if (typeof Error.stackTraceLimit === 'number' && Error.stackTraceLimit < 64)
      Error.stackTraceLimit = 64;
    const valid = Number.isInteger(pointer) && pointer >= 0 && pointer + 12 <= HEAPU8.length;
    const words = valid ? Array.from(HEAPU32.subarray(pointer >>> 2, (pointer >>> 2) + 3)) : null;
    const bottom = words ? Asyncify.callStackIdToFunc.get(words[2]) : null;
    // The pinned dynCall_vi rewind pops two saved arguments and a call-site
    // marker from these final 12 bytes. Copy before the rewind mutates them.
    const stackPosition = words ? words[0] : 0;
    const tailValid = words && (stackPosition & 3) === 0 && stackPosition >= 12 &&
      stackPosition <= words[1] && stackPosition <= HEAPU8.length;
    const stackTail = tailValid ? Array.from(HEAPU32.subarray((stackPosition - 12) >>> 2, stackPosition >>> 2)) : null;
    const entry = {pointer, words, stackTail, state: Asyncify.state, bottom: bottom ? bottom.name : null,
      exportCallStack: Asyncify.exportCallStack.map(function (fn) { return fn.name; })};
    entries.push(entry);
    if (entries.length > 32) entries.shift();
    try { return original.apply(this, arguments); }
    catch (error) {
      entry.error = {name: error.name, message: error.message, stack: error.stack};
      console.error('Browser Asyncify rewind failure: ' + JSON.stringify(entry));
      throw error;
    }
  };
});
