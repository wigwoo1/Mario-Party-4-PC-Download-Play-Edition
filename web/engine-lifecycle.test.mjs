import {test} from 'node:test';
import assert from 'node:assert/strict';
import {createEngineLifecycle} from './engine-lifecycle.mjs';

test('runtime abort rejects pending execution and permits cleanup without its settlement',async()=>{
  let calls=0,cancelled=0,cleaned=0,previousReason;
  const previousAbort=reason=>{previousReason=reason;};
  const module={ccall:()=>++calls===1?0:new Promise(()=>{}),
    _PartyBoard_GameRequestStop(){throw Error('Aborted engine cannot stop');},
    partyBoardCancelInputExchange(){cancelled++;},onAbort:previousAbort};
  const engine=createEngineLifecycle(module,{shutdown:async()=>{cleaned++;}});
  await engine.initialize();const execution=engine.resume();
  module.onAbort('device lost');
  await assert.rejects(execution,/Engine aborted: device lost/);
  assert.equal(engine.phase,'Failed');assert.equal(cancelled,1);
  assert.equal(previousReason,'device lost');assert.throws(()=>engine.resume());
  await engine.dispose();assert.equal(cleaned,1);assert.equal(engine.phase,'Disposed');
  assert.equal(module.onAbort,previousAbort);
});
const deferred=()=>{let resolve,reject;const promise=new Promise((a,b)=>{resolve=a;reject=b;});return {promise,resolve,reject};};
test('renderer loss waits for orderly C exit before releasing resources',async()=>{
  const run=deferred();let calls=0,stops=0,cancelled=0,cleaned=0;
  const module={ccall:()=>++calls===1?0:run.promise,_PartyBoard_GameRequestStop(){stops++;},
    partyBoardCancelInputExchange(){cancelled++;}};
  const engine=createEngineLifecycle(module,{shutdown:async()=>{cleaned++;}});
  await engine.initialize();const execution=engine.resume();
  module.partyBoardRendererLost('WebGPU device lost');
  assert.equal(engine.phase,'Running');assert.equal(stops,1);assert.equal(cancelled,1);
  const rejected=assert.rejects(execution,/Renderer stopped/);
  const cleanup=engine.dispose();await Promise.resolve();assert.equal(cleaned,0);
  run.resolve(0);await rejected;await cleanup;
  assert.equal(cleaned,1);assert.equal(engine.phase,'Disposed');
  assert.equal(module.partyBoardRendererLost,undefined);
});
test('abort callback errors do not interrupt Emscripten terminal transition',async()=>{
  let previousCalled=false;
  const module={ccall:()=>new Promise(()=>{}),_PartyBoard_GameRequestStop(){},
    partyBoardCancelInputExchange(){throw Error('cancel failed');},
    onAbort(){previousCalled=true;throw Error('prior callback failed');}};
  const engine=createEngineLifecycle(module,{shutdown:async()=>{}});
  const initialization=engine.initialize();assert.doesNotThrow(()=>module.onAbort('device lost'));
  await assert.rejects(initialization,error=>{
    assert.match(error.message,/device lost/);assert.equal(error.callbackErrors.length,2);return true;
  });
  assert(previousCalled);assert.equal(engine.phase,'Failed');await engine.dispose();
});
test('abort cannot be overwritten by an already resolved core operation',async()=>{
  const module={ccall:()=>0,_PartyBoard_GameRequestStop(){}};
  const engine=createEngineLifecycle(module,{shutdown:async()=>{}});
  const initialization=engine.initialize();module.onAbort('GPU unavailable');
  await assert.rejects(initialization,/GPU unavailable/);
  assert.equal(engine.phase,'Failed');assert.throws(()=>engine.resume());
  await engine.dispose();assert.equal(engine.phase,'Disposed');
});
test('session seed is validated and applied before initialization, including zero and uint32 maximum',async()=>{
  for(const seed of [0,0xffffffff]) {
    const events=[];
    const engine=createEngineLifecycle({ccall:name=>{events.push(name);return 0;},
      _PartyBoard_GameConfigureSessionSeed:value=>{events.push(value);return 0;},_PartyBoard_GameRequestStop(){}},
      {sessionSeed:seed,shutdown:async()=>{}});
    await engine.initialize();assert.deepEqual(events,[seed,'PartyBoard_GameInitialize']);
    assert.throws(()=>engine.initialize());await engine.dispose();
  }
  for(const seed of [-1,0x100000000,1.5,NaN,'1',null])
    assert.throws(()=>createEngineLifecycle({ccall(){},_PartyBoard_GameRequestStop(){}},
      {sessionSeed:seed,shutdown(){}}),/Session seed/);
  assert.throws(()=>createEngineLifecycle({ccall(){},_PartyBoard_GameRequestStop(){}},
    {sessionSeed:1,shutdown(){}}),/seed export/);
});
test('rejected session seed prevents initialization and resume but preserves cleanup',async()=>{
  for(const reject of [()=>-1,()=>{throw Error('seed unavailable');}]) {
    let initialized=false,cleaned=false;
    const engine=createEngineLifecycle({ccall:()=>{initialized=true;return 0;},
      _PartyBoard_GameConfigureSessionSeed:reject,_PartyBoard_GameRequestStop(){}},
      {sessionSeed:1,shutdown:async()=>{cleaned=true;}});
    await assert.rejects(engine.initialize());assert.equal(engine.phase,'Failed');
    assert.equal(initialized,false);assert.throws(()=>engine.resume());
    await engine.dispose();assert.equal(cleaned,true);
  }
});
test('serialize initialization and runs; disposal stops and waits before cleanup',async()=>{
  const init=deferred(),run=deferred(),events=[];
  const engine=createEngineLifecycle({ccall(name,type,argTypes,args,options){
    assert.deepEqual(options,{async:true});events.push(name);return name.endsWith('Initialize')?init.promise:run.promise;
  },_PartyBoard_GameRequestStop(){events.push('stop');}}, {shutdown:async()=>{events.push('shutdown');}});
  assert.throws(()=>engine.resume());const preparation=engine.initialize();
  assert.throws(()=>engine.initialize());assert.throws(()=>engine.resume());
  init.resolve(0);await preparation;const execution=engine.resume();assert.throws(()=>engine.resume());
  const cleanup=engine.dispose();assert.equal(engine.dispose(),cleanup);assert.equal(events.at(-1),'stop');
  await Promise.resolve();assert.ok(!events.includes('shutdown'));assert.throws(()=>engine.resume());
  run.resolve(0);await execution;await cleanup;await engine.dispose();
  assert.equal(engine.phase,'Disposed');assert.equal(events.filter(e=>e==='shutdown').length,1);
});
test('dispose during preparation waits; failed shutdown retains retry ownership',async()=>{
  const init=deferred();let shutdowns=0;
  const engine=createEngineLifecycle({ccall:()=>init.promise,_PartyBoard_GameRequestStop(){}},
    {shutdown:async()=>{if(++shutdowns===1)throw Error('busy');}});
  const preparation=engine.initialize(),cleanup=engine.dispose();await Promise.resolve();assert.equal(shutdowns,0);
  init.resolve(0);await preparation;await assert.rejects(cleanup,/busy/);assert.throws(()=>engine.resume());
  await engine.dispose();assert.equal(engine.phase,'Disposed');assert.equal(shutdowns,2);
});
test('failed engine calls cannot resume, but cleanup remains available',async()=>{
  let cleaned=false;const engine=createEngineLifecycle({ccall:()=>-1,_PartyBoard_GameRequestStop(){}},
    {shutdown:async()=>{cleaned=true;}});
  await assert.rejects(engine.initialize(),/rejected/);assert.equal(engine.phase,'Failed');
  assert.throws(()=>engine.resume());await engine.dispose();assert.ok(cleaned);
});
test('stop failure can be retried without releasing a live engine',async()=>{
  const run=deferred();let calls=0,stops=0,cleaned=false;
  const engine=createEngineLifecycle({ccall:()=>++calls===1?0:run.promise,
    _PartyBoard_GameRequestStop(){if(++stops===1)throw Error('stop failed');run.resolve(0);}},
    {shutdown:async()=>{cleaned=true;}});
  await engine.initialize();const execution=engine.resume();assert.throws(()=>engine.dispose(),/stop failed/);
  assert.equal(engine.phase,'Running');assert.equal(cleaned,false);
  await engine.dispose();await execution;assert.equal(cleaned,true);
});

