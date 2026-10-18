'use strict';
const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),os=require('node:os'),path=require('node:path'),crypto=require('node:crypto');
const {stageLocalPackage,fingerprint}=require('./stage-local-package.cjs');
function fixture(t){
 const root=fs.mkdtempSync(path.join(os.tmpdir(),'partyboard-package-'));
 // Only remove the exact directory created by this fixture, using one API.
 t.after(()=>fs.rmSync(root,{recursive:true,force:true}));
 const runtime=path.join(root,'installed');fs.mkdirSync(path.join(runtime,'browser-runtime'),{recursive:true});
 const entries=[['native/partyboard.exe','partyboard.exe'],['native/dol.dll','dol.dll'],['contract.mjs','browser-runtime/contract.mjs'],['contract.wasm','browser-runtime/contract.wasm']];
 for(const [name,file] of entries)fs.writeFileSync(path.join(runtime,file),name);
 const descriptor={configuration:{profile:'unlocked-no-save-v1',runtimeArtifacts:entries.filter(([name])=>name.startsWith('native/')).map(([name])=>name)},files:entries.map(([name,file])=>({name,path:file}))};
 descriptor.configuration.buildId=fingerprint(runtime,descriptor);
 fs.writeFileSync(path.join(runtime,'native-session-package.json'),JSON.stringify(descriptor));
 const seed=Buffer.from('fixture shader definitions');fs.writeFileSync(path.join(runtime,'initial_pipeline_cache.db'),seed);
 fs.writeFileSync(path.join(runtime,'initial_pipeline_cache.json'),JSON.stringify({version:1,scope:'discovered-pipelines-only',pipelineCount:1,bytes:seed.length,sha256:crypto.createHash('sha256').update(seed).digest('hex')}));
 for(const privateFile of ['game.ciso','game-disc-path.txt','personal-profile.json','old.pdb'])fs.writeFileSync(path.join(runtime,privateFile),'PRIVATE_SENTINEL');
 return {runtime,target:path.join(root,'candidate'),root,descriptor};
}
test('fresh candidate preserves measured bytes and relocates without private files or build leftovers',t=>{
 const f=fixture(t),report=stageLocalPackage(f.runtime,f.target);
 assert.equal(report.buildId,f.descriptor.configuration.buildId);assert.equal(report.artifacts,4);
 assert.equal(report.publicReleaseReady,false);assert.equal(report.cleanMachineVerified,false);
 assert.deepEqual(fs.readFileSync(path.join(f.target,'licenses/miniupnpc-LICENSE')),fs.readFileSync(path.join(__dirname,'../licenses/miniupnpc-LICENSE')));
 for(const dependency of ['aurora','libco','musyx'])
  assert.deepEqual(fs.readFileSync(path.join(f.target,'licenses',dependency+'-LICENSE')),fs.readFileSync(path.join(__dirname,'../extern',dependency,'LICENSE')));
 const notices=JSON.parse(fs.readFileSync(path.join(__dirname,'../licenses/dependency-notices.json')));
 for(const notice of notices.notices){
  const bytes=fs.readFileSync(path.join(f.target,'licenses',notice.file));
  assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),notice.sha256);
 }
 const moved=path.join(f.root,'relocated folder');fs.cpSync(f.target,moved,{recursive:true,errorOnExist:true,force:false});
 const d=JSON.parse(fs.readFileSync(path.join(moved,'runtime/native-session-package.json')));
 assert.equal(fingerprint(path.join(moved,'runtime'),d),report.buildId);
 for(const file of report.files){
  const bytes=fs.readFileSync(path.join(moved,file.path));assert.equal(bytes.length,file.bytes);
  assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),file.sha256);
  assert.doesNotMatch(file.path,/\.test\.|\.pdb$|\.ciso$|game-disc-path|personal-profile/);
  if(!/\.wasm$|\.db$|\.opus$|\.svg$/.test(file.path))assert.ok(!bytes.includes(Buffer.from(f.runtime)));
 }
 assert.equal(fs.readFileSync(path.join(moved,'native-runtime-path.txt'),'utf8').trim(),'runtime');
 assert.equal(fs.existsSync(path.join(moved,'PACKAGE-INCOMPLETE.txt')),false);
 assert.equal(fs.existsSync(path.join(moved,'partyboard/web/serve.cjs')),true);
 assert.equal(fs.existsSync(path.join(moved,'partyboard/web/audio/wifi-menu.opus')),true);
});
test('existing output is rejected without changing it',t=>{
 const f=fixture(t);fs.mkdirSync(f.target);fs.writeFileSync(path.join(f.target,'keep'),'preserve');
 assert.throws(()=>stageLocalPackage(f.runtime,f.target),/already exists/);
 assert.deepEqual(fs.readdirSync(f.target),['keep']);
});
test('changed compiled bytes are rejected before any destination is created',t=>{
 const f=fixture(t);fs.appendFileSync(path.join(f.runtime,'dol.dll'),'changed');
 assert.throws(()=>stageLocalPackage(f.runtime,f.target),/fingerprint differs/);
 assert.equal(fs.existsSync(f.target),false);
});
test('corrupt shader definitions are rejected before staging',t=>{
 const f=fixture(t);fs.appendFileSync(path.join(f.runtime,'initial_pipeline_cache.db'),'changed');
 assert.throws(()=>stageLocalPackage(f.runtime,f.target),/shader integrity/);
 assert.equal(fs.existsSync(f.target),false);
});
test('descriptor paths cannot escape the installed runtime',t=>{
 const f=fixture(t);f.descriptor.files[0].path='../outside.exe';fs.writeFileSync(path.join(f.root,'outside.exe'),'private');
 fs.writeFileSync(path.join(f.runtime,'native-session-package.json'),JSON.stringify(f.descriptor));
 assert.throws(()=>stageLocalPackage(f.runtime,f.target),/bundle location/);
 assert.equal(fs.existsSync(f.target),false);
});

