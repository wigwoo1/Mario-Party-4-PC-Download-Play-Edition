import test from 'node:test';import assert from 'node:assert/strict';
import {preparedRuntimeReport,receivePreparedRuntimeReport,comparePreparedRuntimeReports} from './prepared-runtime-report.mjs';
const configuration={type:'session-configuration',version:1,profile:'unlocked-no-save-v1',assetId:'a'.repeat(64),session:'1'.repeat(32),seed:0};
const runtime={assetId:configuration.assetId,buildId:'b'.repeat(64),checkpoint:42,engine:{phase:'Idle',preparedCheckpoint:()=>42},module:{_PartyBoard_GameFrameCount:()=>0},rendererPreparation:{scope:'discovered-pipelines-only'}};
test('prepared receipt must match the configured measured package pin',()=>{
 const pinned={...configuration,buildId:runtime.buildId};
 assert.equal(preparedRuntimeReport(runtime,pinned).buildId,pinned.buildId);
 assert.throws(()=>preparedRuntimeReport({...runtime,buildId:'c'.repeat(64)},pinned),/Invalid/);
});
test('actual idle runtime report is immutable and does not imply shader readiness',()=>{
 const report=preparedRuntimeReport(runtime,configuration);
 assert.equal(receivePreparedRuntimeReport(JSON.stringify(report),configuration,report),report);
 assert.equal(report.ready,undefined);assert.equal(comparePreparedRuntimeReports(report,{...report,buildId:'c'.repeat(64)}),false);
 assert.throws(()=>receivePreparedRuntimeReport(JSON.stringify({...report,seed:1}),configuration),/Invalid/);
 assert.throws(()=>receivePreparedRuntimeReport(JSON.stringify({...report,checkpoint:43}),configuration,report),/changed/);
 assert.throws(()=>preparedRuntimeReport({...runtime,module:{_PartyBoard_GameFrameCount:()=>1}},configuration),/frame-zero/);
});
test('different recorded pipeline seeds reject preparation agreement and report replacement',()=>{
 const seeded=preparedRuntimeReport({...runtime,pipelineSeed:{sha256:'c'.repeat(64)}},configuration);
 const unseeded=preparedRuntimeReport(runtime,configuration);
 assert.equal(seeded.pipelineSeed,'c'.repeat(64));assert.equal(comparePreparedRuntimeReports(seeded,unseeded),false);
 assert.equal(comparePreparedRuntimeReports(seeded,{...seeded,pipelineSeed:'d'.repeat(64)}),false);
 assert.throws(()=>receivePreparedRuntimeReport(JSON.stringify({...seeded,pipelineSeed:'d'.repeat(64)}),configuration,seeded),/changed/);
 assert.throws(()=>receivePreparedRuntimeReport(JSON.stringify({...seeded,pipelineSeed:'invalid'}),configuration),/Invalid/);
});
