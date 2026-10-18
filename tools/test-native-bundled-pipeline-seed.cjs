const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const {DatabaseSync}=require('node:sqlite');
const [codec,report]=process.argv.slice(2);
if(!codec||!report)throw Error('Usage: node test-native-bundled-pipeline-seed.cjs NATIVE_CODEC REPORT');
const source=path.resolve(__dirname,'../assets/shaders'),work=fs.realpathSync('work');
const fixture=fs.mkdtempSync(path.join(work,'bundled-seed-test-'));
const metadata=JSON.parse(fs.readFileSync(path.join(source,'initial_pipeline_cache.json'))),checks=[];
try{
 const seed=fs.readFileSync(path.join(source,'initial_pipeline_cache.db'));
 assert.equal(seed.length,metadata.bytes);assert.equal(crypto.createHash('sha256').update(seed).digest('hex'),metadata.sha256);
 const db=new DatabaseSync(path.join(source,'initial_pipeline_cache.db'),{readOnly:true});
 assert.equal(db.prepare('PRAGMA quick_check').get().quick_check,'ok');
 assert.equal(db.prepare('SELECT count(*) AS n FROM pipeline_cache').get().n,metadata.pipelineCount);
 assert.equal(db.prepare('SELECT value FROM aurora_schema').get().value,1);db.close();checks.push('bundled SQLite rows, schema, hash and size');
 const manifest=path.join(fixture,'initial_pipeline_cache.json'),database=path.join(fixture,'initial_pipeline_cache.db');
 fs.writeFileSync(database,seed);
 const run=()=>spawnSync(path.resolve(codec),[fixture],{encoding:'utf8'});
 fs.writeFileSync(manifest,JSON.stringify(metadata));let result=run();assert.equal(result.status,0,result.stderr);
 assert.deepEqual(JSON.parse(result.stdout),{sha256:metadata.sha256,definitions:metadata.pipelineCount,bytes:metadata.bytes});checks.push('native default-bundle reader');
 for(const [name,changed] of [
  ['version',{version:2}],['coverage scope',{scope:'full-coverage'}],['empty row count',{pipelineCount:0}],
  ['size mismatch',{bytes:metadata.bytes+1}],['hash mismatch',{sha256:'0'.repeat(64)}],
 ]){fs.writeFileSync(manifest,JSON.stringify({...metadata,...changed}));result=run();assert.notEqual(result.status,0,name);checks.push('reject '+name);}
 fs.writeFileSync(manifest,' '.repeat(4097));result=run();assert.notEqual(result.status,0);assert.match(result.stderr,/Bounded/);checks.push('reject oversized metadata');
 fs.writeFileSync(manifest,JSON.stringify(metadata));fs.writeFileSync(database,Buffer.alloc(100));result=run();assert.notEqual(result.status,0);assert.match(result.stderr,/SQLite/);checks.push('reject invalid SQLite header');
 fs.unlinkSync(database);result=run();assert.notEqual(result.status,0);assert.match(result.stderr,/missing/);checks.push('reject missing required cache');
 fs.writeFileSync(report,JSON.stringify({verified:true,checks,sha256:metadata.sha256,definitions:metadata.pipelineCount,fullShaderCoverageVerified:false},null,2)+'\n');
 console.log('PASS '+checks.length+' bundled shader validation checks');
}finally{
 const resolved=fs.realpathSync(fixture);
 if(path.dirname(resolved)!==work||!path.basename(resolved).startsWith('bundled-seed-test-'))throw Error('Unexpected fixture cleanup path');
 fs.rmSync(resolved,{recursive:true});
}
