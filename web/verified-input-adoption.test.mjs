import test from 'node:test';import assert from 'node:assert/strict';
import {agreePausedState} from './paused-state-agreement.mjs';import {adoptVerifiedInput} from './verified-input-adoption.mjs';
import {SessionInput} from './session-input.mjs';import {createSessionInputExchange} from './session-input-exchange.mjs';
import {createLobbyInputRouter} from './lobby-input-router.mjs';import {lobbyInputOwnership} from './lobby-input-ownership.mjs';
const identity={session:'01'.repeat(16),buildId:'b'.repeat(64),assetId:'a'.repeat(64),profile:'unlocked-no-save-v1',seed:0,revision:8,membership:4,players:[{peer:'host',slot:0},{peer:'guest1',slot:1}]},nonce='c'.repeat(32);
class Channel extends EventTarget{readyState='open';ordered=true;maxRetransmits=null;maxPacketLifeTime=null;constructor(label){super();this.label=label;}send(data){if(this.other)queueMicrotask(()=>this.other.dispatchEvent(new MessageEvent('message',{data})));}}
test('verified adoption retains original paused queue/poll and rejects forged or already consumed receipts',async()=>{
 const session=new Uint8Array(16).fill(1),queue=new SessionInput(session,[1,2,0,0]);
 const bridge=createSessionInputExchange(queue,{session,owners:[1,2,0,0],peer:1,checkpoint:()=>42,send(){},suspendOnPause:true,now:()=>0});
 const waiting=bridge.exchange(new Uint8Array(44),{frand:0,rand8:5});queue.disconnected(2,0);
 const capture=()=>({attempt:{peer:2,epoch:queue.status(0).epoch,revision:queue.status(0).revision,...bridge.pendingState()},input:queue.status(0),state:{frame:10,checkpoint:42,frand:0,rand8:5}});
 const a=new Channel('partyboard-recovery-v1'),b=new Channel('partyboard-recovery-v1');a.other=b;b.other=a;
 const [receipt]=await Promise.all([agreePausedState({channel:a,identity,peer:'host',remotePeer:'guest1',nonce,capture,current:()=>true,deadline:1000,now:()=>0}),
  agreePausedState({channel:b,identity,peer:'guest1',remotePeer:'host',nonce,capture,current:()=>true,deadline:1000,now:()=>0})]);
 const old=new Channel('partyboard-input-v1'),replacement=new Channel('partyboard-input-v1');
 const router=createLobbyInputRouter({plan:lobbyInputOwnership({membership:4,players:identity.players},'host'),session,host:true,channels:new Map([['guest1',old]]),receive(){}});
 const options={receipt,identity,peer:'host',remotePeer:'guest1',nonce,input:replacement,control:a,router,capture,current:()=>true};
 assert.throws(()=>adoptVerifiedInput({...options,receipt:{}}),/agreement required/);
 replacement.readyState='closed';assert.throws(()=>adoptVerifiedInput(options),/reliable replacement/);replacement.readyState='open';
 const adopted=adoptVerifiedInput(options);assert.equal(adopted.previous,old);assert.equal(queue.status(0).state,'Paused');assert.equal(bridge.pendingState().frame,0n);
 assert.throws(()=>adoptVerifiedInput(options),/agreement required/);queue.cancel();bridge.notify();assert.equal(await waiting,null);router.close();
});
