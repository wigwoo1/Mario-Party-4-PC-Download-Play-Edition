'use strict';
const assert=require('node:assert/strict');
const {test}=require('node:test');
const {m427AlternatingStrokeKey,pressM432ObstacleInput,pressMarioMedleyInput,waitTwoGameFrames}=require('./drive-production-full-match.cjs');

test('two-frame wait predicate handles string frames as BigInt without mixed-number arithmetic',()=>{
 const previous=global.partyBoardApp;
 global.partyBoardApp={lobbyGameState:()=>({input:{state:'Running',frame:'18446744073709551618'}})};
 try{
  assert.equal(waitTwoGameFrames('18446744073709551616'),true);
  global.partyBoardApp={lobbyGameState:()=>({input:{state:'Running',frame:'18446744073709551617'}})};
  assert.equal(waitTwoGameFrames('18446744073709551616'),false);
  global.partyBoardApp={lobbyGameState:()=>({input:{state:'Paused',frame:'18446744073709551620'}})};
  assert.equal(waitTwoGameFrames('18446744073709551616'),false);
 }finally{
  if(previous===undefined)delete global.partyBoardApp;
  else global.partyBoardApp=previous;
 }
});

test('m427 test strokes alternate A and B and reject invalid counters',()=>{
 assert.deepEqual(Array.from({length:6},(_,index)=>m427AlternatingStrokeKey(index)),['z','x','z','x','z','x']);
 assert.throws(()=>m427AlternatingStrokeKey(-1),/nonnegative safe integer/);
});

function mockPage(events,{failWait=false}={}){
 return {
  evaluate:async()=>({frame:40,input:{frame:'40',state:'Running'}}),
  keyboard:{
   down:async key=>events.push('down:'+key),
   up:async key=>events.push('up:'+key),
  },
  waitForFunction:async(_predicate,frame,options)=>{
   events.push('wait:'+frame);
   assert.equal(options.timeout,120000);
   if(failWait)throw new Error('simulated wait failure');
  },
 };
}

test('Mario Medley presses every prompted face button together for two frames then releases',async()=>{
 const events=[];
 await pressMarioMedleyInput(mockPage(events),2);
 assert.equal(events.filter(event=>event.startsWith('down:')).length,4);
 assert.equal(events.filter(event=>event.startsWith('up:')).length,4);
 assert.ok(events.indexOf('wait:40')>events.findIndex(event=>event.startsWith('down:')));
 assert.ok(events.indexOf('wait:40')<events.findIndex(event=>event.startsWith('up:')));
 assert.deepEqual(new Set(events.filter(event=>event.startsWith('down:'))),new Set(['down:z','down:x','down:c','down:v']));
 assert.deepEqual(new Set(events.filter(event=>event.startsWith('up:'))),new Set(['up:z','up:x','up:c','up:v']));
});

test('Mario Medley releases held face buttons even if frame waiting fails',async()=>{
 const events=[];
 await assert.rejects(pressMarioMedleyInput(mockPage(events,{failWait:true}),0),/simulated wait failure/);
 assert.equal(events.filter(event=>event.startsWith('up:')).length,4);
});

test('m432 presses B/X/Y with the selected steering direction through two frames, then releases',async()=>{
 const events=[];
 await pressM432ObstacleInput(mockPage(events),1,'a');
 const downs=events.filter(event=>event.startsWith('down:'));
 const ups=events.filter(event=>event.startsWith('up:'));
 assert.deepEqual(new Set(downs),new Set(['down:x','down:c','down:v','down:ArrowLeft']));
 assert.deepEqual(new Set(ups),new Set(['up:x','up:c','up:v','up:ArrowLeft']));
 assert.ok(events.indexOf('wait:40')>events.findIndex(event=>event.startsWith('down:')));
 assert.ok(events.indexOf('wait:40')<events.findIndex(event=>event.startsWith('up:')));
});

test('m432 releases its obstacle chord after wait failure and rejects invalid steering keys',async()=>{
 const events=[];
 await assert.rejects(pressM432ObstacleInput(mockPage(events,{failWait:true}),0,'w'),/simulated wait failure/);
 assert.equal(events.filter(event=>event.startsWith('up:')).length,4);
 await assert.rejects(pressM432ObstacleInput(mockPage([]),0,'ArrowUp'),/WASD direction/);
});
