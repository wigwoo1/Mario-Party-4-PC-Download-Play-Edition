import test from 'node:test';import assert from 'node:assert/strict';
import {agreePausedState,consumePausedStateAgreement,pausedStateReport} from './paused-state-agreement.mjs';
const identity={session:'01'.repeat(16),buildId:'b'.repeat(64),assetId:'a'.repeat(64),profile:'unlocked-no-save-v1',seed:0,revision:8,membership:4,players:[{peer:'host',slot:0},{peer:'guest1',slot:1}]},nonce='c'.repeat(32);
const captured=(revision=1n)=>({attempt:{peer:1,epoch:1n,revision,frame:7n,checkpoint:42,frand:0,rand8:5},state:{frame:10,checkpoint:42,frand:0,rand8:5},input:{state:'Paused',frame:7n,epoch:1n,revision}});
class Channel extends EventTarget{label='partyboard-recovery-v1';readyState='open';ordered=true;maxRetransmits=null;maxPacketLifeTime=null;
 send(data){queueMicrotask(()=>this.other.dispatchEvent(new MessageEvent('message',{data})));}
 close(){this.readyState='closed';this.dispatchEvent(new Event('close'));}}
function pair(){const a=new Channel(),b=new Channel();a.other=b;b.other=a;return [a,b];}
test('two-sided exact state reports tolerate different local policy revisions and produce one-use current-state receipts',async()=>{
 const [a,b]=pair(),host=captured(),guest=captured(3n);
 const [receipt]=await Promise.all([agreePausedState({channel:a,identity,peer:'host',remotePeer:'guest1',nonce,capture:()=>host,current:()=>true,deadline:1000,now:()=>0}),
  agreePausedState({channel:b,identity,peer:'guest1',remotePeer:'host',nonce,capture:()=>guest,current:()=>true,deadline:1000,now:()=>0})]);
 const options={channel:a,identity,captured:host,peer:'host',remotePeer:'guest1',nonce};
 assert.equal(consumePausedStateAgreement({...receipt},options),false);
 assert.equal(consumePausedStateAgreement(receipt,{...options,captured:{...host,state:{...host.state,frame:11}}}),false);
 assert.equal(consumePausedStateAgreement(receipt,options),true);assert.equal(consumePausedStateAgreement(receipt,options),false);
});
test('different actual game frames reject both peers without issuing agreement',async()=>{
 const [a,b]=pair(),host=captured(),guest=captured();guest.state.frame++;
 const results=await Promise.allSettled([agreePausedState({channel:a,identity,peer:'host',remotePeer:'guest1',nonce,capture:()=>host,current:()=>true,deadline:1000,now:()=>0}),
  agreePausedState({channel:b,identity,peer:'guest1',remotePeer:'host',nonce,capture:()=>guest,current:()=>true,deadline:1000,now:()=>0})]);
 assert.ok(results.every(result=>result.status==='rejected'&&/differs/.test(result.reason.message)));
});
test('changed attempts, aborts and monotonic expiry reject an otherwise silent channel',async()=>{
 const [channel]=pair(),value=captured(),controller=new AbortController();
 const waiting=agreePausedState({channel,identity,peer:'host',remotePeer:'guest1',nonce,capture:()=>value,current:()=>true,signal:controller.signal,deadline:1000,now:()=>0});
 controller.abort(Error('Session stopped'));await assert.rejects(waiting,/Session stopped/);
 await assert.rejects(agreePausedState({channel,identity,peer:'host',remotePeer:'guest1',nonce,capture:()=>value,current:()=>false,deadline:1000,now:()=>0}),/changed/);
 await assert.rejects(agreePausedState({channel,identity,peer:'host',remotePeer:'guest1',nonce,capture:()=>value,current:()=>true,deadline:1000,now:()=>1000}),/deadline/);
});
test('report rejects state that no longer matches pending input metadata',()=>{
 const value=captured();value.state.frand=1;assert.throws(()=>pausedStateReport(identity,value,{peer:'host',nonce}),/Current suspended/);
});
