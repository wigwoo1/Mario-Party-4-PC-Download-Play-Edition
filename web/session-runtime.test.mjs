import test from 'node:test';
import assert from 'node:assert/strict';
import {prepare} from './transfer.mjs';
import {prepareSessionRuntime,prepareFingerprintedSessionRuntime} from './session-runtime.mjs';
import {fingerprintRuntimeFiles} from './runtime-build.mjs';
import {StartBarrier} from './start-barrier.mjs';
import {prepareStartAcknowledgment,resumeCommittedClientRuntime} from './start-client.mjs';
import {bindClientStartPreparation} from './start-preparation-channel.mjs';

async function fixture(){const bytes=new Uint8Array([1,2,3]),{manifest}=await prepare(new Blob([bytes]));
  const events=[],module={};return {id:manifest.id,events,module,
    store:{manifest:async()=>manifest,get:async()=>({bytes:bytes.buffer})},
    create:async options=>{assert.equal(options.noInitialRun,true);events.push('create');return module;},
    options:{sessionSeed:7,mount:()=>{events.push('mount');return {dispose:()=>events.push('unmount')};},
      setup:async()=>events.push('setup'),shutdown:async()=>events.push('shutdown'),
      prepareEngine:async(m,options)=>{assert.equal(m,module);assert.equal(options.sessionSeed,7);events.push('initialize');
        return {sessionSeed:7,checkpoint:42,engine:{phase:'Idle'},dispose:async()=>{events.push('engineDispose');await options.shutdown(m);}};}}};}

test('aborted execution releases mounted session after shutdown and retains verified stored assets',async()=>{
  const f=await fixture();delete f.options.prepareEngine;
  Object.assign(f.module,{
    ccall:name=>{f.events.push(name);return name==='PartyBoard_GameResume'?new Promise(()=>{}):0;},
    _PartyBoard_GameRequestStop(){throw Error('Aborted execution must not be stopped');},
    _PartyBoard_GameConfigureSessionSeed:()=>0,_PartyBoard_GameAlignStartClock:()=>0,
    _PartyBoard_GamePreparedCheckpoint:()=>42,
  });
  const runtime=await prepareSessionRuntime(f.store,f.id,f.create,f.options);
  const execution=runtime.engine.resume();f.module.onAbort('native code called abort()');
  await assert.rejects(execution,/Engine aborted/);assert.equal(runtime.engine.phase,'Failed');
  await runtime.dispose();assert.equal(runtime.module,undefined);
  assert.deepEqual(f.events.slice(-2),['shutdown','unmount']);
  assert.equal((await f.store.manifest()).id,f.id);
  assert.deepEqual(new Uint8Array((await f.store.get()).bytes),new Uint8Array([1,2,3]));
});

test('fingerprinted session retains measured immutable build identity through cleanup',async()=>{
  const f=await fixture(),files=[{name:'contract.mjs',blob:new Blob(['wrapper'])},
    {name:'contract.wasm',blob:new Blob(['wasm'])}];
  const runtime=await prepareFingerprintedSessionRuntime(f.store,f.id,files,{
    ...f.options,runtimeOptions:{noInitialRun:false},
    runtimeBuild:{importWrapper:async()=>({default:f.create})},
  });
  assert.equal(runtime.buildId,(await fingerprintRuntimeFiles(files)).buildId);
  assert.throws(()=>{runtime.buildId='c'.repeat(64);},TypeError);
  assert.equal(runtime.module,f.module);await runtime.dispose();assert.equal(runtime.module,undefined);
  assert.deepEqual(f.events,['create','mount','setup','initialize','engineDispose','shutdown','unmount']);
});

test('pinned shared package rejects drift before assets, platform or engine preparation',async()=>{
  const f=await fixture(),files=[{name:'contract.mjs',blob:new Blob(['wrapper'])},
    {name:'contract.wasm',blob:new Blob(['wasm'])},{name:'native/dol.dll',blob:new Blob(['native'])}];
  const expectedBuildId=(await fingerprintRuntimeFiles(files)).buildId;
  f.store.manifest=()=>{throw Error('Assets must not be touched on package drift');};
  const changed=files.map(file=>file.name==='native/dol.dll'?{...file,blob:new Blob(['drift'])}:file);
  await assert.rejects(prepareFingerprintedSessionRuntime(f.store,f.id,changed,{
    ...f.options,runtimeBuild:{expectedBuildId,importWrapper:()=>{throw Error('Must not import');}},
  }),/differs from agreed identity/);
  assert.deepEqual(f.events,[]);
});

