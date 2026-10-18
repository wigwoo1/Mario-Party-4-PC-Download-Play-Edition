import test from 'node:test';import assert from 'node:assert/strict';import {knownShaderReadiness} from './shader-readiness.mjs';
const fixture=()=>({engine:{phase:'Idle',preparedCheckpoint:()=>42},checkpoint:42,pipelineSeed:{scope:'seeded-pipelines-only',sha256:'a'.repeat(64),size:925696},rendererPreparation:{checkpoint:42,scope:'discovered-pipelines-only',created:221,queued:0},module:{_PartyBoard_GameFrameCount:()=>0,_PartyBoardBrowserPipelineCount:kind=>kind===0?0:221}});
test('known cache readiness retains explicit partial coverage',()=>{const result=knownShaderReadiness(fixture());assert.equal(result.created,221);assert.equal(result.fullCoverage,false);assert.equal(result.scope,'known-pipelines-only');});
test('empty, unfinished, stale or advanced runtime cannot become shader ready',()=>{
 for(const change of [r=>r.pipelineSeed=undefined,r=>r.rendererPreparation.created=0,r=>r.rendererPreparation.queued=1,r=>r.rendererPreparation.checkpoint=43,r=>r.engine.phase='Running',r=>r.module._PartyBoard_GameFrameCount=()=>1,r=>r.module._PartyBoardBrowserPipelineCount=()=>1]){
  const runtime=fixture();change(runtime);assert.throws(()=>knownShaderReadiness(runtime));
 }
});