test('changed, missing, unsafe and excessive declared notices fail before candidate creation',t=>{
 for(const kind of ['changed','missing','unsafe','excessive']){
  const f=fixture(t),project=path.join(f.root,'project');
  const files=['LICENSE','licenses/miniupnpc-LICENSE','extern/aurora/LICENSE','extern/libco/LICENSE','extern/musyx/LICENSE',
   'tools/launch-local.ps1','tools/automatic-hosting.cjs','tools/host-game.cjs','tools/ensure-hosting-tools.cjs','tools/hosting-tools.json','tools/hosting-process-job.ps1',
   'licenses/cloudflared-LICENSE','licenses/caddy-LICENSE','licenses/hosting-node-LICENSE',
   'web/index.html','web/audio/wifi-menu.opus','web/audio/currently-spectating.opus','licenses/example-LICENSE'];
  for(const file of files){fs.mkdirSync(path.dirname(path.join(project,file)),{recursive:true});fs.writeFileSync(path.join(project,file),'fixture notice content');}
  const item={file:'example-LICENSE',sha256:crypto.createHash('sha256').update('fixture notice content').digest('hex')};
  if(kind==='changed')fs.appendFileSync(path.join(project,'licenses/example-LICENSE'),'changed');
  if(kind==='missing')fs.unlinkSync(path.join(project,'licenses/example-LICENSE'));
  if(kind==='unsafe')item.file='../outside-LICENSE';
  fs.writeFileSync(path.join(project,'licenses/dependency-notices.json'),JSON.stringify({version:1,notices:kind==='excessive'?Array(257).fill(item):[item]}));
  assert.throws(()=>stageLocalPackage(f.runtime,f.target,project),kind==='changed'?/notice hash differs/:kind==='unsafe'?/Invalid dependency notice/:kind==='excessive'?/Bounded dependency notice/:/ENOENT/);
  assert.equal(fs.existsSync(f.target),false);
 }
});
