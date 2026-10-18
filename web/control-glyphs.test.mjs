import {test} from 'node:test';
import assert from 'node:assert/strict';
import {actionGlyph,controlGlyph} from './control-glyphs.mjs';
test('same physical button uses local family labels',()=>{
  const binding={kind:'button',button:'south'};
  assert.equal(controlGlyph({family:'xbox',binding}).label,'A');
  assert.equal(controlGlyph({family:'switch',binding}).label,'B');
  assert.equal(controlGlyph({family:'playstation',binding}).label,'Cross');
});
test('remapped action uses actual binding and escapes keyboard labels',()=>{
  const bindings={confirm:{kind:'key',label:'Enter'},cancel:{kind:'key',label:'<&"'}};
  assert.equal(actionGlyph('confirm',{bindings}).label,'Enter');
  bindings.confirm={kind:'key',label:'Space'};
  assert.equal(actionGlyph('confirm',{bindings}).label,'Space');
  assert.ok(actionGlyph('cancel',{bindings}).svg.includes('&lt;&amp;&quot;'));
  assert.equal(actionGlyph('missing',{bindings}),null);
  assert.throws(()=>controlGlyph({family:'unknown',binding:{kind:'button',button:'south'}}));
});
