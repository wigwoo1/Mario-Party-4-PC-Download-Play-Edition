import test from 'node:test';
import assert from 'node:assert/strict';
import {browserPlatformCallbacks} from './browser-platform.mjs';
const fixture=()=>{
  const events=[],engine={phase:'Idle',preparedCheckpoint:()=>42};
  const module={ccall:async name=>{events.push(name);return 0;},
    _PartyBoardBrowserPreparePlatform(){},_PartyBoardBrowserAdvancePreparedRenderer(){},_PartyBoardBrowserShutdownPlatform(){},
    _PartyBoardBrowserPipelineCount:()=>0};
  return {module,engine,events};
};
test('speaker destination is selected only after engine initialization while Idle',async()=>{
  const f=fixture(),destination={channelCount:2,maxChannelCount:6};
  f.module._PartyBoardBrowserSetAudioChannels=()=>{};
  const api=browserPlatformCallbacks({audioChannels:6});
  await api.setup(f.module);assert.equal(destination.channelCount,2);
  f.module.SDL3={audioContext:{destination}};
  await api.prepareRenderer(f.module,{engine:f.engine});assert.equal(destination.channelCount,6);
});

test('speaker preference is applied after platform setup, before engine initialization',async()=>{
  for(const audioChannels of [2,6]){
    const f=fixture(),calls=[],api=browserPlatformCallbacks({audioChannels});
    f.module._PartyBoardBrowserSetAudioChannels=()=>{};
    f.module.ccall=async(name,result,types,args)=>{calls.push({name,types,args});return 0;};
    await api.setup(f.module);
    assert.equal(calls[0].name,'PartyBoardBrowserPreparePlatform');
    assert.deepEqual(calls[1],{name:'PartyBoardBrowserSetAudioChannels',types:['number'],args:[audioChannels]});
  }
});

test('unsupported or rejected speaker preference fails preparation and retains cleanup',async()=>{
  for(const audioChannels of [0,1,4,8,'6',null])assert.throws(()=>browserPlatformCallbacks({audioChannels}),/speaker layout/);
  const f=fixture(),api=browserPlatformCallbacks({audioChannels:2});
  await assert.rejects(api.setup(f.module),/does not support speaker/);
  await api.shutdown(f.module);
  assert.equal(f.events.at(-1),'PartyBoardBrowserShutdownPlatform');
  const g=fixture(),other=browserPlatformCallbacks({audioChannels:6});
  g.module._PartyBoardBrowserSetAudioChannels=()=>{};
  g.module.ccall=async name=>{g.events.push(name);return name==='PartyBoardBrowserSetAudioChannels'?-1:0;};
  await assert.rejects(other.setup(g.module),/speaker layout rejected/);
  await other.shutdown(g.module);
  assert.equal(g.events.at(-1),'PartyBoardBrowserShutdownPlatform');
});
test('selected resolution and aspect are retained before platform/GPU startup',async()=>{
  for(const width of [640,960,1280,1920,2560])for(const aspect of [[4,3],[16,9],[16,10],[21,9]]){
    const [ratioWidth,ratioHeight]=aspect,height=Math.round(width*ratioHeight/ratioWidth);
    const f=fixture(),calls=[],resolution={width,height},api=browserPlatformCallbacks({resolution});
    resolution.width=1;f.module._PartyBoardBrowserSetInitialResolution=()=>{};
    f.module.ccall=async(name,result,types,args)=>{calls.push({name,types,args});return 0;};
    await api.setup(f.module);
    assert.deepEqual(calls[0],{name:'PartyBoardBrowserSetInitialResolution',types:['number','number'],args:[width,height]});
    assert.equal(calls[1].name,'PartyBoardBrowserPreparePlatform');
  }
});
test('invalid, unsupported or rejected resolution prevents platform startup',async()=>{
  assert.throws(()=>browserPlatformCallbacks({resolution:{width:1920,height:1000}}),/resolution/);
  assert.throws(()=>browserPlatformCallbacks({resolution:{width:1280,height:400}}),/resolution/);
  const f=fixture(),api=browserPlatformCallbacks({resolution:{width:640,height:480}});
  await assert.rejects(api.setup(f.module),/does not support/);assert.deepEqual(f.events,[]);
  await api.shutdown(f.module);assert.deepEqual(f.events,[]);
  f.module._PartyBoardBrowserSetInitialResolution=()=>{};f.module.ccall=async()=>-2;
  await assert.rejects(api.setup(f.module),/rejected/);
});
test('failed platform allocation retains shutdown and successful cleanup is idempotent',async()=>{
  const f=fixture(),api=browserPlatformCallbacks();
  f.module.ccall=async name=>{f.events.push(name);return name==='PartyBoardBrowserPreparePlatform'?-2:0;};
  await assert.rejects(api.setup(f.module),/operation failed/);
  await api.shutdown(f.module);await api.shutdown(f.module);
  assert.deepEqual(f.events,['PartyBoardBrowserPreparePlatform','PartyBoardBrowserShutdownPlatform']);
});
test('platform callbacks prepare/drain/shutdown using reusable exports without authorizing frames',async()=>{
  const f=fixture(),api=browserPlatformCallbacks();await api.setup(f.module);
  const receipt=await api.prepareRenderer(f.module,{engine:f.engine});await api.shutdown(f.module);
  assert.equal(receipt.checkpoint,42);assert.equal(receipt.scope,'discovered-pipelines-only');assert.equal(f.engine.phase,'Idle');
  assert.deepEqual(f.events,['PartyBoardBrowserPreparePlatform','PartyBoardBrowserAdvancePreparedRenderer','PartyBoardBrowserShutdownPlatform']);
});
test('renderer bridge rejects nonidle engine before platform call and changed checkpoint after initial renderer work',async()=>{
  const f=fixture(),api=browserPlatformCallbacks();f.engine.phase='Running';
  await assert.rejects(api.prepareRenderer(f.module,{engine:f.engine}),/Idle/);assert.deepEqual(f.events,[]);
  f.engine.phase='Idle';f.module.ccall=async()=>{f.engine.preparedCheckpoint=()=>43;return 0;};
  await assert.rejects(api.prepareRenderer(f.module,{engine:f.engine}),/changed prepared/);
});
test('setup cancellation prevents invocation; failed and missing exports remain errors',async()=>{
  const f=fixture(),api=browserPlatformCallbacks(),abort=new AbortController();abort.abort();
  await assert.rejects(api.setup(f.module,{signal:abort.signal}),{name:'AbortError'});assert.deepEqual(f.events,[]);
  f.module.ccall=async()=>-3;await assert.rejects(api.setup(f.module),/operation failed.*-3/);
  delete f.module._PartyBoardBrowserPreparePlatform;await assert.rejects(api.setup(f.module),/export missing/);
  assert.throws(()=>browserPlatformCallbacks({maxSteps:0}),/bound/);
});
