import test from 'node:test';
import assert from 'node:assert/strict';
import {defaultControlBindings,controlBindingGlyphs,compileKeyboardBindings,compileControllerBindings} from './control-bindings.mjs';
import {windowControlGlyph} from './window-control-glyphs.mjs';
import {actionGlyph} from './control-glyphs.mjs';
import {prepareRuntimeGlyphs} from './runtime-glyphs.mjs';

test('Start bank12 follows default and remapped keyboard/controller actions',()=>{
 const profile=defaultControlBindings();
 assert.equal(controlBindingGlyphs(profile).keyboard[12],'Enter');
 assert.equal(controlBindingGlyphs(profile).gamepad[12],'start');
 profile.keyboard.start='Space';profile.controller.start='button:8';
 assert.equal(controlBindingGlyphs(profile).keyboard[12],'Space');
 assert.equal(controlBindingGlyphs(profile).gamepad[12],'select');
 assert.equal(compileKeyboardBindings(profile).buttons.find(b=>b.padButton===0x1000).scancode,44);
 assert.equal(compileControllerBindings(profile).buttons.find(b=>b.padButton===0x1000).nativeButton,4);
});

test('Start resolves Enter and actual controller family while preserving frame sentinels',()=>{
 const resolve=(binding,family='generic')=>action=>actionGlyph(action,{family,bindings:{PAD_BUTTON_MENU:binding}});
 assert.equal(windowControlGlyph(12,0,resolve({kind:'key',label:'Enter'})).label,'Enter');
 for(const [family,label] of [['playstation','Options'],['xbox','Menu'],['switch','+'],['generic','Start']])
  assert.equal(windowControlGlyph(12,0,resolve({kind:'button',button:'start'},family)).label,label);
 assert.equal(windowControlGlyph(12,1,()=>{throw Error('End sentinel must not resolve');}),null);
 assert.equal(windowControlGlyph(13,0,()=>{throw Error('Arrow decoration must not resolve');}),null);
});

test('runtime texture bridge includes remapped Start and preserves unknown mappings',async()=>{
 const profile=defaultControlBindings();profile.keyboard.start='Space';profile.controller.start='button:16';
 const requests=[];
 const textures=await prepareRuntimeGlyphs({bindings:controlBindingGlyphs(profile),render:async request=>{
  requests.push(request);return{width:64,height:64,rgba:new Uint8Array(16384)};
 }});
 assert.equal(textures.initial[12].length,16384);
 assert.ok(requests.some(request=>request.binding.label==='Space'));
 assert.equal(requests.some(request=>request.binding.button==='start'),false);
});
