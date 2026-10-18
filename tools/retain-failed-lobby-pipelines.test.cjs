const {test}=require('node:test');const assert=require('node:assert/strict');const fs=require('node:fs'),os=require('node:os'),path=require('node:path'),crypto=require('node:crypto');
const {retainFailedLobbyPipelines}=require('./retain-failed-lobby-pipelines.cjs');
const seed=fs.readFileSync(path.resolve(__dirname,'../../../outputs/validation/browser-menu-board-tree-stomp-merged-pipeline-cache.db'));
test('retains actual closed shader database with partial scope; rejects altered bytes and preserves peer failure',async()=>{
 const directory=fs.mkdtempSync(path.join(os.tmpdir(),'partyboard-cache-'));
 try{
  const captured={scope:'observed-scenes-only',sha256:crypto.createHash('sha256').update(seed).digest('hex'),bytes:Array.from(seed)};
  const results=await retainFailedLobbyPipelines([{evaluate:async()=>captured},{evaluate:async()=>({...captured,sha256:'0'.repeat(64)})},{evaluate:async()=>{throw Error('engine unavailable');}}],directory);
  assert.equal(results[0].result,'captured');assert.equal(results[0].fullCoverage,false);assert.equal(results[0].matchCompleted,false);
  assert.deepEqual(fs.readFileSync(path.join(directory,results[0].filename)),seed);
  assert.equal(results[1].result,'unavailable');assert.match(results[1].error,/integrity/);assert.equal(results[2].result,'unavailable');
  assert.equal(fs.existsSync(path.join(directory,'browser-failed-closed-pipelines-peer1.db')),false);
 }finally{fs.rmSync(directory,{recursive:true,force:true});}
});
