import {test} from 'node:test';
import assert from 'node:assert/strict';
import {audioLayout} from './audio-layout.mjs';
test('layout follows prepared runtime and preserves local preference',()=>{
  const select=new EventTarget(),label={},saved=new Map([['partyboard.audioChannels','6']]),calls=[];
  const control=audioLayout(select,label,{getItem:key=>saved.get(key),setItem:(key,value)=>saved.set(key,value)});
  assert.equal(select.disabled,false);assert.equal(select.value,'6');
  assert.equal(control.channels,6);
  assert.equal(control.bind({_PartyBoardBrowserSetAudioChannels:value=>{calls.push(value);return 0;}}),true);
  assert.deepEqual(calls,[6]);assert.equal(select.disabled,false);
  select.value='2';select.dispatchEvent(new Event('change'));
  assert.equal(saved.get('partyboard.audioChannels'),'2');assert.deepEqual(calls,[6,2]);
  assert.equal(control.channels,2);
  select.value='9';select.dispatchEvent(new Event('change'));
  assert.equal(select.value,'2');assert.deepEqual(calls,[6,2]);
  control.bind(undefined);assert.equal(select.disabled,false);
  assert.equal(control.bind({_PartyBoardBrowserSetAudioChannels:()=>-1}),false);
  select.value='6';select.dispatchEvent(new Event('change'));
  assert.equal(select.value,'2');assert.equal(saved.get('partyboard.audioChannels'),'2');
});

test('layout can be selected before runtime preparation and applies on bind',()=>{
  const select=new EventTarget(),label={},saved=new Map(),calls=[];
  const control=audioLayout(select,label,{getItem:key=>saved.get(key),setItem:(key,value)=>saved.set(key,value)});
  assert.equal(select.disabled,false);
  assert.equal(select.value,'2');
  select.value='6';select.dispatchEvent(new Event('change'));
  assert.equal(saved.get('partyboard.audioChannels'),'6');
  assert.deepEqual(calls,[]);
  assert.match(label.textContent,/next compatible game runtime/);
  assert.equal(control.bind({_PartyBoardBrowserSetAudioChannels:value=>{calls.push(value);return 0;}}),true);
  assert.deepEqual(calls,[6]);
  assert.equal(select.disabled,false);
});
test('blocked storage and old runtime remain safe',()=>{
  const select=new EventTarget(),label={};
  const control=audioLayout(select,label,{getItem(){throw Error('blocked');},setItem(){throw Error('blocked');}});
  assert.equal(select.value,'2');assert.equal(control.bind({}),false);
  control.bind({_PartyBoardBrowserSetAudioChannels:()=>0});
  select.value='6';select.dispatchEvent(new Event('change'));
  assert.equal(select.value,'6');assert.equal(select.disabled,false);
});
test('reports actual destination separately from selected layout',()=>{
  const select=new EventTarget(),label={};let device=0;
  const control=audioLayout(select,label,{getItem:()=> '6',setItem(){}});
  control.bind({_PartyBoardBrowserSetAudioChannels:()=>0,_PartyBoardBrowserAudioSourceChannels:()=>6,_PartyBoardBrowserAudioDeviceChannels:()=>device});
  assert.match(label.textContent,/unavailable/);
  device=2;select.dispatchEvent(new Event('focus'));
  assert.match(label.textContent,/SDL output: 2 channels; source: 6 channels/);
  assert.match(label.textContent,/unverified/);
  control.bind({_PartyBoardBrowserSetAudioChannels:()=>0,_PartyBoardBrowserAudioSourceChannels:()=>6,_PartyBoardBrowserAudioDeviceChannels:()=>6,SDL3:{audioContext:{destination:{channelCount:2,maxChannelCount:2}}}});
  assert.match(label.textContent,/SDL output: 6/);
  assert.match(label.textContent,/Browser destination: 2 channels \(maximum 2\)/);
});
test('reports supported rear bus and configures a compatible live destination',()=>{
  const select=new EventTarget(),label={},destination={channelCount:2,maxChannelCount:6};
  const control=audioLayout(select,label,{getItem:()=> '6',setItem(){}});
  const module={_PartyBoardBrowserSetAudioChannels:()=>0,_PartyBoardBrowserAudioSourceChannels:()=>6,
    _PartyBoardBrowserAudioSynthMetric:field=>field===13?1:0,SDL3:{audioContext:{destination}}};
  control.bind(module);assert.equal(destination.channelCount,6);
  assert.match(label.textContent,/mono surround feeds both rear/);assert.match(label.textContent,/center\/LFE/);
  // A preference for the next game must not resize a currently six-channel mix.
  select.value='2';select.dispatchEvent(new Event('change'));assert.equal(destination.channelCount,6);
});
