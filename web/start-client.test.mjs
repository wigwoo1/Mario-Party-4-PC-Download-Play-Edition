import {test} from 'node:test';
import assert from 'node:assert/strict';
import {StartBarrier} from './start-barrier.mjs';
import {prepareStartAcknowledgment} from './start-client.mjs';
function fixture() {
  const snapshot={revision:4,membership:2,total:100,assetId:'a'.repeat(64),players:
    ['host','guest'].map((peer,slot)=>({peer,slot,bytes:100,verified:true,shaders:true,ready:true}))};
  const barrier=new StartBarrier(snapshot,{session:new Uint8Array(16).fill(1),seed:1,
    buildId:'b'.repeat(64),checkpoint:42});
  return {snapshot,barrier,options:{peer:'guest',buildId:'b'.repeat(64),snapshot:()=>snapshot}};
}

test('required online input rejects acknowledgment and a hook removed before commit',async()=>{
  const {barrier,options}=fixture();let disposed=0,runs=0;
  const runtime={checkpoint:42,sessionSeed:1,dispose:async()=>disposed++,
    engine:{phase:'Idle',preparedCheckpoint:()=>42,resume:()=>runs++}};
  const online={...options,requireOnlineInput:true};
  await assert.rejects(prepareStartAcknowledgment(barrier.proposal,{...online,prepare:async()=>runtime}),/online input hook/);
  assert.equal(disposed,1);
  runtime.inputExchangeInstalled=true;runtime.module={partyBoardInputExchange:()=>{},
    partyBoardCancelInputExchange:()=>{},_PartyBoardBrowserOnlineInputRequired:()=>1};
  const accepted=await prepareStartAcknowledgment(barrier.proposal,{...online,prepare:async()=>runtime});
  delete runtime.module.partyBoardInputExchange;
  assert.throws(()=>resumeCommittedClientRuntime({...barrier.proposal,type:'session-start-commit'},accepted,online),/online input hook/);
  assert.equal(runs,0);
});

test('foreign runtime build cannot acknowledge or resume despite matching wire identity',async()=>{
  const {barrier,options}=fixture();let disposed=0,runs=0;
  const runtime={buildId:'c'.repeat(64),sessionSeed:1,checkpoint:42,dispose:async()=>disposed++,
    engine:{phase:'Idle',preparedCheckpoint:()=>42,resume:()=>runs++}};
  await assert.rejects(prepareStartAcknowledgment(barrier.proposal,{...options,prepare:async()=>runtime}),/runtime build differs/);
  assert.equal(disposed,1);
  const prepared={acknowledgment:{...barrier.proposal,type:'session-start-ack'},runtime};
  const commit={...barrier.proposal,type:'session-start-commit'};
  assert.throws(()=>resumeCommittedClientRuntime(commit,prepared,options),/runtime build differs/);
  assert.equal(runs,0);runtime.buildId=options.buildId;
  resumeCommittedClientRuntime(commit,prepared,options);assert.equal(runs,1);
});

test('mounted runtime with foreign asset cannot acknowledge and is disposed',async()=>{
  const {barrier,options}=fixture();let disposed=0;
  await assert.rejects(prepareStartAcknowledgment(barrier.proposal,{...options,
    prepare:async()=>({assetId:'c'.repeat(64),checkpoint:42,dispose:async()=>disposed++})}),/runtime asset differs/);
  assert.equal(disposed,1);
});

test('prepared engine with wrong agreed seed cannot acknowledge and is disposed',async()=>{
  const {barrier,options}=fixture();let disposed=0;
  await assert.rejects(prepareStartAcknowledgment(barrier.proposal,{...options,
    prepare:async()=>({sessionSeed:2,checkpoint:42,dispose:async()=>disposed++})}),/runtime seed differs/);
  assert.equal(disposed,1);
});

test('running or changed prepared engine cannot acknowledge and is disposed',async()=>{
  for(const changed of ['running','checkpoint']) {
    const {barrier,options}=fixture();let disposed=0;
    await assert.rejects(prepareStartAcknowledgment(barrier.proposal,{...options,
      prepare:async()=>({sessionSeed:1,checkpoint:42,dispose:async()=>disposed++,
        engine:{phase:changed==='running'?'Running':'Idle',preparedCheckpoint:()=>changed==='checkpoint'?43:42}})}),/not idle/);
    assert.equal(disposed,1);
  }
});

