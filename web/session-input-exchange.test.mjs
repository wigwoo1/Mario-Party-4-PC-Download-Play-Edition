import test from 'node:test';
import assert from 'node:assert/strict';
import {SessionInput} from './session-input.mjs';
import {createSessionInputExchange} from './session-input-exchange.mjs';
import {encodeInput,decodeInput} from './input-packet.mjs';
const session=new Uint8Array(16).fill(1),owners=[11,22,0,0];
test('completed receipt retains the last actual poll independently of caller mutation and pending recovery',async()=>{
 const queue=new SessionInput(session,owners);let time=0;
 const bridge=createSessionInputExchange(queue,{session,owners,peer:11,checkpoint:metadata=>metadata.checkpoint,
  send:()=>{},suspendOnPause:true,now:()=>time});
 assert.equal(bridge.completedState(),null);
 for(let index=0;index<2;index++){
  const wait=bridge.exchange(new Uint8Array(44).fill(index+7),{checkpoint:42+index,frand:3+index,rand8:4+index});
  queue.receive(22,encodeInput({session,frame:BigInt(index),checkpoint:42+index,slot:1,pad:new Uint8Array(11).fill(index+9)}),time);
  bridge.notify();const result=await wait;result.fill(0);
  const receipt=bridge.completedState();assert.equal(receipt.frame,BigInt(index));assert.equal(receipt.checkpoint,42+index);
  assert.equal(receipt.frand,3+index);assert.equal(receipt.rand8,4+index);
  assert.equal(receipt.pads[0],index+7);assert.equal(receipt.pads[11],index+9);assert.equal(receipt.pads[32],255);
  assert.equal(decodeInput(receipt.packets[0]).frame,BigInt(index));assert.equal(decodeInput(receipt.packets[0]).pad[0],index+7);
  receipt.pads.fill(0);receipt.packets[0].fill(0);receipt.packets.length=0;
  assert.equal(bridge.completedState().pads[0],index+7);assert.equal(bridge.completedState().packets.length,1);
 }
 const controller=new AbortController(),wait=bridge.exchange(new Uint8Array(44).fill(55),{checkpoint:44,frand:5,rand8:6,signal:controller.signal});
 queue.disconnected(22,time);time=1000;bridge.notify();
 const before=queue.status(time);assert.equal(before.state,'Paused');assert.equal(before.remainingMs,59000);
 assert.equal(bridge.completedState().frame,1n);assert.equal(bridge.pendingState().frame,2n);
 assert.equal(queue.status(time).frame,2n);assert.throws(()=>bridge.resend(),/Running pending/);
 assert.deepEqual(queue.status(time),before);controller.abort();assert.equal(await wait,null);
 assert.equal(bridge.completedState().frame,1n);
});
test('a divergent or aborted poll never creates a completed receipt',async()=>{
 for(const divergent of [false,true]){
  const queue=new SessionInput(session,owners),controller=new AbortController();
  const bridge=createSessionInputExchange(queue,{session,owners,peer:11,checkpoint:()=>42,send:()=>{},now:()=>0});
  const wait=bridge.exchange(new Uint8Array(44),{signal:controller.signal});
  if(divergent){queue.receive(22,encodeInput({session,frame:0n,checkpoint:43,slot:1,pad:new Uint8Array(11)}),0);bridge.notify();}
  else controller.abort();
  assert.equal(await wait,null);assert.equal(bridge.completedState(),null);assert.equal(queue.status(0).frame,0n);
 }
});
test('disconnect before callback entry suspends and registers retained local input on recovery',async()=>{
 const queue=new SessionInput(session,owners),sent=[];
 const bridge=createSessionInputExchange(queue,{session,owners,peer:11,checkpoint:()=>42,send:packet=>sent.push(packet),suspendOnPause:true,now:()=>0});
 queue.disconnected(22,0);let settled=false;
 const wait=bridge.exchange(new Uint8Array(44).fill(7)).then(value=>{settled=true;return value;});
 await Promise.resolve();assert.equal(settled,false);assert.equal(sent.length,0);assert.equal(bridge.pendingState().frame,0n);
 const token=queue.status(0);assert.ok(queue.recovered(22,token,0));assert.ok(queue.resume(token,0));bridge.resend();
 assert.equal(sent.length,1);queue.receive(22,encodeInput({session,frame:0n,checkpoint:42,slot:1,pad:new Uint8Array(11)}),0);bridge.notify();
 assert.equal((await wait)[0],7);assert.equal(queue.status(0).frame,1n);
});
test('callback entering an already paused queue releases on abort or deadline without sending',async()=>{
 for(const action of ['abort','expiry']){
  const queue=new SessionInput(session,owners),sent=[];let time=0;
  const bridge=createSessionInputExchange(queue,{session,owners,peer:11,checkpoint:()=>42,send:packet=>sent.push(packet),suspendOnPause:true,now:()=>time});
  queue.disconnected(22,0);const controller=new AbortController();
  const wait=bridge.exchange(new Uint8Array(44),{signal:controller.signal,frand:7,rand8:8});
  if(action==='abort')controller.abort();else{time=60000;bridge.notify();}
  assert.equal(await wait,null);assert.equal(bridge.pendingState(),null);assert.equal(sent.length,0);assert.equal(queue.status(time).frame,0n);
  if(action==='expiry')assert.equal(queue.status(time).state,'ReturnToMenu');
 }
});
test('reentrant recovery receipt waits until every owned packet has been resent',async()=>{
 const owned=[11,22,11,0],queue=new SessionInput(session,owned);let recovering=false,sends=0,bridge;
 bridge=createSessionInputExchange(queue,{session,owners:owned,peer:11,checkpoint:()=>42,suspendOnPause:true,now:()=>0,
  send:()=>{sends++;if(recovering){queue.receive(22,encodeInput({session,frame:0n,checkpoint:42,slot:1,pad:new Uint8Array(11)}),0);bridge.notify();assert.equal(queue.status(0).frame,0n);}}});
 const wait=bridge.exchange(new Uint8Array(44));queue.disconnected(22,0);bridge.notify();
 const token=queue.status(0);assert.ok(queue.recovered(22,token,0));assert.ok(queue.resume(token,0));recovering=true;bridge.resend();
 assert.equal(sends,4);assert.equal((await wait).length,44);assert.equal(queue.status(0).frame,1n);
});
function fixture(){const queue=new SessionInput(session,owners),sent=[];return {queue,sent,...createSessionInputExchange(queue,{session,owners,peer:11,checkpoint:()=>42,send:p=>sent.push(p),now:()=>0})};}
test('suspended pending poll resumes in place or releases at reconnect deadline',async()=>{
 for(const timeout of [false,true]) {
  let time=0,settled=false;const queue=new SessionInput(session,owners),sent=[];
  const bridge=createSessionInputExchange(queue,{session,owners,peer:11,checkpoint:()=>42,send:p=>sent.push(p),suspendOnPause:true,now:()=>time});
  const wait=bridge.exchange(new Uint8Array(44).fill(6)).then(p=>{settled=true;return p;});queue.disconnected(22,0);bridge.notify();
  await Promise.resolve();assert.equal(settled,false);assert.throws(()=>bridge.resend(),/Running pending/);
  if(timeout){time=60000;bridge.notify();assert.equal(await wait,null);assert.equal(queue.status(time).frame,0n);}
  else {const token=queue.status(0);assert.ok(queue.recovered(22,token,0));assert.ok(queue.resume(token,0));bridge.resend();assert.deepEqual(sent[1],sent[0]);
   queue.receive(22,encodeInput({session,frame:0n,checkpoint:42,slot:1,pad:new Uint8Array(11)}),0);bridge.notify();assert.equal((await wait)[0],6);assert.equal(queue.status(0).frame,1n);}
 }
});
test('verified resume resends original pending sample, then samples fresh input next poll',async()=>{
 const f=fixture(),first=f.exchange(new Uint8Array(44).fill(1));
 f.queue.disconnected(22,0);f.notify();assert.equal(await first,null);
 const token=f.queue.status(0);assert.equal(await f.exchange(new Uint8Array(44).fill(8)),null);assert.equal(f.sent.length,1);
 assert.ok(f.queue.recovered(22,token,0));assert.ok(f.queue.resume(token,0));
 const retry=f.exchange(new Uint8Array(44).fill(9));assert.deepEqual(f.sent[1],f.sent[0]);
 f.queue.receive(22,encodeInput({session,frame:0n,checkpoint:42,slot:1,pad:new Uint8Array(11).fill(3)}),0);f.notify();
 assert.equal((await retry)[0],1);
 const controller=new AbortController(),next=f.exchange(new Uint8Array(44).fill(7),{signal:controller.signal});
 assert.equal(decodeInput(f.sent[2]).frame,1n);assert.equal(decodeInput(f.sent[2]).pad[0],7);controller.abort();await next;
});
test('pending sample cannot resume under a changed checkpoint',async()=>{
 const queue=new SessionInput(session,owners);let hash=42;
 const bridge=createSessionInputExchange(queue,{session,owners,peer:11,checkpoint:()=>hash,send:()=>{},now:()=>0});
 const controller=new AbortController(),wait=bridge.exchange(new Uint8Array(44),{signal:controller.signal});controller.abort();await wait;hash=43;
 assert.throws(()=>bridge.exchange(new Uint8Array(44)),/checkpoint or poll changed/);assert.equal(queue.status(0).state,'ReturnToMenu');
});
test('missing remote input waits; merged owned pads advance one poll only',async()=>{
 const f=fixture(),pads=new Uint8Array(44).fill(9);let settled=false;
 const wait=f.exchange(pads).then(p=>{settled=true;return p;});pads.fill(7);
 await Promise.resolve();assert.equal(settled,false);assert.equal(f.queue.status(0).frame,0n);
 assert.equal(f.sent.length,1);assert.equal(decodeInput(f.sent[0]).pad[0],9);
 f.queue.receive(22,encodeInput({session,frame:0n,checkpoint:42,slot:1,pad:new Uint8Array(11).fill(3)}),0);f.notify();
 const result=await wait;assert.equal(result[0],9);assert.equal(result[11],3);assert.equal(result[32],255);assert.equal(result[43],255);assert.equal(f.queue.status(0).frame,1n);
});
test('abort or disconnect releases pending callback without advancing queue',async()=>{
 for(const mode of ['abort','disconnect']){const f=fixture(),controller=new AbortController();const wait=f.exchange(new Uint8Array(44),{signal:controller.signal});
 if(mode==='abort')controller.abort();else {f.queue.disconnected(22,0);f.notify();}
 assert.equal(await wait,null);assert.equal(f.queue.status(0).frame,0n);}
});
test('divergent peer checkpoint and failed transport halt pending input',async()=>{
 const f=fixture(),wait=f.exchange(new Uint8Array(44));
 f.queue.receive(22,encodeInput({session,frame:0n,checkpoint:43,slot:1,pad:new Uint8Array(11)}),0);f.notify();
 assert.equal(await wait,null);assert.equal(f.queue.status(0).state,'ReturnToMenu');
 const queue=new SessionInput(session,owners),bridge=createSessionInputExchange(queue,{session,owners,peer:11,checkpoint:()=>42,send:()=>{throw Error('closed');},now:()=>0});
 await assert.rejects(bridge.exchange(new Uint8Array(44)),/closed/);assert.equal(queue.status(0).frame,0n);
});


