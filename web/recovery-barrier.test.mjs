import test from 'node:test';import assert from 'node:assert/strict';
import {RecoveryBarrier,recoveryAcknowledgment} from './recovery-barrier.mjs';
const identity={session:'01'.repeat(16),buildId:'b'.repeat(64),assetId:'a'.repeat(64),profile:'unlocked-no-save-v1',seed:0,revision:8,membership:4,players:[{peer:'host',slot:0},{peer:'g1',slot:1},{peer:'g2',slot:2},{peer:'g3',slot:3}]};
const captured=(revision=1n)=>({attempt:{peer:1,epoch:1n,revision,frame:7n,checkpoint:42,frand:0,rand8:5},state:{frame:10,checkpoint:42,frand:0,rand8:5},input:{state:'Paused',frame:7n,epoch:1n,revision}});
function fixture(){let active=true,time=0;const state=captured();const barrier=new RecoveryBarrier({identity,capture:()=>state,current:()=>active,host:'host',nonce:'c'.repeat(32),deadline:60000,now:()=>time});return {barrier,state,deactivate:()=>active=false,expire:()=>time=60000};}
test('four assigned players must agree before a one-use commit, despite different local revisions',()=>{
 const {barrier}=fixture();assert.throws(()=>barrier.commit(),/Every assigned/);
 for(const [index,peer] of ['g1','g2','g3'].entries()){
  const ack=recoveryAcknowledgment(barrier.proposal,identity,captured(3n),{peer,host:'host'});
  assert.equal(barrier.acknowledge(peer,ack),index===2);
  if(index<2)assert.throws(()=>barrier.commit(),/Every assigned/);
 }
 const commit=barrier.commit();assert.deepEqual(commit.acknowledged,['host','g1','g2','g3']);assert.equal(commit.inputFrame,'7');assert.throws(()=>barrier.commit(),/expired/);
});
test('wrong state, round, bound identity and repeated acknowledgments cannot satisfy missing players',()=>{
 const {barrier}=fixture(),wrong=captured();wrong.state.frame++;
 assert.throws(()=>recoveryAcknowledgment(barrier.proposal,identity,wrong,{peer:'g1',host:'host'}),/differs/);
 const ack=recoveryAcknowledgment(barrier.proposal,identity,captured(),{peer:'g1',host:'host'});
 assert.throws(()=>barrier.acknowledge('g2',ack),/Bound/);
 barrier.acknowledge('g1',ack);barrier.acknowledge('g1',ack);assert.deepEqual(barrier.pending,['g2','g3']);
 const stale=structuredClone(ack);stale.accepted.nonce='d'.repeat(32);assert.throws(()=>barrier.acknowledge('g1',stale),/Bound/);
});
test('actual state drift, stale local attempt, cancellation and original deadline invalidate rounds',()=>{
 for(const action of [f=>f.state.state.frame++,f=>f.deactivate(),f=>f.expire(),f=>f.barrier.cancel()]){
  const f=fixture();action(f);assert.throws(()=>f.barrier.commit(),/expired/);
 }
});
