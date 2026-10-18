import test from 'node:test';
import assert from 'node:assert/strict';
import {cachedDownloadControls} from './cached-download-controls.mjs';
const asset={id:'a'.repeat(64),name:'Game.ciso'};
function fixture(){
 let idle=false,generation=0,finish,fail=false,opens=0,removes=0,closes=0;
 const cleanup=new Promise(resolve=>finish=resolve),button={},info={textContent:''},messages=[];
 const controls=cachedDownloadControls(button,info,{available:()=>idle,settled:()=>cleanup,epoch:()=>generation,
  openStore:async()=>{opens++;return {remove:async id=>{assert.equal(id,asset.id);if(fail)throw Error('storage unavailable');removes++;},close(){closes++;}};},
  status:message=>messages.push(message)});
 controls.remember(asset);
 return {button,controls,messages,finish,idle:value=>{idle=value;controls.refresh();},epoch:()=>generation++,fail:value=>fail=value,
  counts:()=>({opens,removes,closes})};
}
test('remove is unavailable during session; idle removal waits for runtime cleanup',async()=>{
 const f=fixture();assert.equal(f.button.disabled,true);await f.button.onclick();assert.equal(f.counts().opens,0);
 f.idle(true);assert.equal(f.button.disabled,false);const removing=f.button.onclick();
 await Promise.resolve();assert.equal(f.counts().opens,0);assert.equal(f.button.disabled,true);
 f.finish();await removing;assert.deepEqual(f.counts(),{opens:1,removes:1,closes:1});assert.equal(f.button.disabled,true);
});
test('a new session while cleanup is pending cancels removal and keeps its status',async()=>{
 const f=fixture();f.idle(true);const removing=f.button.onclick();f.epoch();f.idle(false);f.finish();await removing;
 assert.deepEqual(f.counts(),{opens:0,removes:0,closes:0});assert.deepEqual(f.messages,[]);
 f.idle(true);assert.equal(f.button.disabled,false);
});
test('failed deletion retains the choice and can be retried',async()=>{
 const f=fixture();f.idle(true);f.finish();f.fail(true);await f.button.onclick();
 assert.match(f.messages[0],/Could not remove download/);assert.equal(f.button.disabled,false);assert.equal(f.counts().closes,1);
 f.fail(false);await f.button.onclick();assert.equal(f.counts().removes,1);assert.equal(f.counts().closes,2);assert.equal(f.button.disabled,true);
});
test('new session during database opening closes storage without deleting its download',async()=>{
 let generation=0,idle=true,provide,removes=0,closes=0;
 const opening=new Promise(resolve=>provide=resolve),button={},messages=[];
 const controls=cachedDownloadControls(button,{textContent:''},{available:()=>idle,settled:()=>Promise.resolve(),epoch:()=>generation,
  openStore:()=>opening,status:message=>messages.push(message)});
 controls.remember(asset);const removing=button.onclick();await Promise.resolve();
 generation++;idle=false;controls.remember({...asset,name:'New session with same game'});
 provide({remove:async()=>removes++,close:()=>closes++});await removing;
 assert.equal(removes,0);assert.equal(closes,1);assert.deepEqual(messages,[]);
 idle=true;controls.refresh();assert.equal(button.disabled,false);
});
