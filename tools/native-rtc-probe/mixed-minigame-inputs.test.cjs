'use strict';
const {test}=require('node:test'),assert=require('node:assert/strict');
const {pressMixedMarioMedleyInput,pressMixedM432ObstacleInput}=require('./mixed-minigame-inputs.cjs');
const {createBoundMatchKeyboard,browserMatchMapping}=require('./bound-match-keyboard.cjs');
function fixture({failedDown,failedUp}={}){
 const events=[],held=new Set(),peer={keyboard:{down:async key=>{events.push(['down',key]);held.add(key);if(key===failedDown)throw Error('down failure');},up:async key=>{events.push(['up',key]);held.delete(key);if(key===failedUp)throw Error('up failure');}}};
 return {events,held,peer};
}
test('Mario Medley holds the required face-button chord through two actual frame waits and releases every edge',async()=>{
 const f=fixture();await pressMixedMarioMedleyInput(f.peer,async(peer,frames)=>{assert.equal(peer,f.peer);assert.equal(frames,2);assert.deepEqual([...f.held].sort(),['c','v','x','z']);});
 assert.equal(f.held.size,0);assert.equal(f.events.filter(([type])=>type==='up').length,4);
});
test('m432 supplies all obstacle response buttons while steering and releases them when frame waiting fails',async()=>{
 const f=fixture();await assert.rejects(pressMixedM432ObstacleInput(f.peer,async(_peer,frames)=>{assert.equal(frames,2);assert.deepEqual([...f.held].sort(),['d','c','v','x'].sort());throw Error('simulation stopped');},'d'),/simulation stopped/);
 assert.equal(f.held.size,0);assert.equal(f.events.filter(([type])=>type==='up').length,4);
});
test('a failed key-down still releases every candidate key after concurrent key-downs settle',async()=>{
 const f=fixture({failedDown:'c'});let waits=0;await assert.rejects(pressMixedM432ObstacleInput(f.peer,async()=>waits++),/down failure/);assert.equal(waits,0);assert.equal(f.held.size,0);
});
test('cleanup errors are surfaced and do not replace the original simulation failure',async()=>{
 const f=fixture({failedUp:'x'});await assert.rejects(pressMixedMarioMedleyInput(f.peer,async()=>{}),/up failure/);
 const g=fixture({failedUp:'x'});await assert.rejects(pressMixedMarioMedleyInput(g.peer,async()=>{throw Error('original failure');}),/original failure/);
});
test('m432 logical directions use the real default browser main-stick binding, with unchanged B/X/Y keys',async()=>{
 const {defaultControlBindings}=await import('../../web/control-bindings.mjs');const f=fixture();
 const peer={keyboard:createBoundMatchKeyboard(f.peer.keyboard,browserMatchMapping(defaultControlBindings()))};
 await pressMixedM432ObstacleInput(peer,async(_peer,count)=>{assert.equal(count,2);assert.deepEqual([...f.held].sort(),['ArrowUp','c','v','x']);},'w');
 assert.equal(f.held.size,0);
});
test('invalid steering is rejected before any input is sent',()=>{
 const f=fixture();assert.throws(()=>pressMixedM432ObstacleInput(f.peer,async()=>{},'Enter'),/logical main-stick direction/);assert.deepEqual(f.events,[]);
});
