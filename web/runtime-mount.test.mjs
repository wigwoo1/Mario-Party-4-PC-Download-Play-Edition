import {test} from 'node:test';
import assert from 'node:assert/strict';
import {prepare} from './transfer.mjs';
import {prepareMountedRuntime} from './runtime-mount.mjs';

async function fixture() {
  const bytes = new Uint8Array([1,2,3]);
  const {manifest} = await prepare(new Blob([bytes]));
  return {id:manifest.id, store:{manifest:async()=>manifest,
    get:async()=>({bytes:bytes.buffer})}};
}
test('verification precedes initialization; disposal stops before unmounting',async()=>{
  const f=await fixture(), events=[], module={partyboardReleaseRuntimeArtifacts:()=>events.push('release')};
  const runtime=await prepareMountedRuntime(f.store,f.id,async options=>{
    assert.equal(options.noInitialRun,true); events.push('create'); return module;
  },{shutdown:async m=>{assert.equal(m,module);events.push('stop');},
    mount:(m,blob)=>{assert.equal(blob.size,3);events.push('mount');
      return {dispose:()=>events.push('unmount')};}});
  assert.equal(runtime.module,module);
  await Promise.all([runtime.dispose(),runtime.dispose()]);
  await runtime.dispose();
  assert.deepEqual(events,['create','mount','stop','release','unmount']);
  assert.equal(runtime.module,undefined);
});
test('cancel during initialization cleans up late runtime without mounting',async()=>{
  const f=await fixture(), abort=new AbortController(), events=[];
  await assert.rejects(prepareMountedRuntime(f.store,f.id,async()=>{
    abort.abort();return {};
  },{signal:abort.signal,shutdown:async()=>events.push('stop'),
    mount:()=>{throw Error('Must not mount');}}),{name:'AbortError'});
  assert.deepEqual(events,['stop']);
});

test('factory receives session cancellation and late initialization still cleans up',async()=>{
  const f=await fixture(), abort=new AbortController(),events=[];
  let began,finish;
  const started=new Promise(resolve=>{began=resolve;});
  const initialized=new Promise(resolve=>{finish=resolve;});
  const preparation=prepareMountedRuntime(f.store,f.id,options=>{
    assert.equal(options.signal,abort.signal);began(options.signal);return initialized;
  },{signal:abort.signal,shutdown:async()=>events.push('stop'),
    mount:()=>{throw Error('Must not mount');}});
  const suppliedSignal=await started;abort.abort();
  assert.equal(suppliedSignal.aborted,true);assert.deepEqual(events,[]);
  finish({});
  await assert.rejects(preparation,{name:'AbortError'});
  assert.deepEqual(events,['stop']);
});
test('damaged storage never initializes runtime',async()=>{
  const f=await fixture(); f.store.get=async()=>({bytes:new Uint8Array([3,2,1]).buffer});
  await assert.rejects(prepareMountedRuntime(f.store,f.id,()=>{throw Error('Must not initialize');},
    {shutdown:async()=>{}}),/integrity/);
});
test('shutdown failure retains mount and permits cleanup retry',async()=>{
  const f=await fixture();let attempts=0,unmounted=0,released=0;
  const runtime=await prepareMountedRuntime(f.store,f.id,async()=>({partyboardReleaseRuntimeArtifacts:()=>released++}),{
    shutdown:async()=>{if(++attempts===1)throw Error('Read still pending');},
    mount:()=>({dispose:()=>++unmounted}),
  });
  await assert.rejects(runtime.dispose(),/pending/);
  assert.equal(unmounted,0);assert.equal(released,0);assert.ok(runtime.module);
  await runtime.dispose();assert.equal(unmounted,1);assert.equal(released,1);
});

test('shutdown callback requesting disposal joins the same operation',async()=>{
  const f=await fixture();let runtime,joined,shutdowns=0,unmounts=0;
  runtime=await prepareMountedRuntime(f.store,f.id,async()=>({}),{
    shutdown:()=>{shutdowns++;joined=runtime.dispose();},
    mount:()=>({dispose:()=>unmounts++}),
  });
  await runtime.dispose();await joined;
  assert.equal(shutdowns,1);assert.equal(unmounts,1);assert.equal(runtime.module,undefined);
});
