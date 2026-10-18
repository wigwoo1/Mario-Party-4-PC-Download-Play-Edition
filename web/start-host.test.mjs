import {test} from 'node:test';
import assert from 'node:assert/strict';
import {StartBarrier} from './start-barrier.mjs';
import {prepareAgreedHostCommit,resumeAgreedHostRuntime} from './start-host.mjs';
function fixture() {
  const snapshot={revision:1,membership:1,total:100,assetId:'a'.repeat(64),players:
    ['host','guest'].map((peer,slot)=>({peer,slot,bytes:100,verified:true,shaders:true,ready:true}))};
  const barrier=new StartBarrier(snapshot,{session:new Uint8Array(16).fill(1),seed:7,buildId:'b'.repeat(64),checkpoint:42});
  let resumed=0;
  const runtime={sessionSeed:7,checkpoint:42,engine:{phase:'Idle',preparedCheckpoint:()=>42,resume(){this.phase='Running';resumed++;return Promise.resolve();}}};
  const ack=peer=>barrier.acknowledge(peer,{...barrier.proposal,type:'session-start-ack'},snapshot);
  return {snapshot,barrier,runtime,ack,count:()=>resumed};
}
test('host stays paused until all acknowledgments and resumes synchronously once',async()=>{
  const f=fixture();f.ack('host');assert.throws(()=>resumeAgreedHostRuntime(f.barrier,f.runtime,()=>f.snapshot));
  assert.equal(f.count(),0);f.ack('guest');const running=resumeAgreedHostRuntime(f.barrier,f.runtime,()=>f.snapshot);
  assert.equal(f.count(),1);assert.throws(()=>resumeAgreedHostRuntime(f.barrier,f.runtime,()=>f.snapshot));await running;
});

test('online host cannot prepare delivery or execute without required input hook',()=>{
  const f=fixture();f.ack('host');f.ack('guest');const options={requireOnlineInput:true};
  assert.throws(()=>prepareAgreedHostCommit(f.barrier,f.runtime,()=>f.snapshot,options),/online input hook/);
  assert.throws(()=>resumeAgreedHostRuntime(f.barrier,f.runtime,()=>f.snapshot,options),/online input hook/);
  assert.equal(f.count(),0);
  f.runtime.inputExchangeInstalled=true;f.runtime.module={partyBoardInputExchange:()=>{},
    partyBoardCancelInputExchange:()=>{},_PartyBoardBrowserOnlineInputRequired:()=>1};
  assert.equal(prepareAgreedHostCommit(f.barrier,f.runtime,()=>f.snapshot,options).type,'session-start-commit');
  f.runtime.module._PartyBoardBrowserOnlineInputRequired=()=>0;
  assert.throws(()=>resumeAgreedHostRuntime(f.barrier,f.runtime,()=>f.snapshot,options),/online input hook/);
  assert.equal(f.count(),0);
});

test('host runtime build must match agreement before delivery or execution',()=>{
  const f=fixture();f.ack('host');f.ack('guest');f.runtime.buildId='c'.repeat(64);
  assert.throws(()=>prepareAgreedHostCommit(f.barrier,f.runtime,()=>f.snapshot),/runtime build differs/);
  assert.throws(()=>resumeAgreedHostRuntime(f.barrier,f.runtime,()=>f.snapshot),/runtime build differs/);
  assert.equal(f.count(),0);f.runtime.buildId=f.barrier.proposal.buildId;
  assert.equal(prepareAgreedHostCommit(f.barrier,f.runtime,()=>f.snapshot).buildId,f.runtime.buildId);
});
test('lobby changes after agreement cancel start before execution',()=>{
  const f=fixture();f.ack('host');f.ack('guest');f.snapshot.revision++;
  assert.throws(()=>resumeAgreedHostRuntime(f.barrier,f.runtime,()=>f.snapshot));assert.equal(f.count(),0);assert.equal(f.barrier.state,'Cancelled');
});
test('seed, captured checkpoint and current engine checkpoint must match agreement',()=>{
  for(const changed of ['seed','captured','current']) {
    const f=fixture();f.ack('host');f.ack('guest');
    if(changed==='seed')f.runtime.sessionSeed=8;
    if(changed==='captured')f.runtime.checkpoint=43;
    if(changed==='current')f.runtime.engine.preparedCheckpoint=()=>43;
    assert.throws(()=>resumeAgreedHostRuntime(f.barrier,f.runtime,()=>f.snapshot));assert.equal(f.count(),0);
  }
});

test('foreign mounted host asset cannot start despite all player acknowledgments',async()=>{
  const f=fixture();f.ack('host');f.ack('guest');f.runtime.assetId='c'.repeat(64);
  assert.throws(()=>resumeAgreedHostRuntime(f.barrier,f.runtime,()=>f.snapshot),/runtime asset differs/);
  assert.equal(f.count(),0);assert.equal(f.runtime.engine.phase,'Idle');
  f.runtime.assetId=f.snapshot.assetId;
  await resumeAgreedHostRuntime(f.barrier,f.runtime,()=>f.snapshot);assert.equal(f.count(),1);
});

test('host commit requires every acknowledgment and keeps the engine idle',()=>{
  const f=fixture();f.ack('host');
  assert.throws(()=>prepareAgreedHostCommit(f.barrier,f.runtime,()=>f.snapshot));
  f.ack('guest');const commit=prepareAgreedHostCommit(f.barrier,f.runtime,()=>f.snapshot);
  assert.deepEqual(commit,{...f.barrier.proposal,type:'session-start-commit'});
  assert.equal(f.count(),0);assert.equal(f.runtime.engine.phase,'Idle');
  commit.players[0].peer='changed';commit.seed=99;
  assert.equal(f.barrier.proposal.players[0].peer,'host');
  assert.equal(f.barrier.proposal.seed,7);
});

test('preparing a commit does not bypass later host state validation',()=>{
  for(const changed of ['asset','seed','checkpoint','phase','lobby']) {
    const f=fixture();f.ack('host');f.ack('guest');
    prepareAgreedHostCommit(f.barrier,f.runtime,()=>f.snapshot);
    if(changed==='asset')f.runtime.assetId='c'.repeat(64);
    if(changed==='seed')f.runtime.sessionSeed=8;
    if(changed==='checkpoint')f.runtime.engine.preparedCheckpoint=()=>43;
    if(changed==='phase')f.runtime.engine.phase='Running';
    if(changed==='lobby')f.snapshot.revision++;
    assert.throws(()=>prepareAgreedHostCommit(f.barrier,f.runtime,()=>f.snapshot));
    assert.throws(()=>resumeAgreedHostRuntime(f.barrier,f.runtime,()=>f.snapshot));
    assert.equal(f.count(),0);
  }
});