test('clock alignment only occurs once before execution and never during disposal',async()=>{
  let alignments=0;
  const engine=createEngineLifecycle({ccall:()=>0,_PartyBoard_GameRequestStop(){},
    _PartyBoard_GameAlignStartClock(){alignments++;return 0;}},{shutdown:async()=>{}});
  assert.throws(()=>engine.alignStartClock());await engine.initialize();engine.alignStartClock();
  assert.throws(()=>engine.alignStartClock());assert.equal(alignments,1);
  await engine.resume();assert.throws(()=>engine.alignStartClock());await engine.dispose();
  assert.throws(()=>engine.alignStartClock());
  const unaligned=createEngineLifecycle({ccall:()=>0,_PartyBoard_GameRequestStop(){},
    _PartyBoard_GameAlignStartClock(){alignments++;return 0;}},{shutdown:async()=>{}});
  await unaligned.initialize();await unaligned.resume();assert.throws(()=>unaligned.alignStartClock());
  assert.equal(alignments,1);await unaligned.dispose();
});
test('missing or rejected clock export prevents alignment without marking it successful',async()=>{
  for(const exported of [undefined,()=>-1]) {
    const engine=createEngineLifecycle({ccall:()=>0,_PartyBoard_GameRequestStop(){},
      _PartyBoard_GameAlignStartClock:exported},{shutdown:async()=>{}});
    await engine.initialize();assert.throws(()=>engine.alignStartClock());
    assert.equal(engine.phase,'Idle');await engine.dispose();
  }
});