test('mounted runtime asset is rechecked before committed start resumes engine',()=>{
  const {barrier,options}=fixture();let runs=0;
  const prepared={acknowledgment:{...barrier.proposal,type:'session-start-ack'},runtime:{
    assetId:'c'.repeat(64),sessionSeed:1,checkpoint:42,
    engine:{phase:'Idle',preparedCheckpoint:()=>42,resume:()=>runs++},
  }};
  const commit={...barrier.proposal,type:'session-start-commit'};
  assert.throws(()=>resumeCommittedClientRuntime(commit,prepared,options),/runtime asset differs/);
  assert.equal(runs,0);prepared.runtime.assetId=barrier.proposal.assetId;
  resumeCommittedClientRuntime(commit,prepared,options);assert.equal(runs,1);
});
test('acknowledges actual prepared checkpoint and retains runtime for caller',async()=>{
  const {barrier,options}=fixture();let disposed=0;
  const result=await prepareStartAcknowledgment(barrier.proposal,{...options,prepare:async proposal=>{
    proposal.seed=9;return {checkpoint:42,dispose:async()=>disposed++};
  }});
  assert.equal(result.acknowledgment.seed,1);assert.equal(result.acknowledgment.type,'session-start-ack');
  assert.equal(disposed,0);await result.runtime.dispose();assert.equal(disposed,1);
});
test('cancellation while initialization is pending disposes late runtime and never acknowledges',async()=>{
  const {barrier,options}=fixture(),abort=new AbortController();let finish,disposed=0;
  const pending=prepareStartAcknowledgment(barrier.proposal,{...options,signal:abort.signal,
    prepare:()=>new Promise(resolve=>finish=resolve)});
  abort.abort();finish({checkpoint:42,dispose:async()=>disposed++});
  await assert.rejects(pending,{name:'AbortError'});assert.equal(disposed,1);
});
test('membership changes and different prepared state both release runtime',async()=>{
  for(const changedLobby of [false,true]) {
    const {snapshot,barrier,options}=fixture();let disposed=0;
    await assert.rejects(prepareStartAcknowledgment(barrier.proposal,{...options,prepare:async()=>{
      if(changedLobby)snapshot.revision++;
      return {checkpoint:changedLobby?42:43,dispose:async()=>disposed++};
    }}),changedLobby?/Lobby changed/:/state differs/);
    assert.equal(disposed,1);
  }
});
test('foreign build/assets and slot assignments are rejected before engine initialization',async()=>{
  const {barrier,options}=fixture();let started=0;
  for(const changes of [{buildId:'c'.repeat(64)},{assetId:'d'.repeat(64)},
    {players:[{peer:'host',slot:1},{peer:'guest',slot:0}]},{profile:'save-enabled'}])
    await assert.rejects(prepareStartAcknowledgment({...barrier.proposal,...changes},{...options,
      prepare:async()=>{started++;}}));
  assert.equal(started,0);
});
test('cleanup failure is visible alongside preparation failure',async()=>{
  const {barrier,options}=fixture();
  await assert.rejects(prepareStartAcknowledgment(barrier.proposal,{...options,prepare:async()=>({
    checkpoint:43,dispose:async()=>{throw Error('Cleanup failed');},
  })}),error=>error instanceof AggregateError&&error.errors.length===2&&/Cleanup failed/.test(error.errors[1].message));
});

