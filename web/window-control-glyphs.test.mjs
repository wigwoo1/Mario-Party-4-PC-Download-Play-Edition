import {test} from 'node:test';
import assert from 'node:assert/strict';
import {windowControlGlyph} from './window-control-glyphs.mjs';
import {actionGlyph} from './control-glyphs.mjs';
test('original A bank resolves actual remapped input instead of literal A',()=>{
  const bindings={PAD_BUTTON_A:{kind:'key',label:'Z'}};
  const resolve=action=>actionGlyph(action,{bindings});
  assert.equal(windowControlGlyph(2,0,resolve).label,'Z');
  bindings.PAD_BUTTON_A={kind:'key',label:'Space'};
  assert.equal(windowControlGlyph(2,0,resolve).label,'Space');
  assert.equal(windowControlGlyph(3,0,resolve),null);
});
test('trigger animation phase survives while decorations and sentinels remain untouched',()=>{
  const resolve=action=>({label:action});
  assert.equal(windowControlGlyph(7,0,resolve).pressed,false);
  assert.equal(windowControlGlyph(7,1,resolve).pressed,true);
  assert.equal(windowControlGlyph(9,1,resolve).action,'PAD_TRIGGER_L');
  assert.equal(windowControlGlyph(7,2,resolve),null);
  for(const bank of [0,1,10,13,14,15,16,17,18,19,-1,2.5]) {
    assert.equal(windowControlGlyph(bank,0,()=>{throw Error('Unexpected replacement');}),null);
  }
});
