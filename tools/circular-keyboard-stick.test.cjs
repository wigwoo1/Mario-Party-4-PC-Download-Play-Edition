const test=require('node:test'),assert=require('node:assert/strict');
const {circularKeyboardStick}=require('./circular-keyboard-stick.cjs');
test('a rotation supplies eight non-neutral ordered angles over actual frame waits',async()=>{
 const held=new Set(),samples=[];
 await circularKeyboardStick({cycles:1,hold:async(key,down)=>{if(down)held.add(key);else held.delete(key);},
  waitFrames:async count=>{assert.equal(count,2);samples.push([...held].sort().join(''));}});
 assert.deepEqual(samples,['w','dw','d','ds','s','as','a','aw']);assert.equal(held.size,0);
});
test('a frame-wait failure releases all currently held directions',async()=>{
 const held=new Set();let waits=0;
 await assert.rejects(circularKeyboardStick({hold:async(key,down)=>{if(down)held.add(key);else held.delete(key);},
  waitFrames:async()=>{if(++waits===2)throw Error('Simulation observation failed');}}),/Simulation observation failed/);
 assert.equal(held.size,0);
});
test('cleanup attempts every key and preserves both observation and release errors',async()=>{
 const released=[];let waits=0;
 await assert.rejects(circularKeyboardStick({hold:async(key,down)=>{
  if(!down){released.push(key);throw Error('Release '+key);}
 },waitFrames:async()=>{if(++waits===2)throw Error('Original observation');}}),error=>{
  assert.ok(error instanceof AggregateError);
  assert.deepEqual(error.errors.map(item=>item.message),['Original observation','Release w','Release d']);return true;
 });
 assert.deepEqual(released,['w','d']);
});
