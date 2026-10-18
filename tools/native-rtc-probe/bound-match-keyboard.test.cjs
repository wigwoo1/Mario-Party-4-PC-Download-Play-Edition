'use strict';
const {test}=require('node:test');
const assert=require('node:assert/strict');
const {createBoundMatchKeyboard,browserMatchMapping}=require('./bound-match-keyboard.cjs');
test('custom A, held stick and unchanged Start reach the physical keyboard',async()=>{
 const calls=[];
 const keyboard=Object.fromEntries(['press','down','up'].map(method=>[method,async(...args)=>calls.push([method,...args])]));
 const bound=createBoundMatchKeyboard(keyboard,{z:'Space',w:'ArrowUp'});
 await bound.down('w'); await bound.press('z',{delay:100}); await bound.up('w'); await bound.press('Enter');
 assert.deepEqual(calls,[['down','ArrowUp'],['press','Space',{delay:100}],['up','ArrowUp'],['press','Enter',undefined]]);
});
test('actual default browser bindings preserve the native match driver stick and D-pad actions',async()=>{
 const {defaultControlBindings,compileKeyboardBindings}=await import('../../web/control-bindings.mjs');
 const defaults=defaultControlBindings(),mapping=browserMatchMapping(defaults),compiled=compileKeyboardBindings(defaults);
 assert.equal(mapping.w,'ArrowUp');assert.equal(mapping.ArrowUp,'w');
 const calls=[],keyboard=createBoundMatchKeyboard({down:async key=>calls.push(key),up:async()=>{},press:async()=>{}},mapping);
 await keyboard.down('w');assert.deepEqual(calls,['ArrowUp']);
 const stickUp=compiled.axes.find(axis=>axis.padAxis===2),padUp=compiled.buttons.find(button=>button.padButton===8);
 assert.equal(stickUp.scancode,82);assert.equal(padUp.scancode,26);
 assert.equal(mapping.e,'e');assert.equal(mapping.z,'z');
});