test('pinned shared package identity reaches prepared runtime without removing native entries',async()=>{
  const f=await fixture(),files=[{name:'contract.mjs',blob:new Blob(['wrapper'])},
    {name:'contract.wasm',blob:new Blob(['wasm'])},{name:'native/dol.dll',blob:new Blob(['native'])}];
  const expectedBuildId=(await fingerprintRuntimeFiles(files)).buildId;
  const runtime=await prepareFingerprintedSessionRuntime(f.store,f.id,files,{
    ...f.options,runtimeBuild:{expectedBuildId,importWrapper:async()=>({default:f.create})},
  });
  assert.equal(runtime.buildId,expectedBuildId);assert.equal(runtime.engine.phase,'Idle');
  await runtime.dispose();assert.equal(runtime.module,undefined);
});

test('verified mounted assets prepare idle seeded engine and dispose before unmount',async()=>{
  const f=await fixture(),runtime=await prepareSessionRuntime(f.store,f.id,f.create,f.options);
  assert.equal(runtime.engine.phase,'Idle');assert.equal(runtime.checkpoint,42);assert.equal(runtime.assetId,f.id);
  await Promise.all([runtime.dispose(),runtime.dispose()]);
  assert.deepEqual(f.events,['create','mount','setup','initialize','engineDispose','shutdown','unmount']);
});
test('platform setup failure releases mounted assets without starting an engine',async()=>{
  const f=await fixture();f.options.setup=async()=>{throw Error('Setup failed');};
  await assert.rejects(prepareSessionRuntime(f.store,f.id,f.create,f.options),/Setup failed/);
  assert.deepEqual(f.events,['create','mount','shutdown','unmount']);
});
test('cancel after engine preparation disposes engine before unmount',async()=>{
  const f=await fixture(),controller=new AbortController(),prepareEngine=f.options.prepareEngine;
  f.options.signal=controller.signal;f.options.prepareEngine=async(...args)=>{const result=await prepareEngine(...args);controller.abort();return result;};
  await assert.rejects(prepareSessionRuntime(f.store,f.id,f.create,f.options),{name:'AbortError'});
  assert.deepEqual(f.events,['create','mount','setup','initialize','engineDispose','shutdown','unmount']);
});

test('default engine lifecycle prepares checkpoint without running game loop',async()=>{
  const f=await fixture();delete f.options.prepareEngine;
  Object.assign(f.module,{
    ccall:async name=>{f.events.push(name);return 0;},
    _PartyBoard_GameRequestStop:()=>{},
    _PartyBoard_GameConfigureSessionSeed:seed=>{assert.equal(seed,7);return 0;},
    _PartyBoard_GameAlignStartClock:()=>0,_PartyBoard_GamePreparedCheckpoint:()=>42,
  });
  const runtime=await prepareSessionRuntime(f.store,f.id,f.create,f.options);
  assert.equal(runtime.engine.phase,'Idle');assert.equal(runtime.checkpoint,42);
  assert.ok(!f.events.includes('PartyBoard_GameResume'));await runtime.dispose();
  assert.deepEqual(f.events,['create','mount','setup','PartyBoard_GameInitialize','shutdown','unmount']);
});

test('engine initialization failure shuts platform down once before asset unmount',async()=>{
  const f=await fixture();delete f.options.prepareEngine;
  Object.assign(f.module,{ccall:async()=>{throw Error('Initialization failed');},
    _PartyBoard_GameRequestStop:()=>{},_PartyBoard_GameConfigureSessionSeed:()=>0});
  await assert.rejects(prepareSessionRuntime(f.store,f.id,f.create,f.options),/Initialization failed/);
  assert.deepEqual(f.events,['create','mount','setup','shutdown','unmount']);
});

test('invalid agreed seed rejects before module creation or storage verification',async()=>{
  const f=await fixture();f.store.manifest=()=>{throw Error('Storage must not be read');};
  for(const sessionSeed of [undefined,-1,0x100000000,1.5])
    await assert.rejects(prepareSessionRuntime(f.store,f.id,f.create,{...f.options,sessionSeed}),/Agreed seed/);
  assert.deepEqual(f.events,[]);
});

