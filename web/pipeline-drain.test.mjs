import {test} from 'node:test';
import assert from 'node:assert/strict';
import {drainPreparedPipelines,drainIdleEnginePipelines} from './pipeline-drain.mjs';
test('drains discovered work through renderer steps and reports limited coverage',async()=>{
  let queued=3,steps=0;const progress=[];
  const result=await drainPreparedPipelines({read:()=>({queued,created:3-queued}),advance:async()=>{steps++;queued--;},progress:s=>progress.push(s)});
  assert.equal(steps,3);assert.deepEqual(result,{steps:3,queued:0,created:3,scope:'discovered-pipelines-only'});
  assert.equal(progress.length,4);
});
test('stalled pipeline work fails at exact step budget',async()=>{
  let steps=0;
  await assert.rejects(drainPreparedPipelines({read:()=>({queued:1,created:0}),advance:()=>steps++,maxSteps:2}),/limit reached/);
  assert.equal(steps,2);
});
test('abort during renderer step prevents readiness receipt and further reads',async()=>{
  const abort=new AbortController();let reads=0;
  await assert.rejects(drainPreparedPipelines({signal:abort.signal,read:()=>{reads++;return {queued:1,created:0};},advance:()=>abort.abort()}),{name:'AbortError'});
  assert.equal(reads,1);
});
test('invalid pipeline statistics and renderer failure propagate',async()=>{
  for(const queued of [-1,NaN,1.5,undefined])
    await assert.rejects(drainPreparedPipelines({read:()=>({queued,created:0}),advance:()=>{throw Error('Must not step');}}),/Invalid renderer/);
  await assert.rejects(drainPreparedPipelines({read:()=>({queued:1,created:0}),advance:()=>{throw Error('Renderer lost');}}),/Renderer lost/);
});
test('progress mutation cannot falsely mark queued work complete',async()=>{
  let steps=0;
  await assert.rejects(drainPreparedPipelines({read:()=>({queued:1,created:0}),advance:()=>steps++,maxSteps:1,progress:s=>{s.queued=0;}}),/limit reached/);
  assert.equal(steps,1);
});

test('idle engine drain rejects simulation changes during renderer steps',async()=>{
  for(const field of ['phase','checkpoint']) {
    let checkpoint=42;const engine={phase:'Idle',preparedCheckpoint:()=>checkpoint};
    await assert.rejects(drainIdleEnginePipelines(engine,{read:()=>({queued:1,created:0}),advance:()=>{
      if(field==='phase')engine.phase='Running';else checkpoint=43;
    }}),/changed the prepared simulation/);
  }
});
test('idle engine drain checks asynchronous statistics and final progress callbacks',async()=>{
  for(const stage of ['read','progress']) {
    let checkpoint=42;const engine={phase:'Idle',preparedCheckpoint:()=>checkpoint};
    await assert.rejects(drainIdleEnginePipelines(engine,{read:async()=>{
      if(stage==='read')checkpoint=43;return {queued:0,created:2};
    },advance:()=>{},progress:()=>{if(stage==='progress')checkpoint=43;}}),/changed the prepared simulation/);
  }
});
