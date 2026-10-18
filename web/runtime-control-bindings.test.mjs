import test from 'node:test';import assert from 'node:assert/strict';
import {applyRuntimeKeyboardBindings,applyRuntimeControllerBindings} from './runtime-control-bindings.mjs';import {defaultControlBindings} from './control-bindings.mjs';
test('controller bridge packs invalid-button sentinel and frees memory when device is absent',()=>{
 let freed=0;const module={HEAP32:new Int32Array(100),get HEAPU8(){return new Uint8Array(this.HEAP32.buffer);},_malloc:()=>4,_free:()=>freed++, _PartyBoardBrowserApplyControllerBindings:(pointer,count)=>{
  assert.equal(count,64);assert.equal(module.HEAP32[1],0);assert.equal(module.HEAP32[9],-1);return -3;
 }};
 assert.throws(()=>applyRuntimeControllerBindings(module,defaultControlBindings()),/Connect your controller/);assert.equal(freed,1);
});
test('binding bridge delivers complete typed engine records and frees memory on rejection',()=>{
 let freed=0;const module={HEAP32:new Int32Array(100),get HEAPU8(){return new Uint8Array(this.HEAP32.buffer);},_malloc:()=>4,_free:p=>{assert.equal(p,4);freed++;},_PartyBoardBrowserApplyKeyboardBindings:(pointer,count)=>{assert.equal(count,54);assert.equal(module.HEAP32[pointer/4],29);return 0;}};
 applyRuntimeKeyboardBindings(module,defaultControlBindings());assert.equal(freed,1);
 module._PartyBoardBrowserApplyKeyboardBindings=()=>-1;assert.throws(()=>applyRuntimeKeyboardBindings(module,defaultControlBindings()),/rejected/);assert.equal(freed,2);
 const invalid=defaultControlBindings();invalid.keyboard.a='KeyX';assert.throws(()=>applyRuntimeKeyboardBindings(module,invalid),/conflict/);assert.equal(freed,2);
});