test('verified mounted engine acknowledges start and runs only after matching commit',async()=>{
  const f=await fixture();delete f.options.prepareEngine;
  Object.assign(f.module,{
    ccall:async name=>{f.events.push(name);return 0;},
    _PartyBoard_GameRequestStop:()=>{},_PartyBoard_GameConfigureSessionSeed:()=>0,
    _PartyBoard_GameAlignStartClock:()=>0,_PartyBoard_GamePreparedCheckpoint:()=>42,
  });
  const snapshot={revision:1,membership:1,total:3,assetId:f.id,players:
    ['host','guest'].map((peer,slot)=>({peer,slot,bytes:3,verified:true,shaders:true,ready:true}))};
  const barrier=new StartBarrier(snapshot,{session:new Uint8Array(16).fill(1),seed:7,
    buildId:'b'.repeat(64),checkpoint:42});
  const options={peer:'guest',buildId:'b'.repeat(64),snapshot:()=>snapshot};
  const prepared=await prepareStartAcknowledgment(barrier.proposal,{...options,
    prepare:proposal=>prepareSessionRuntime(f.store,proposal.assetId,f.create,{...f.options,sessionSeed:proposal.seed})});
  assert.equal(prepared.runtime.engine.phase,'Idle');assert.equal(prepared.runtime.assetId,f.id);
  assert.ok(!f.events.includes('PartyBoard_GameResume'));
  assert.throws(()=>resumeCommittedClientRuntime({...barrier.proposal,type:'session-start-commit',seed:8},prepared,options),/differs/);
  assert.equal(prepared.runtime.engine.phase,'Idle');
  await resumeCommittedClientRuntime({...barrier.proposal,type:'session-start-commit'},prepared,options);
  assert.equal(f.events.filter(name=>name==='PartyBoard_GameResume').length,1);
  await prepared.runtime.dispose();assert.deepEqual(f.events.slice(-2),['shutdown','unmount']);
});

test('start disconnect during engine initialization waits before platform shutdown and unmount',async()=>{
  const f=await fixture();delete f.options.prepareEngine;
  let initialized,release;
  const entered=new Promise(resolve=>initialized=resolve),gate=new Promise(resolve=>release=resolve);
  Object.assign(f.module,{
    ccall:async name=>{f.events.push(name);initialized();await gate;f.events.push('initializeSettled');return 0;},
    _PartyBoard_GameRequestStop:()=>{},_PartyBoard_GameConfigureSessionSeed:()=>0,
    _PartyBoard_GameAlignStartClock:()=>{throw Error('Cancelled engine must not align');},
  });
  const snapshot={revision:1,membership:1,total:3,assetId:f.id,players:
    ['host','guest'].map((peer,slot)=>({peer,slot,bytes:3,verified:true,shaders:true,ready:true}))};
  const barrier=new StartBarrier(snapshot,{session:new Uint8Array(16).fill(1),seed:7,buildId:'b'.repeat(64),checkpoint:42});
  class Channel extends EventTarget {
    readyState='open';ordered=true;sent=[];
    send(data){this.sent.push(data);}
    close(){this.readyState='closed';this.dispatchEvent(new Event('close'));}
  }
  const channel=new Channel(),errors=[];
  const owner=bindClientStartPreparation(channel,{peer:'guest',buildId:'b'.repeat(64),snapshot:()=>snapshot,
    prepare:(proposal,{signal})=>prepareSessionRuntime(f.store,proposal.assetId,f.create,{...f.options,signal})},
    {error:cause=>errors.push(cause.message)});
  channel.dispatchEvent(new MessageEvent('message',{data:JSON.stringify(barrier.proposal)}));await entered;
  channel.close();const closing=owner.close();
  await new Promise(resolve=>setImmediate(resolve));
  assert.deepEqual(f.events,['create','mount','setup','PartyBoard_GameInitialize']);
  assert.equal(channel.sent.length,0);release();await closing;
  assert.deepEqual(f.events,['create','mount','setup','PartyBoard_GameInitialize','initializeSettled','shutdown','unmount']);
  assert.equal(channel.sent.length,0);assert.equal(errors.length,1);assert.equal(owner.committed,false);
});

