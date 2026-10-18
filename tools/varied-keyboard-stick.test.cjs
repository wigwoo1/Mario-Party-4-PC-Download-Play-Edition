'use strict';
const test=require('node:test'),assert=require('node:assert/strict');
const {variedKeyboardStick}=require('./varied-keyboard-stick.cjs');
test('varied stick covers eight directions without opposing keys and releases them',async()=>{
 const held=new Set(),directions=new Set();let frames=0;
 await variedKeyboardStick({hold:async(key,down)=>{down?held.add(key):held.delete(key);},waitFrames:async count=>{
  assert.equal(count,4);assert(!(held.has('a')&&held.has('d')));assert(!(held.has('w')&&held.has('s')));
  directions.add([...held].sort().join(''));frames++;
 }});
 assert.equal(frames,512);assert.equal(directions.size,8);assert.equal(held.size,0);
});
test('event end exits before sending another segment',async()=>{
 let checks=0,frames=0;const held=new Set();
 await variedKeyboardStick({hold:async(key,down)=>{down?held.add(key):held.delete(key);},waitFrames:async()=>frames++,shouldContinue:async()=>++checks<=3});
 assert.equal(frames,3);assert.equal(checks,4);assert.equal(held.size,0);
});
test('input wait failure preserves original error and releases diagonal keys',async()=>{
 const held=new Set(),original=new Error('lost frame observer');
 await assert.rejects(variedKeyboardStick({hold:async(key,down)=>{down?held.add(key):held.delete(key);},waitFrames:async()=>{if(held.size===2)throw original;}}),error=>error===original);
 assert.equal(held.size,0);
});
test('release failures retain original operation failure',async()=>{
 const original=new Error('wait failed'),release=new Error('release failed');
 await assert.rejects(variedKeyboardStick({hold:async(key,down)=>{if(!down)throw release;},waitFrames:async()=>{throw original;}}),error=>error instanceof AggregateError&&error.errors[0]===original&&error.errors.includes(release));
});
