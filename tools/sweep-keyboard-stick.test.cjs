const test=require('node:test'),assert=require('node:assert/strict');
const {sweepKeyboardStick}=require('./sweep-keyboard-stick.cjs');
test('ordinary sweep reaches all ticket rows and both horizontal bounds from any corner',async()=>{
 for(const initial of [[192,192],[384,192],[192,288],[384,288],[320,240]]){
  const held=new Set(),visited=new Set();let [x,y]=initial;
  await sweepKeyboardStick({hold:async(key,down)=>{if(down)held.add(key);else held.delete(key);},waitFrames:async count=>{
   assert.equal(held.size,1);
   for(let frame=0;frame<count;frame++){
    const key=[...held][0];x=Math.max(192,Math.min(384,x+(key==='a'?-8:key==='d'?8:0)));
    y=Math.max(192,Math.min(288,y+(key==='w'?-8:key==='s'?8:0)));visited.add(x+','+y);
   }
  }});
  for(let row=0;row<=12;row++)for(let column=0;column<=24;column++)assert.ok(visited.has((192+column*8)+','+(192+row*8)),'Missing scratch coordinate');
  assert.equal(held.size,0);
 }
});
test('observation failure releases the currently held key',async()=>{
 const held=new Set();let waits=0;
 await assert.rejects(sweepKeyboardStick({hold:async(key,down)=>{if(down)held.add(key);else held.delete(key);},waitFrames:async()=>{if(++waits===3)throw Error('Frame observation failed');}}),/Frame observation failed/);
 assert.equal(held.size,0);
});
test('cleanup retains observation and release failures',async()=>{
 await assert.rejects(sweepKeyboardStick({hold:async(key,down)=>{if(!down)throw Error('Release failed');},waitFrames:async()=>{throw Error('Observation failed');}}),error=>{
  assert.ok(error instanceof AggregateError);assert.deepEqual(error.errors.map(item=>item.message),['Observation failed','Release failed']);return true;
 });
});
