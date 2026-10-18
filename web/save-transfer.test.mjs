import {test} from 'node:test';
import assert from 'node:assert/strict';
import {host} from './transfer.mjs';
import {prepareSaveTransfer,receiveSaveTransfer} from './save-transfer.mjs';
class Channel extends EventTarget {
  readyState='connecting';
  send(data){queueMicrotask(()=>{if(this.peer.readyState==='open')this.peer.dispatchEvent(new MessageEvent('message',{data}));});}
  close(){if(this.readyState==='closed')return;this.readyState='closed';this.dispatchEvent(new Event('close'));this.peer.close();}
}
const context={assetId:'a'.repeat(64),sessionId:'test-session-0001'};
async function run(prepared,offer=prepared.offer) {
  const a=new Channel(),b=new Channel();a.peer=b;b.peer=a;
  return new Promise((resolve,reject)=>{
    receiveSaveTransfer(b,offer,context,{complete:resolve,error:reject});
    host(a,prepared.asset,reject);
    a.readyState=b.readyState='open';a.dispatchEvent(new Event('open'));b.dispatchEvent(new Event('open'));
  });
}
test('save transport verifies multi-window snapshot and isolates caller mutation',{timeout:3000},async()=>{
  const bytes=Uint8Array.from({length:8192*80},(_,i)=>i%251),original=bytes.slice();
  const pending=prepareSaveTransfer(bytes,context);bytes.fill(0);
  const prepared=await pending,result=await run(prepared,{...prepared.offer,bytes:new Uint8Array([99]),unexpected:'ignore'});
  assert.deepEqual(result.bytes,original);assert.equal(result.sha256,prepared.offer.sha256);
  assert.equal(result.sessionId,context.sessionId);
  assert.equal(result.unexpected,undefined);
});
test('wrong session and oversized save offers are rejected before channel use',async()=>{
  const {offer}=await prepareSaveTransfer(new Uint8Array(8192),context);
  for(const overrides of [{sessionId:'other-session-01'},{assetId:'b'.repeat(64)},{size:40*1024*1024},{slot:2}])
    assert.throws(()=>receiveSaveTransfer(null,{...offer,...overrides},context));
});
test('valid chunk transport cannot acknowledge an incorrect raw-card hash',{timeout:3000},async()=>{
  const prepared=await prepareSaveTransfer(new Uint8Array(8192),context);
  await assert.rejects(run(prepared,{...prepared.offer,sha256:'0'.repeat(64)}),/snapshot integrity/);
  await assert.rejects(run(prepared,{...prepared.offer,transferId:'f'.repeat(64)}),/manifest/);
});

test('installation failure or cancellation cannot acknowledge a save',{timeout:3000},async()=>{
  const prepared=await prepareSaveTransfer(new Uint8Array(8192),context);
  for(const cancel of [false,true]) {
    const a=new Channel(),b=new Channel();a.peer=b;b.peer=a;
    let acknowledged=false,completed=false,release,entered;
    const started=new Promise(resolve=>{entered=resolve;});
    const gate=new Promise(resolve=>{release=resolve;});
    const errors=[];
    let signal;
    const receiver=receiveSaveTransfer(b,prepared.offer,context,{
      install:async(_snapshot,abortSignal)=>{signal=abortSignal;entered();await gate;if(!cancel)throw Error('Install failed');},
      complete:()=>{completed=true;},error:error=>errors.push(error)
    });
    host(a,prepared.asset,error=>errors.push(error),()=>{acknowledged=true;});
    a.readyState=b.readyState='open';a.dispatchEvent(new Event('open'));
    await started;
    assert.equal(acknowledged,false);assert.equal(completed,false);
    if(cancel){receiver.cancel();assert.equal(signal.aborted,true);}
    release();await new Promise(resolve=>setTimeout(resolve,20));
    assert.equal(acknowledged,false);assert.equal(completed,false);
    if(!cancel)assert.match(errors[0].message,/Install failed/);
  }
});
