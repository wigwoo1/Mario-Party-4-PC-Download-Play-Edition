import test from 'node:test';import assert from 'node:assert/strict';
import {reconcilePausedPoll} from './paused-poll-reconciliation.mjs';
import {agreePausedState} from './paused-state-agreement.mjs';
import {SessionInput} from './session-input.mjs';
import {createSessionInputExchange} from './session-input-exchange.mjs';
import {encodeInput} from './input-packet.mjs';
const identity={session:'01'.repeat(16),buildId:'b'.repeat(64),assetId:'a'.repeat(64),profile:'unlocked-no-save-v1',seed:0,revision:8,membership:4,players:[{peer:'host',slot:0},{peer:'guest1',slot:1}]},nonce='c'.repeat(32);
class Channel extends EventTarget{label='partyboard-recovery-v1';readyState='open';ordered=true;maxRetransmits=null;maxPacketLifeTime=null;
 send(data){queueMicrotask(()=>this.other.dispatchEvent(new MessageEvent('message',{data})));}}
function pair(){const a=new Channel(),b=new Channel();a.other=b;b.other=a;return[a,b];}
function captured(frame=7n,checkpoint=42){return{attempt:{peer:22,epoch:1n,revision:1n,frame,checkpoint,frand:3,rand8:5},state:{frame:Number(frame)+3,checkpoint,frand:3,rand8:5},input:{state:'Paused',frame,epoch:1n,revision:1n}};}
test('late completed-route acknowledgment cannot poison or authorize a new round',async()=>{
 const channels=pair(),previous='d'.repeat(32);
 const options=channels.map((channel,index)=>({channel,identity,nonce,peer:index?'guest1':'host',remotePeer:index?'host':'guest1',
  previousNonces:new Set([previous]),capture:()=>captured(),current:()=>true,
  bridge:{completedState:()=>null,pendingState:()=>({}),replayCompletedPoll(){throw Error('Unexpected replay');}},deadline:performance.now()+1000}));
 const results=options.map(option=>reconcilePausedPoll(option));
 for(const option of options)option.channel.dispatchEvent(new MessageEvent('message',{data:JSON.stringify({type:'paused-poll-ack',version:1,nonce:previous,peer:option.remotePeer})}));
 assert.equal((await Promise.all(results))[0].inputFrame,'7');
 const isolated=pair()[0],option={...options[0],channel:isolated};
 const rejected=reconcilePausedPoll(option);
 isolated.dispatchEvent(new MessageEvent('message',{data:JSON.stringify({type:'paused-poll-ack',version:1,nonce:'e'.repeat(32),peer:option.remotePeer})}));
 await assert.rejects(rejected,/identity differs/);
 for(const change of [{peer:'stranger'},{version:2}]){
  const channel=pair()[0],pending=reconcilePausedPoll({...options[0],channel});
  channel.dispatchEvent(new MessageEvent('message',{data:JSON.stringify({type:'paused-poll-ack',version:1,nonce:previous,peer:options[0].remotePeer,...change})}));
  await assert.rejects(pending,/identity differs/);
 }
});
test('matching paused polls negotiate without replay',async()=>{
 const channels=pair();let replayed=0;
 const results=await Promise.all(channels.map((channel,index)=>reconcilePausedPoll({channel,identity,nonce,peer:index?'guest1':'host',remotePeer:index?'host':'guest1',
  capture:()=>captured(),current:()=>true,bridge:{completedState:()=>null,pendingState:()=>({}),replayCompletedPoll(){replayed++;throw Error('unexpected');}},deadline:performance.now()+1000})));
 assert.equal(replayed,0);assert.equal(results[0].inputFrame,'7');assert.equal(results[1].inputFrame,'7');
});
test('healthy start routes require explicit ownership permission to reconcile',async()=>{
 const channels=pair();for(const channel of channels)channel.label='partyboard-start-v1';
 const options=channels.map((channel,index)=>({channel,identity,nonce,peer:index?'guest1':'host',remotePeer:index?'host':'guest1',capture:()=>captured(),current:()=>true,
  bridge:{completedState:()=>null,pendingState:()=>({}),replayCompletedPoll(){throw Error('Unexpected replay');}},deadline:performance.now()+1000}));
 assert.throws(()=>reconcilePausedPoll(options[0]),/Authenticated/);
 await Promise.all(options.map(option=>reconcilePausedPoll({...option,allowStartChannel:true})));
});
test('one-frame reconciliation waits for next actual callback and requires exact resulting state',async()=>{
 const channels=pair();let host=captured(),pending=true,replayed=0;
 const guest=captured(8n,99),pads=new Uint8Array(44);pads[32]=pads[43]=255;
 const bridges=[{completedState:()=>null,pendingState:()=>pending?{}:null,replayCompletedPoll(options){
  replayed++;assert.equal(options.receipt.frame,'7');pending=false;
  setTimeout(()=>{host=captured(8n,99);pending=true;},15);return{frame:11,inputFrame:'8',checkpoint:99,frand:3,rand8:5};}},
 {completedState:()=>({frame:7n,checkpoint:42,frand:3,rand8:5,pads}),pendingState:()=>({}),replayCompletedPoll(){throw Error('Ahead peer must remain paused');}}];
 const options=channels.map((channel,index)=>({channel,identity,nonce,peer:index?'guest1':'host',remotePeer:index?'host':'guest1',capture:()=>index?guest:host,current:()=>true,bridge:bridges[index],deadline:performance.now()+1000}));
 const results=await Promise.all(options.map(reconcilePausedPoll));assert.equal(replayed,1);assert.equal(results[0].checkpoint,99);
 // Strict state verification still runs as a distinct phase.
 const verified=await Promise.all(options.map(agreePausedState));assert.equal(verified.length,2);
});
test('larger skew, altered identity and expired original deadline fail reconciliation',async()=>{
 for(const mode of ['gap','identity','expiry']){
  const channels=pair(),controller=new AbortController(),values=[captured(),captured(mode==='gap'?9n:7n)];
  const results=await Promise.allSettled(channels.map((channel,index)=>reconcilePausedPoll({channel,identity:index&&mode==='identity'?{...identity,buildId:'d'.repeat(64)}:identity,
   nonce,peer:index?'guest1':'host',remotePeer:index?'host':'guest1',capture:()=>values[index],current:()=>true,
   bridge:{completedState:()=>null,pendingState:()=>({}),replayCompletedPoll(){throw Error('Rejected replay');}},signal:controller.signal,
   deadline:mode==='expiry'?0:performance.now()+300})));
  assert.ok(results.every(result=>result.status==='rejected'));
 }
});
test('protocol recovers a real retained lockstep poll without resuming the next poll',async()=>{
 const channels=pair(),session=new Uint8Array(16).fill(1),owners=[11,22,0,0],queues=owners.slice(0,2).map(()=>new SessionInput(session,owners));
 const outgoing=[[],[]],bridges=queues.map((queue,index)=>createSessionInputExchange(queue,{session,owners,peer:owners[index],suspendOnPause:true,
  checkpoint:options=>options.checkpoint,send:packet=>outgoing[index].push(packet),now:()=>0}));
 const pads=new Uint8Array(44);pads.fill(7,0,11);pads.fill(9,11,22);pads[32]=pads[43]=255;
 const waits=bridges.map(bridge=>bridge.exchange(pads,{checkpoint:42,frand:3,rand8:5}));
 // Deliver host's packet to the guest, but lose guest's packet to the host.
 queues[1].receive(11,outgoing[0][0],0);bridges[1].notify();await waits[1];
 let frames=[0,1],registered=false;
 const nextGuest=bridges[1].exchange(pads,{checkpoint:99,frand:3,rand8:5});queues[0].disconnected(22,0);queues[1].disconnected(11,0);
 let nextHost;
 waits[0].then(()=>{frames[0]++;nextHost=bridges[0].exchange(pads,{checkpoint:99,frand:3,rand8:5});registered=true;});
 const capture=index=>{const input=queues[index].status(0),pending=bridges[index].pendingState();
  assert.ok(pending);return{input,attempt:{peer:owners[1-index],epoch:input.epoch,revision:input.revision,...pending},
   state:{frame:frames[index],checkpoint:pending.checkpoint,frand:pending.frand,rand8:pending.rand8}};};
 const options=channels.map((channel,index)=>({channel,identity,nonce,peer:index?'guest1':'host',remotePeer:index?'host':'guest1',bridge:bridges[index],
  capture:()=>capture(index),current:()=>queues[index].status(0).state==='Paused',deadline:performance.now()+1000}));
 const result=await Promise.all(options.map(reconcilePausedPoll));assert.equal(registered,true);
 assert.deepEqual(frames,[1,1]);assert.equal(result[0].checkpoint,99);
 for(const queue of queues){assert.equal(queue.status(0).frame,1n);assert.equal(queue.status(0).state,'Paused');assert.equal(queue.status(0).remainingMs,60000);}
 await Promise.all(options.map(agreePausedState));
 for(const queue of queues)queue.cancel();for(const bridge of bridges)bridge.notify();assert.equal(await nextHost,null);assert.equal(await nextGuest,null);
});
test('a replay reaching the target frame with different actual state is rejected',async()=>{
 const channels=pair();let host=captured(),pending=true;
 const guest=captured(8n,99),pads=new Uint8Array(44);pads[32]=pads[43]=255;
 const bridges=[{completedState:()=>null,pendingState:()=>pending?{}:null,replayCompletedPoll(){
  pending=false;setTimeout(()=>{host=captured(8n,100);pending=true;},5);
  return{frame:11,inputFrame:'8',checkpoint:99,frand:3,rand8:5};}},
 {completedState:()=>({frame:7n,checkpoint:42,frand:3,rand8:5,pads}),pendingState:()=>({}),replayCompletedPoll(){throw Error('Unexpected replay');}}];
 const results=await Promise.allSettled(channels.map((channel,index)=>reconcilePausedPoll({channel,identity,nonce,
  peer:index?'guest1':'host',remotePeer:index?'host':'guest1',capture:()=>index?guest:host,current:()=>true,bridge:bridges[index],deadline:performance.now()+100})));
 assert.equal(results[0].status,'rejected');assert.match(results[0].reason.message,/actual state differs/);assert.equal(results[1].status,'rejected');
});
