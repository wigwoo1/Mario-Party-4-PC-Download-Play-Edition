'use strict';
const {test}=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),os=require('node:os'),path=require('node:path');
const {retainBrowserInspection}=require('./retain-browser-inspection.cjs');
test('optional endpoint ownership and cleanup',()=>{
 retainBrowserInspection(undefined,'unused')();
 const directory=fs.mkdtempSync(path.join(os.tmpdir(),'partyboard-inspection-')),file=path.join(directory,'endpoint.json');
 try{
  assert.throws(()=>retainBrowserInspection(file,'ws://example.com/token'),/Local/);
  const close=retainBrowserInspection(file,'ws://127.0.0.1:1234/token');
  assert.equal(JSON.parse(fs.readFileSync(file)).endpoint,'ws://127.0.0.1:1234/token');
  assert.throws(()=>retainBrowserInspection(file,'ws://127.0.0.1:1234/other'),{code:'EEXIST'});
  close();assert.equal(fs.existsSync(file),false);close();
  const replaced=retainBrowserInspection(file,'ws://localhost:1234/token');
  fs.writeFileSync(file,'new owner');replaced();assert.equal(fs.readFileSync(file,'utf8'),'new owner');
 }finally{if(fs.existsSync(file))fs.unlinkSync(file);fs.rmdirSync(directory);}
});
