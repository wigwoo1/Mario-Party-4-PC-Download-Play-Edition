import test from 'node:test';
import assert from 'node:assert/strict';
import {gameAudioControls} from './game-audio-controls.mjs';
function fixture(saved){
  const volume=new EventTarget(),mute=new EventTarget(),label={},writes=[];
  mute.setAttribute=(key,value)=>{mute[key]=value;};
  const control=gameAudioControls(volume,mute,label,{getItem:()=>saved,setItem:(...values)=>writes.push(values)});
  const input=value=>{volume.value=String(value);volume.dispatchEvent(new Event('input'));};
  const click=()=>mute.dispatchEvent(new Event('click'));
  return {volume,mute,label,writes,control,input,click};
}
test('saved local volume applies before playback, updates live, mutes and restores chosen level',()=>{
  const f=fixture(JSON.stringify({volume:37,muted:false})),calls=[];
  const owner={_PartyBoardBrowserSetAudioOutput:(...args)=>{calls.push(args);return 0;}};
  assert.equal(f.control.bind(owner),true);assert.deepEqual(calls,[[37,0]]);
  f.click();f.input(25);assert.deepEqual(calls.at(-1),[25,1]);
  f.click();assert.deepEqual(calls.at(-1),[25,0]);assert.equal(f.mute['aria-pressed'],'false');
  assert.deepEqual(JSON.parse(f.writes.at(-1)[1]),{volume:25,muted:false});
  assert.deepEqual(f.control.preferences,{volume:25,muted:false});
});
test('cleanup only detaches its own runtime, never sends changes to a stopped owner',()=>{
  const f=fixture(),a=[],b=[],first={_PartyBoardBrowserSetAudioOutput:(...v)=>{a.push(v);return 0;}},second={_PartyBoardBrowserSetAudioOutput:(...v)=>{b.push(v);return 0;}};
  f.control.bind(first);f.control.bind(second);f.control.unbind(first);f.input(50);
  assert.equal(a.length,1);assert.deepEqual(b.at(-1),[50,0]);
  f.control.unbind(second);f.input(60);assert.equal(b.length,2);
  f.control.bind(first);assert.deepEqual(a.at(-1),[60,0]);
});
test('malformed saved values and invalid edits cannot amplify or corrupt volume',()=>{
  for(const saved of ['{','null','[]','{"volume":-1,"muted":false}','{"volume":101,"muted":false}','{"volume":25,"muted":"false"}']) {
    const f=fixture(saved);assert.deepEqual(f.control.preferences,{volume:80,muted:false});
    for(const value of [-1,101,NaN,1.5])f.input(value);
    assert.equal(f.control.preferences.volume,80);assert.equal(f.writes.length,0);
  }
});
test('blocked storage and unsupported/rejected runtime preserve editable preferences',()=>{
  const volume=new EventTarget(),mute=new EventTarget(),label={};mute.setAttribute=()=>{};
  const api=gameAudioControls(volume,mute,label,{getItem(){throw Error();},setItem(){throw Error();}});
  assert.equal(api.bind({}),false);assert.match(label.textContent,/cannot apply/);
  volume.value='0';volume.dispatchEvent(new Event('input'));assert.equal(api.preferences.volume,0);
  assert.equal(api.bind({_PartyBoardBrowserSetAudioOutput:()=>-1}),false);
  mute.dispatchEvent(new Event('click'));assert.equal(api.preferences.muted,true);
});