// Preparation adapter integration: no simulation call before start agreement.
import {prepareEngineLifecycle} from './engine-lifecycle.mjs';
test('engine preparation feeds actual checkpoint to start acknowledgment without running',async()=>{
  const {barrier,options}=fixture();const calls=[];let cleaned=0;
  const result=await prepareStartAcknowledgment(barrier.proposal,{...options,prepare:proposal=>
    prepareEngineLifecycle({ccall:name=>{calls.push(name);return 0;},
      _PartyBoard_GameRequestStop(){},_PartyBoard_GameConfigureSessionSeed:seed=>{calls.push(seed);return 0;},
      _PartyBoard_GameAlignStartClock:()=>{calls.push('align');return 0;},
      _PartyBoard_GamePreparedCheckpoint:()=>42},
      {sessionSeed:proposal.seed,shutdown:async()=>cleaned++})});
  assert.deepEqual(calls,[1,'PartyBoard_GameInitialize','align']);
  assert.equal(result.runtime.engine.phase,'Idle');assert.equal(result.acknowledgment.checkpoint,42);
  assert.equal(cleaned,0);await result.runtime.dispose();await result.runtime.dispose();assert.equal(cleaned,1);
});
test('preparation cancellation waits for initialization then cleans up without alignment',async()=>{
  let finish,cleaned=0,aligned=false;const abort=new AbortController();
  const pending=prepareEngineLifecycle({ccall:()=>new Promise(resolve=>finish=resolve),
    _PartyBoard_GameRequestStop(){},_PartyBoard_GameConfigureSessionSeed:()=>0,
    _PartyBoard_GameAlignStartClock:()=>{aligned=true;return 0;}},
    {sessionSeed:0,signal:abort.signal,shutdown:async()=>cleaned++});
  abort.abort();assert.equal(cleaned,0);finish(0);
  await assert.rejects(pending,{name:'AbortError'});assert.equal(cleaned,1);assert.equal(aligned,false);
});
test('preparation alignment failure cleans up and preserves cleanup failure evidence',async()=>{
  for(const failsCleanup of [false,true]) {
    let cleaned=0;
    const pending=prepareEngineLifecycle({ccall:()=>0,_PartyBoard_GameRequestStop(){},
      _PartyBoard_GameConfigureSessionSeed:()=>0,_PartyBoard_GameAlignStartClock:()=>-1},
      {sessionSeed:1,shutdown:async()=>{cleaned++;if(failsCleanup)throw Error('cleanup unavailable');}});
    await assert.rejects(pending,error=>failsCleanup?error instanceof AggregateError&&error.errors.length===2:/alignment/.test(error.message));
    assert.equal(cleaned,1);
  }
});
test('pre-aborted or missing-seed preparation never initializes and releases owned platform',async()=>{
  for(const canceled of [false,true]) {
    const abort=new AbortController();if(canceled)abort.abort();let initialized=false,cleaned=0;
    await assert.rejects(prepareEngineLifecycle({ccall:()=>{initialized=true;return 0;},
      _PartyBoard_GameRequestStop(){},_PartyBoard_GameConfigureSessionSeed:()=>0},
      {sessionSeed:canceled?1:undefined,signal:abort.signal,shutdown:async()=>cleaned++}));
    assert.equal(initialized,false);assert.equal(cleaned,1);
  }
});

import {resumeCommittedClientRuntime} from './start-client.mjs';
test('client commit binds every accepted identity field and current runtime before execution',async()=>{
  const {barrier,options,snapshot}=fixture();let starts=0;
  const prepared={acknowledgment:{...barrier.proposal,type:'session-start-ack'},runtime:{sessionSeed:1,checkpoint:42,
    engine:{phase:'Idle',preparedCheckpoint:()=>42,resume(){this.phase='Running';starts++;return Promise.resolve();}}}};
  const commit={...barrier.proposal,type:'session-start-commit'};
  for(const change of [{seed:2},{checkpoint:43},{session:'02'.repeat(16)},{revision:5},{buildId:'c'.repeat(64)},
    {assetId:'d'.repeat(64)},{profile:'save-enabled'},{players:[]}])
    assert.throws(()=>resumeCommittedClientRuntime({...commit,...change},prepared,options));
  assert.equal(starts,0);snapshot.revision++;
  assert.throws(()=>resumeCommittedClientRuntime(commit,prepared,options));snapshot.revision--;
  await resumeCommittedClientRuntime(commit,prepared,options);assert.equal(starts,1);
  assert.throws(()=>resumeCommittedClientRuntime(commit,prepared,options));
});
test('client commit rejects changed seed or engine checkpoint without resuming',()=>{
  for(const change of ['seed','state']) {
    const {barrier,options}=fixture();let starts=0;
    const prepared={acknowledgment:{...barrier.proposal,type:'session-start-ack'},runtime:{sessionSeed:change==='seed'?2:1,
      checkpoint:42,engine:{phase:'Idle',preparedCheckpoint:()=>change==='state'?43:42,resume(){starts++;}}}};
    assert.throws(()=>resumeCommittedClientRuntime({...barrier.proposal,type:'session-start-commit'},prepared,options));assert.equal(starts,0);
  }
});
