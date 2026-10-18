'use strict';
const test = require('node:test'), assert = require('node:assert/strict');
const {readPreparedBrowserProfile} = require('./prepared-browser-profile.cjs');
function fixture() {
  const layout = {gameStatSize:280, availabilityOffset:24, availabilityWords:2, presentOffset:210,
    presentCount:60, flagsOffset:270, unlockMask:52, continueMask:192};
  const heap = new Uint8Array(1024), pointer = 128;
  heap.fill(255, pointer+24, pointer+32); heap.fill(1, pointer+210, pointer+270); heap[pointer+270]=52;
  const calls = [];
  const module = {_GWGameStat:pointer, HEAPU8:heap, _PartyBoard_GameFrameCount:()=>0,
    _GWMGAvailGet:id=>{calls.push(id);return 1;}, _SLSaveFlagGet:()=>0};
  return {layout, calls, runtime:{module,checkpoint:123,engine:{phase:'Idle',preparedCheckpoint:()=>123}}};
}
test('serializable observer reads all actual ID ranges and preserves every heap byte',()=>{
  const f=fixture(), before=f.runtime.module.HEAPU8.slice();
  const callback=(0,eval)('('+readPreparedBrowserProfile.toString()+')');
  const report=callback(f);
  assert.equal(report.unlockedMinigameGetters,64); assert.equal(report.savingDisabled,true);
  assert.deepEqual(f.calls,Array.from({length:64},(_,i)=>401+i));
  assert.deepEqual(f.runtime.module.HEAPU8,before); assert.equal(report.normalBootScreensVerified,false);
});
test('uninitialized or running runtime is rejected before any production getter',()=>{
  for(const phase of ['New','Initializing','Running']) {
    const f=fixture(); f.runtime.engine.phase=phase;
    assert.throws(()=>readPreparedBrowserProfile(f),/prepared idle/); assert.equal(f.calls.length,0);
  }
});
test('locked bits, present and progression flags are not accepted',()=>{
  for(const offset of [24,210,270]) {
    const f=fixture();f.runtime.module.HEAPU8[128+offset]=0;
    assert.throws(()=>readPreparedBrowserProfile(f),/locked/);
  }
});
test('getter lock, continued save and enabled saving are not accepted',()=>{
  const f=fixture();f.runtime.module._GWMGAvailGet=id=>id===464?0:1;
  assert.throws(()=>readPreparedBrowserProfile(f),/getter locked: 464/);
  const continued=fixture();continued.runtime.module.HEAPU8[398]|=192;
  assert.throws(()=>readPreparedBrowserProfile(continued),/continue/);
  const saving=fixture();saving.runtime.module._SLSaveFlagGet=()=>1;
  assert.throws(()=>readPreparedBrowserProfile(saving),/Saving is enabled/);
});
test('invalid pointer and stale layout fail before minigame reads',()=>{
  const f=fixture();f.runtime.module._GWGameStat=1000;
  assert.throws(()=>readPreparedBrowserProfile(f),/profile address/);assert.equal(f.calls.length,0);
  const old=fixture();old.layout.flagsOffset++;
  assert.throws(()=>readPreparedBrowserProfile(old),/compiled Wasm profile layout/);
});
test('side effects from a getter cannot be reported as read-only',()=>{
  const f=fixture(); f.runtime.module._SLSaveFlagGet=()=>{f.runtime.module.HEAPU8[128]++;return 0;};
  assert.throws(()=>readPreparedBrowserProfile(f),/Profile changed/);
  const advanced=fixture();advanced.runtime.module._SLSaveFlagGet=()=>{advanced.runtime.engine.preparedCheckpoint=()=>124;return 0;};
  assert.throws(()=>readPreparedBrowserProfile(advanced),/Simulation changed/);
});
