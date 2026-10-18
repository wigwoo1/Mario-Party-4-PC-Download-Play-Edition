import test from 'node:test';
import assert from 'node:assert/strict';
import {InputChannels} from './input-channels.mjs';
import {SessionInput} from './session-input.mjs';
import {encodeInput} from './input-packet.mjs';
import {createSessionInputExchange} from './session-input-exchange.mjs';
const id=Uint8Array.of(1,...Array(15).fill(0));
class Channel extends EventTarget {readyState='open';close(){this.readyState='closed';this.dispatchEvent(new Event('close'));}message(data){this.dispatchEvent(new MessageEvent('message',{data}));}}
test('incoming channel cannot be assigned to another peer during replacement notification',()=>{
 const q=new SessionInput(id,[11,22,0,0]),old=new Channel(),next=new Channel();let t,attempts=0;
 t=new InputChannels(q,{now:()=>0,changed:(_peer,reason)=>{if(reason==='Replaced'){attempts++;assert.throws(()=>t.attach(11,next),/already in progress/);}}});
 t.attach(22,old);t.attach(22,next);assert.equal(attempts,1);assert.equal(t.ready(11),false);assert.equal(t.ready(22),true);
 assert.throws(()=>t.attach(11,next),/already assigned/);t.close();assert.equal(next.readyState,'closed');
});
test('nested attachment for the same peer cannot overwrite replacement ownership',()=>{
 const q=new SessionInput(id,[11,22,0,0]),nested=new Channel(),old=new Channel(),next=new Channel();let t,rejected=0;
 t=new InputChannels(q,{now:()=>0,changed:(_peer,reason)=>{if(reason==='Replaced'){assert.throws(()=>t.attach(22,nested),/already in progress/);rejected++;}}});
 t.attach(22,old);t.attach(22,next);assert.equal(rejected,1);assert.equal(t.ready(22),true);
 nested.message(new ArrayBuffer(1));assert.equal(q.status(0).state,'Paused');
 t.close();assert.equal(next.readyState,'closed');assert.equal(nested.readyState,'open');nested.close();
});
test('session shutdown during replacement cannot install a new channel afterward',()=>{
 const q=new SessionInput(id,[11,22,0,0]),events=[];let t;
 t=new InputChannels(q,{now:()=>0,changed:(peer,reason)=>{events.push([peer,reason]);if(reason==='Replaced')t.close();}});
 const old=new Channel(),next=new Channel();t.attach(22,old);
 assert.throws(()=>t.attach(22,next),/closed during replacement/);
 assert.equal(old.readyState,'closed');assert.equal(next.readyState,'closed');assert.equal(t.ready(22),false);
 next.message(new ArrayBuffer(1));next.dispatchEvent(new Event('open'));assert.deepEqual(events,[[22,'Replaced'],[null,'Closed']]);
 assert.equal(q.status(0).state,'ReturnToMenu');t.close();assert.equal(events.length,2);
});
test('connecting replacement reports readiness without automatically resuming; stale opens ignored',()=>{
 const q=new SessionInput(id,[11,22,0,0]),events=[],t=new InputChannels(q,{now:()=>0,changed:(peer,reason)=>events.push([peer,reason])});
 const old=new Channel(),next=new Channel();next.readyState='connecting';t.attach(22,old);t.attach(22,next);
 assert.equal(t.ready(22),false);const revision=q.status(0).revision;
 old.readyState='open';old.dispatchEvent(new Event('open'));assert.deepEqual(events,[[22,'Replaced']]);
 next.readyState='open';next.dispatchEvent(new Event('open'));assert.equal(t.ready(22),true);
 assert.deepEqual(events,[[22,'Replaced'],[22,'Open']]);assert.equal(q.status(0).state,'Paused');assert.equal(q.status(0).revision,revision);
 t.close();next.dispatchEvent(new Event('open'));assert.equal(events.length,3);assert.equal(t.ready(22),false);
});

test('channel loss, replacement and transport shutdown wake a pending PAD wait once',async()=>{
 for(const action of ['loss','replace','shutdown']) {
  const queue=new SessionInput(id,[11,22,0,0]),events=[];
  const bridge=createSessionInputExchange(queue,{session:id,owners:[11,22,0,0],peer:11,checkpoint:()=>42,send:()=>{},now:()=>0});
  const transport=new InputChannels(queue,{now:()=>0,changed:(peer,reason)=>{events.push([peer,reason]);bridge.notify();}}),channel=new Channel();
  transport.attach(22,channel);const wait=bridge.exchange(new Uint8Array(44));
  if(action==='loss')channel.close();else if(action==='replace')transport.attach(22,new Channel());else transport.close();
  assert.equal(await wait,null);assert.equal(queue.status(0).frame,0n);assert.equal(events.length,1);
  channel.close();assert.equal(events.length,1);transport.close();transport.close();
 }
});
test('replaced channel cannot inject late data or change recovery revision',()=>{
 const q=new SessionInput(id,[11,22,0,0]),transport=new InputChannels(q,{now:()=>10}),old=new Channel(),next=new Channel();
 transport.attach(22,old);transport.attach(22,next);const token=q.status(10);
 old.message(new ArrayBuffer(1));old.dispatchEvent(new Event('close'));
 assert.equal(q.status(10).state,'Paused');assert.equal(q.status(10).revision,token.revision);
 const packet=encodeInput({session:id,frame:0n,slot:1,checkpoint:42,pad:new Uint8Array(11)});
 next.message(packet.buffer);assert.ok(q.recovered(22,token,10));assert.ok(q.resume(token,10));
 assert.equal(q.status(10).state,'Running');transport.close();transport.close();
 assert.equal(next.readyState,'closed');assert.equal(q.status(10).state,'ReturnToMenu');
});
test('malformed active input closes session; duplicate close is harmless',()=>{
 const q=new SessionInput(id,[11,22,0,0]),errors=[],transport=new InputChannels(q,{now:()=>0,error:(peer,e)=>errors.push([peer,e.message])}),c=new Channel();
 transport.attach(22,c);c.message('not binary');c.close();
 assert.equal(q.status(0).state,'ReturnToMenu');assert.equal(errors.length,1);assert.equal(errors[0][0],22);
});


test('unassigned peers and shared channels cannot disturb the current connection',()=>{
 const q=new SessionInput(id,[11,22,0,0]),t=new InputChannels(q,{now:()=>0}),c=new Channel();
 t.attach(22,c);assert.throws(()=>t.attach(99,new Channel()),/Invalid input channel/);
 assert.throws(()=>t.attach(11,c),/already assigned/);
 assert.equal(c.readyState,'open');assert.equal(q.status(0).state,'Running');
 c.message(encodeInput({session:id,frame:0n,slot:1,checkpoint:42,pad:new Uint8Array(11)}).buffer);
 assert.equal(q.status(0).state,'Running');t.close();
});

test('unreliable replacement is rejected before disturbing active input ownership',()=>{
 const q=new SessionInput(id,[11,22,0,0]),t=new InputChannels(q,{now:()=>0}),current=new Channel();t.attach(22,current);
 for(const options of [{ordered:false},{maxRetransmits:0},{maxPacketLifeTime:100}]) {
  const replacement=Object.assign(new Channel(),options);assert.throws(()=>t.attach(22,replacement),/Invalid input channel/);
  assert.equal(current.readyState,'open');assert.equal(q.status(0).state,'Running');
 }
 t.close();
});