test('local controllers map into owned global slots and retained recovery packets preserve sampled mapping',async()=>{
 const owners=[11,22,11,22],queue=new SessionInput(session,owners),sent=[],mapping=[null,0,null,1];
 const bridge=createSessionInputExchange(queue,{session,owners,peer:22,localPadSlots:mapping,checkpoint:()=>42,send:p=>sent.push(p),now:()=>0});
 mapping[1]=3;
 const pads=new Uint8Array(44);pads.fill(7,0,11);pads.fill(9,11,22);pads.fill(99,22);
 const pending=bridge.exchange(pads,{frand:3,rand8:4});pads.fill(0);
 assert.deepEqual(sent.map(p=>decodeInput(p).slot),[1,3]);
 assert.deepEqual(Array.from(decodeInput(sent[0]).pad),Array(11).fill(7));
 assert.deepEqual(Array.from(decodeInput(sent[1]).pad),Array(11).fill(9));
 queue.disconnected(11,0);bridge.notify();assert.equal(await pending,null);
 const token=queue.status(0);assert.ok(queue.recovered(11,token,0));assert.ok(queue.resume(token,0));
 const resumed=bridge.exchange(new Uint8Array(44).fill(55),{frand:3,rand8:4});
 assert.deepEqual(sent[2],sent[0]);assert.deepEqual(sent[3],sent[1]);
 for(const slot of [0,2])queue.receive(11,encodeInput({session,frame:0n,checkpoint:42,slot,pad:new Uint8Array(11).fill(1)}),0);
 bridge.notify();const merged=await resumed;assert.equal(merged[11],7);assert.equal(merged[33],9);assert.equal(merged[0],1);
});
test('local mapping rejects duplicate devices, unowned slots and invalid lanes',()=>{
 for(const localPadSlots of [[null,0,null,0],[0,0,null,1],[null,4,null,1],[null,0,null,null]])
  assert.throws(()=>createSessionInputExchange(new SessionInput(session,[11,22,11,22]),{session,owners:[11,22,11,22],peer:22,localPadSlots,checkpoint:()=>42,send:()=>{}}),/PAD lanes/);
});
