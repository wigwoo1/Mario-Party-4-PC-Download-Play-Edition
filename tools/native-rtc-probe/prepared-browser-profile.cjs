'use strict';
// Serializable Playwright callback: observe only a normally prepared runtime.
// Never reset a synchronized peer or call profile functions before platform setup.
function readPreparedBrowserProfile({runtime, layout}) {
  const check = (ok, message) => { if (!ok) throw Error(message); };
  check(runtime?.engine?.phase === 'Idle', 'Normally prepared idle runtime required');
  const module = runtime.module;
  for (const name of ['_PartyBoard_GameFrameCount', '_GWMGAvailGet', '_SLSaveFlagGet'])
    check(typeof module?.[name] === 'function', 'Missing production getter ' + name);
  check(module._PartyBoard_GameFrameCount() === 0, 'Fresh prepared frame zero required');
  check(typeof runtime.engine.preparedCheckpoint === 'function', 'Prepared checkpoint required');
  const checkpoint = runtime.engine.preparedCheckpoint();
  check(checkpoint === runtime.checkpoint, 'Prepared checkpoint changed before profile observation');
  check(layout?.gameStatSize === 280 && layout.availabilityOffset === 24 && layout.availabilityWords === 2 &&
    layout.presentOffset === 210 && layout.presentCount === 60 && layout.flagsOffset === 270 &&
    layout.unlockMask === 52 && layout.continueMask === 192, 'Current compiled Wasm profile layout required');
  const pointer = module._GWGameStat, heap = module.HEAPU8;
  check(heap instanceof Uint8Array && Number.isSafeInteger(pointer) && pointer > 0 &&
    pointer + layout.gameStatSize <= heap.length, 'Invalid production profile address');
  const before = Array.from(heap.subarray(pointer, pointer + layout.gameStatSize));
  const view = new DataView(heap.buffer, heap.byteOffset, heap.byteLength);
  const availabilityWords = [0,1].map(index => view.getUint32(pointer + layout.availabilityOffset + index * 4, true));
  check(availabilityWords.every(word => word === 0xffffffff), 'A minigame availability bit remains locked');
  for (let id = 401; id <= 464; id++) check(module._GWMGAvailGet(id) === 1, 'Minigame getter locked: ' + id);
  check(before.slice(layout.presentOffset, layout.presentOffset + layout.presentCount).every(value => value === 1), 'A present remains locked');
  const flags = before[layout.flagsOffset];
  check((flags & layout.unlockMask) === layout.unlockMask, 'Bonus board, difficulty or music remains locked');
  check((flags & layout.continueMask) === 0, 'A saved-game continue flag remains enabled');
  check(module._SLSaveFlagGet() === 0, 'Saving is enabled');
  check(before.every((value,index) => module.HEAPU8[pointer+index] === value), 'Profile changed during read-only observation');
  check(module._PartyBoard_GameFrameCount() === 0 && runtime.engine.preparedCheckpoint() === checkpoint,
    'Simulation changed during profile observation');
  return {frame:0, checkpoint, availabilityWords, unlockedMinigameGetters:64, unlockedPresents:60,
    bonusBoardDifficultyMusicUnlocked:true, continueFlagsCleared:true, savingDisabled:true,
    profileBytesUnchanged:true, preparedCheckpointUnchanged:true, normalBootScreensVerified:false};
}
module.exports = {readPreparedBrowserProfile};
