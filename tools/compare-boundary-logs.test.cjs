const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),os=require('node:os'),path=require('node:path');
const {compareBoundaryLogs}=require('./compare-boundary-logs.cjs');
test('boundary comparison rejects altered RNG and incomplete engine execution',()=>{
 const dir=fs.mkdtempSync(path.join(os.tmpdir(),'partyboard-boundary-'));
 try {
  const a=path.join(dir,'a.log'),b=path.join(dir,'b.log');
  const records=[0,1].map(poll=>({poll,checkpoint:42+poll,frand:0xffffffff,rand8:1}));
  const log=()=>records.map(record=>'Actual PAD boundary: '+JSON.stringify(record)).join('\n')+'\nPASS: JavaScript awaited real engine';
  fs.writeFileSync(a,log());fs.writeFileSync(b,log());assert.equal(compareBoundaryLogs(a,b).boundaries,2);
  records[1].rand8=2;fs.writeFileSync(b,log());assert.throws(()=>compareBoundaryLogs(a,b),/boundary states differ/);
  fs.writeFileSync(b,log().replace('PASS:','FAIL:'));assert.throws(()=>compareBoundaryLogs(a,b),/Engine run must pass/);
 } finally {fs.rmSync(dir,{recursive:true,force:true});}
});