test('prepared checkpoint reads actual uint32 value only while aligned and before execution',async()=>{
  for(const value of [0,0xffffffff]) {
    const engine=createEngineLifecycle({ccall:()=>0,_PartyBoard_GameRequestStop(){},
      _PartyBoard_GameAlignStartClock:()=>0,_PartyBoard_GamePreparedCheckpoint:()=>value},
      {shutdown:async()=>{}});
    assert.throws(()=>engine.preparedCheckpoint());await engine.initialize();
    assert.throws(()=>engine.preparedCheckpoint());engine.alignStartClock();
    assert.equal(engine.preparedCheckpoint(),value);await engine.resume();
    assert.throws(()=>engine.preparedCheckpoint());await engine.dispose();
  }
  for(const value of [-1,NaN,1.5,0x100000000]) {
    const engine=createEngineLifecycle({ccall:()=>0,_PartyBoard_GameRequestStop(){},
      _PartyBoard_GameAlignStartClock:()=>0,_PartyBoard_GamePreparedCheckpoint:()=>value},
      {shutdown:async()=>{}});
    await engine.initialize();engine.alignStartClock();assert.throws(()=>engine.preparedCheckpoint());await engine.dispose();
  }
});

test('paused state reads selected checkpoint and both RNG streams only while idle',async()=>{
  const values=[3,0xffffffff,0,42];
  const engine=createEngineLifecycle({ccall:()=>0,_PartyBoard_GameRequestStop(){},
    _PartyBoard_GamePausedState:field=>values[field]},{shutdown:async()=>{}});
  assert.throws(()=>engine.pausedState());await engine.initialize();
  assert.deepEqual(engine.pausedState(),{frame:3,checkpoint:0xffffffff,frand:0,rand8:42});
  values[2]=-1;assert.throws(()=>engine.pausedState());values[2]=0;
  await engine.dispose();assert.throws(()=>engine.pausedState());
});