test('session awaits renderer preparation and releases resources if renderer fails or changes simulation',async()=>{
  for(const failure of ['renderer','state','abort']) {
    const f=await fixture(),abort=new AbortController();f.options.signal=abort.signal;
    const prepare=f.options.prepareEngine;
    f.options.prepareEngine=async(...args)=>{const result=await prepare(...args);result.engine.preparedCheckpoint=()=>42;return result;};
    f.options.prepareRenderer=async(module,{engine,signal})=>{
      assert.equal(module,f.module);assert.equal(signal,abort.signal);f.events.push('renderer');
      if(failure==='renderer')throw Error('Renderer failed');
      if(failure==='state')engine.phase='Running';else abort.abort();
    };
    await assert.rejects(prepareSessionRuntime(f.store,f.id,f.create,f.options));
    assert.deepEqual(f.events,['create','mount','setup','initialize','renderer','engineDispose','shutdown','unmount']);
  }
});
test('renderer receipt is retained only after idle checkpoint validation',async()=>{
  const f=await fixture(),prepare=f.options.prepareEngine;
  f.options.prepareEngine=async(...args)=>{const result=await prepare(...args);result.engine.preparedCheckpoint=()=>42;return result;};
  f.options.prepareRenderer=async()=>({scope:'discovered-pipelines-only',created:2});
  const runtime=await prepareSessionRuntime(f.store,f.id,f.create,f.options);
  assert.deepEqual(runtime.rendererPreparation,{scope:'discovered-pipelines-only',created:2});
  assert.equal(runtime.engine.phase,'Idle');await runtime.dispose();
});

test('session owns input hook and cancels pending exchange before shutdown and unmount',async()=>{
  const f=await fixture(),prepare=f.options.prepareEngine;
  let entered,exchangeSignal;
  const started=new Promise(resolve=>entered=resolve);
  f.options.prepareEngine=async(...args)=>{const result=await prepare(...args);result.engine.preparedCheckpoint=()=>42;return result;};
  f.module._PartyBoardBrowserRequireOnlineInput=required=>{assert.equal(required,1);f.events.push('inputInstalled');};
  f.options.inputExchange=(input,{signal})=>{exchangeSignal=signal;entered();return new Promise(()=>{});};
  const shutdown=f.options.shutdown;
  f.options.shutdown=async module=>{assert.equal(exchangeSignal.aborted,true);await shutdown(module);};
  const runtime=await prepareSessionRuntime(f.store,f.id,f.create,f.options);
  assert.equal(runtime.inputExchangeInstalled,true);
  const pending=f.module.partyBoardInputExchange(new Uint8Array(44),{checkpoint:42});await started;
  await runtime.dispose();assert.equal(await pending,null);
  assert.equal(f.module.partyBoardInputExchange,undefined);assert.equal(f.module.partyBoardCancelInputExchange,undefined);
  assert.deepEqual(f.events,['create','mount','setup','initialize','inputInstalled','engineDispose','shutdown','unmount']);
});

test('input installation failure releases prepared session without advancing simulation',async()=>{
  const f=await fixture(),prepare=f.options.prepareEngine;
  f.options.prepareEngine=async(...args)=>{const result=await prepare(...args);result.engine.preparedCheckpoint=()=>42;return result;};
  f.options.inputExchange=async input=>input;
  f.module._PartyBoardBrowserRequireOnlineInput=()=>{throw Error('Input install failed');};
  await assert.rejects(prepareSessionRuntime(f.store,f.id,f.create,f.options),/Input install failed/);
  assert.equal(f.module.partyBoardInputExchange,undefined);
  assert.deepEqual(f.events,['create','mount','setup','initialize','engineDispose','shutdown','unmount']);
});
test('verified pipeline cache mounts before platform setup and is removed after shutdown',async()=>{
 const f=await fixture(),bytes=new Uint8Array(100);bytes.set(new TextEncoder().encode('SQLite format 3\0'));
 const sha256=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)),b=>b.toString(16).padStart(2,'0')).join('');
 f.module.FS={writeFile:()=>f.events.push('seedWrite'),unlink:()=>f.events.push('seedRemove')};
 const runtime=await prepareSessionRuntime(f.store,f.id,f.create,{...f.options,pipelineSeed:{blob:new Blob([bytes]),sha256}});
 assert.ok(f.events.indexOf('seedWrite')<f.events.indexOf('setup'));assert.equal(runtime.pipelineSeed.scope,'seeded-pipelines-only');
 await runtime.dispose();assert.ok(f.events.indexOf('shutdown')<f.events.indexOf('seedRemove'));assert.ok(f.events.indexOf('seedRemove')<f.events.indexOf('unmount'));
});
