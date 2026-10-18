import test from 'node:test';import assert from 'node:assert/strict';
import {defaultControlBindings,validateControlBindings,createControlBindingEditor,compileKeyboardBindings,compileControllerBindings,controlBindingGlyphs} from './control-bindings.mjs';
test('standard browser controller numbering converts to SDL with inverted Y and analog trigger companions',()=>{
 const profile=defaultControlBindings(),compiled=compileControllerBindings(profile);
 assert.equal(compiled.buttons.find(b=>b.padButton===0x10).nativeButton,10);
 assert.equal(profile.controller.z,'button:5');assert.equal(controlBindingGlyphs(profile).gamepad[11],'r1');
 assert.deepEqual(compiled.axes.find(b=>b.padAxis===2),{nativeAxis:{nativeAxis:1,sign:-1},nativeButton:-1,padAxis:2});
 assert.deepEqual(compiled.axes.find(b=>b.padAxis===8),{nativeAxis:{nativeAxis:4,sign:1},nativeButton:-1,padAxis:8});
 assert.equal(compiled.buttons.find(b=>b.padButton===0x40).nativeButton,0xffffffff);
 profile.controller.l='button:4';const remapped=compileControllerBindings(profile);
 assert.equal(remapped.buttons.find(b=>b.padButton===0x40).nativeButton,9);
 assert.equal(remapped.axes.find(b=>b.padAxis===8).nativeButton,9);
 profile.controller.a='axis:0:+';profile.controller.stickRight='button:0';assert.throws(()=>compileControllerBindings(profile),/digital action/);
});
test('engine keyboard plan matches SDL defaults and preserves digital and analog trigger remaps',()=>{
 const defaults=compileKeyboardBindings(defaultControlBindings());
 assert.equal(defaults.buttons.length,12);assert.equal(defaults.axes.length,10);
 assert.deepEqual(defaults.buttons.find(b=>b.padButton===0x100),{scancode:29,padButton:0x100});
 for(const [padAxis,scancode] of [[0,79],[1,80],[2,82],[3,81]])
  assert.deepEqual(defaults.axes.find(b=>b.padAxis===padAxis),{scancode,padAxis,influence:0});
 for(const [padButton,scancode] of [[8,26],[4,22],[1,4],[2,7]])
  assert.deepEqual(defaults.buttons.find(b=>b.padButton===padButton),{scancode,padButton});
 const profile=defaultControlBindings();profile.keyboard.l='Space';
 const compiled=compileKeyboardBindings(profile);
 assert.equal(compiled.buttons.find(b=>b.padButton===0x40).scancode,44);
 assert.equal(compiled.axes.find(b=>b.padAxis===8).scancode,44);
 profile.keyboard.a='Space';assert.throws(()=>compileKeyboardBindings(profile),/conflict/);
});
test('cancel restores committed controls and reset does not commit until accepted',()=>{
 const editor=createControlBindingEditor();editor.set('keyboard','a','Space');editor.commit();editor.set('keyboard','b','Tab');editor.cancel();assert.equal(editor.snapshot().keyboard.b,'KeyX');assert.equal(editor.snapshot().keyboard.a,'Space');editor.reset();assert.equal(editor.committed().keyboard.a,'Space');editor.commit();assert.deepEqual(editor.committed(),defaultControlBindings());
});
test('conflicts reject commit without changing accepted keyboard or controller preferences',()=>{
 const editor=createControlBindingEditor();editor.set('keyboard','a','KeyX');assert.match(editor.conflicts(),/conflict/);assert.throws(()=>editor.commit(),/conflict/);assert.equal(editor.committed().keyboard.a,'KeyZ');editor.cancel();editor.set('controller','a','button:1');assert.throws(()=>editor.commit(),/conflict/);assert.equal(editor.committed().controller.a,'button:0');
});
test('opposite controller axis directions remain distinct and malformed profiles are rejected',()=>{
 assert.deepEqual(validateControlBindings(defaultControlBindings()),defaultControlBindings());const profile=defaultControlBindings();delete profile.keyboard.a;assert.throws(()=>validateControlBindings(profile));const editor=createControlBindingEditor();editor.set('controller','a','axis:8:+');assert.throws(()=>editor.commit());const snapshot=editor.committed();snapshot.keyboard.a='Space';assert.equal(editor.committed().keyboard.a,'KeyZ');
});


