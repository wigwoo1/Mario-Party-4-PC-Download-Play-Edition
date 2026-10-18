import test from 'node:test';import assert from 'node:assert/strict';
import {RecoveryBarrier,recoveryAcknowledgment} from './recovery-barrier.mjs';
import {resumeCommittedInput} from './committed-input-recovery.mjs';
import {SessionInput} from './session-input.mjs';
import {createSessionInputExchange} from './session-input-exchange.mjs';
const identity={session:'01'.repeat(16),buildId:'b'.repeat(64),assetId:'a'.repeat(64),profile:'unlocked-no-save-v1',seed:0,revision:8,membership:4,players:[{peer:'host',slot:0},{peer:'g1',slot:1},{peer:'g2',slot:2},{peer:'g3',slot:3}]};
function fixture(){
 const queue=new SessionInput(new Uint8Array(16).fill(1),[1,2,3,4]);queue.disconnected(1,0);queue.disconnected(3,0);queue.disconnected(4,0);
 const capture=()=>{const input=queue.status(0);return {input,attempt:{peer:1,epoch:input.epoch,revision:input.revision,frame:input.frame,checkpoint:42,frand:0,rand8:5},state:{frame:10,checkpoint:42,frand:0,rand8:5}};};
 const barrier=new RecoveryBarrier({identity,capture,current:()=>true,host:'host',nonce:'c'.repeat(32),deadline:60000,now:()=>0});
 const acknowledged=barrier.proposal;
 for(const peer of ['g1','g2','g3'])barrier.acknowledge(peer,recoveryAcknowledgment(acknowledged,identity,capture(),{peer,host:'host'}));
 const message=barrier.commit();let resend=0,notify=0;
 const options={message,acknowledged,identity,peer:'g1',host:'host',capture,current:()=>true,queue,bridge:{resend(){resend++;},notify(){notify++;}},transportReady:()=>true,now:()=>0};
 return {options,queue,counts:()=>({resend,notify})};
}
test('verified all-player commit resumes original queue and resends retained input once',()=>{
 const f=fixture();assert.equal(resumeCommittedInput(f.options),true);assert.equal(f.queue.status(0).frame,0n);assert.deepEqual(f.counts(),{resend:1,notify:1});
 assert.throws(()=>resumeCommittedInput(f.options));assert.deepEqual(f.counts(),{resend:1,notify:1});
});
test('missing acknowledgments, wrong round, unavailable routes and stale attempt preserve pause',()=>{
 for(const change of [o=>o.message.acknowledged.pop(),o=>o.message.nonce='d'.repeat(32),o=>o.transportReady=()=>false,o=>o.current=()=>false]){
  const f=fixture();change(f.options);assert.throws(()=>resumeCommittedInput(f.options));assert.equal(f.queue.status(0).state,'Paused');assert.deepEqual(f.counts(),{resend:0,notify:0});
 }
});
test('original 60-second deadline cannot be extended by commit receipt',()=>{
 const f=fixture();f.options.now=()=>60000;assert.throws(()=>resumeCommittedInput(f.options),/stale or expired/);assert.equal(f.queue.status(60000).state,'ReturnToMenu');assert.deepEqual(f.counts(),{resend:0,notify:0});
});
test('four real suspended input bridges retain PAD bytes and release the same polls after agreed recovery',async()=>{
 const session=new Uint8Array(16).fill(1),owners=[1,2,3,4],players=identity.players;
 const peers=players.map((player,slot)=>{
  const queue=new SessionInput(session,owners),sent=[];
  const bridge=createSessionInputExchange(queue,{session,owners,peer:slot+1,localPadSlots:owners.map((_,lane)=>lane===slot?0:null),checkpoint:()=>42,send:bytes=>sent.push(bytes),suspendOnPause:true,now:()=>0});
  const pads=new Uint8Array(44);pads.fill(slot+11,0,11);
  const poll=bridge.exchange(pads,{frand:0,rand8:5});
  for(const owner of owners)if(owner!==slot+1)queue.disconnected(owner,0);
  const capture=()=>{const input=queue.status(0);return {input,attempt:{peer:owners.find(owner=>owner!==slot+1),epoch:input.epoch,revision:input.revision,...bridge.pendingState()},state:{frame:10,checkpoint:42,frand:0,rand8:5}};};
  return {player,slot,queue,bridge,poll,sent,capture};
 });
 const barrier=new RecoveryBarrier({identity,capture:peers[0].capture,current:()=>true,host:'host',nonce:'e'.repeat(32),deadline:60000,now:()=>0});
 const acknowledged=barrier.proposal;
 for(const peer of peers.slice(1))barrier.acknowledge(peer.player.peer,recoveryAcknowledgment(acknowledged,identity,peer.capture(),{peer:peer.player.peer,host:'host'}));
 const message=barrier.commit();
 // Delivery while paused fills lockstep lanes but cannot release any poll.
 for(const receiver of peers)for(const sender of peers)if(receiver!==sender){
  assert.equal(receiver.queue.receive(sender.slot+1,sender.sent[0],0),'Accepted');receiver.bridge.notify();
 }
 for(const peer of peers){assert.equal(peer.queue.status(0).state,'Paused');assert.equal(peer.bridge.pendingState().frame,0n);}
 for(const peer of peers)assert.equal(resumeCommittedInput({message,acknowledged,identity,peer:peer.player.peer,host:'host',capture:peer.capture,current:()=>true,queue:peer.queue,bridge:peer.bridge,transportReady:()=>true,now:()=>0}),true);
 const pads=await Promise.all(peers.map(peer=>peer.poll));
 for(const peer of peers){
  assert.equal(peer.queue.status(0).frame,1n);assert.equal(peer.bridge.pendingState(),null);
  assert.equal(peer.sent.length,2);assert.deepEqual(peer.sent[1],peer.sent[0]);
 }
 for(const bytes of pads)for(let slot=0;slot<4;slot++)assert.deepEqual(bytes.slice(slot*11,slot*11+11),new Uint8Array(11).fill(slot+11));
});

