import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import {spawnSync} from 'node:child_process';
import {createServer} from 'node:http';
import {createReadStream} from 'node:fs';
import {createRequire} from 'node:module';
import {fingerprintRuntimeFiles} from '../../web/runtime-build.mjs';
import {loadBrowserRuntimePackage} from '../../web/browser-runtime-package.mjs';
import {receiveSessionConfiguration} from '../../web/session-config.mjs';
import {receivePreparedRuntimeReport} from '../../web/prepared-runtime-report.mjs';
const nativeDirectory=path.resolve(process.env.PARTYBOARD_NATIVE_RUNTIME_DIRECTORY);
const browserDirectory=path.resolve(process.env.PARTYBOARD_BROWSER_RUNTIME_DIRECTORY);
const nativeReceiptLog=process.env.PARTYBOARD_PACKAGE_NATIVE_RECEIPT_LOG;
const pipelineSeedPath=process.env.PARTYBOARD_PACKAGE_PIPELINE_SEED;
const pipelineSeedHash=process.env.PARTYBOARD_PACKAGE_PIPELINE_SEED_SHA256;
if(pipelineSeedPath||pipelineSeedHash){assert.ok(pipelineSeedPath);assert.match(pipelineSeedHash||'',/^[a-f0-9]{64}$/);}
let actualNativeReceipt;
if(nativeReceiptLog){
 const lines=(await fs.readFile(nativeReceiptLog,'utf8')).split(/\r?\n/).filter(line=>line.startsWith('Native measured prepared receipt: '));
 assert.equal(lines.length,1,'Exactly one actual native prepared receipt required.');
 actualNativeReceipt=JSON.parse(lines[0].slice('Native measured prepared receipt: '.length));
 assert.equal(actualNativeReceipt.type,'runtime-prepared');
 assert.equal(actualNativeReceipt.frame,0);
}
const artifacts=(await fs.readdir(nativeDirectory)).filter(name=>/\.(exe|dll)$/i.test(name)).map(name=>({name:'native/'+name,path:path.join(nativeDirectory,name)}));
assert.ok(artifacts.some(file=>file.name==='native/partyboard.exe'));
assert.ok(artifacts.some(file=>file.name==='native/dol.dll'));
let resourceCount=0;
async function resources(directory,relative=''){
 for(const entry of await fs.readdir(directory,{withFileTypes:true})){
  const name=relative?relative+'/'+entry.name:entry.name,file=path.join(directory,entry.name);
  assert.ok(!entry.isSymbolicLink(),'Runtime resources must be supplied regular files, not symlinks.');
  if(entry.isDirectory())await resources(file,name);
  else {
   assert.ok(entry.isFile(),'Regular runtime resource required.');
   // Reversible UTF-8 path encoding permits original resource filenames with
   // spaces while retaining the canonical artifact name grammar on both sides.
   artifacts.push({name:'native/res/'+Buffer.from(name,'utf8').toString('hex'),path:file});resourceCount++;
  }
 }
}
await resources(path.join(nativeDirectory,'res'));
assert.ok(resourceCount>0,'Shipped native UI/controller/font resources required.');
for(const name of ['wifi-menu.opus','currently-spectating.opus'])
 artifacts.push({name:'native/audio/'+name,path:path.join(nativeDirectory,'audio',name)});
for(const name of ['contract.mjs','contract.wasm'])artifacts.push({name,path:path.join(browserDirectory,name)});
const blobs=[];
for(const file of artifacts)blobs.push({name:file.name,blob:new Blob([await fs.readFile(file.path)])});
const browser=await fingerprintRuntimeFiles(blobs);
const result=spawnSync(path.resolve(process.env.PARTYBOARD_RUNTIME_BUILD_CODEC),[],{input:JSON.stringify(artifacts),encoding:'utf8',maxBuffer:4*1024*1024});
assert.equal(result.status,0,result.stderr);assert.deepEqual(JSON.parse(result.stdout),browser);
const privateAssetPath=process.env.PARTYBOARD_PACKAGE_PRIVATE_ASSET;
const expectedAssetId=privateAssetPath?process.env.PARTYBOARD_PACKAGE_ASSET_ID:'a'.repeat(64);
assert.match(expectedAssetId,/^[a-f0-9]{64}$/);
const retainedDescriptor=process.env.PARTYBOARD_PACKAGE_RETAINED_DESCRIPTOR?
 JSON.parse(await fs.readFile(process.env.PARTYBOARD_PACKAGE_RETAINED_DESCRIPTOR,'utf8')):null;
if(retainedDescriptor){
 assert.equal(retainedDescriptor.configuration.buildId,browser.buildId);
 const ordered=files=>files.slice().sort((a,b)=>a.name<b.name?-1:a.name>b.name?1:0);
 assert.deepEqual(ordered(retainedDescriptor.files),ordered(artifacts));
}
const configured=spawnSync(path.resolve(process.env.PARTYBOARD_RUNTIME_BUILD_CODEC),[],{
 input:JSON.stringify({files:artifacts,expectedBuildId:browser.buildId,
  ...(retainedDescriptor?{session:retainedDescriptor.configuration.session,seed:retainedDescriptor.configuration.seed}:{newHostSession:true}),assetId:expectedAssetId,
  ...(privateAssetPath?{assetPath:privateAssetPath}:{})}),
 encoding:'utf8',maxBuffer:4*1024*1024});
assert.equal(configured.status,0,configured.stderr);
const configuredResult=JSON.parse(configured.stdout);
const configuration=receiveSessionConfiguration(JSON.stringify(configuredResult.configuration),{assetId:expectedAssetId});
if(retainedDescriptor)assert.deepEqual(configuration,retainedDescriptor.configuration);
if(actualNativeReceipt)assert.deepEqual(receivePreparedRuntimeReport(JSON.stringify(actualNativeReceipt),configuration),actualNativeReceipt);
if(privateAssetPath){assert.equal(configuredResult.assetManifest.id,expectedAssetId);assert.equal(configuredResult.assetManifest.size,(await fs.stat(privateAssetPath)).size);}
assert.equal(configuration.buildId,browser.buildId);
assert.equal(configuration.runtimeArtifacts.length,artifacts.length-2);
if(process.env.PARTYBOARD_PACKAGE_DESCRIPTOR)
 await fs.writeFile(path.resolve(process.env.PARTYBOARD_PACKAGE_DESCRIPTOR),JSON.stringify({configuration,files:artifacts})+'\n');
assert.notEqual(configuration.session,'0'.repeat(32));
assert.ok(Number.isInteger(configuration.seed)&&configuration.seed>=0&&configuration.seed<=4294967295);
// Duplicate delivery retains the exact native-generated configuration. A peer
// cannot replace its seed or measured package while downloading that package.
assert.equal(receiveSessionConfiguration(JSON.stringify(configuration),{
 assetId:expectedAssetId,current:configuration}),configuration);
for(const change of [{seed:configuration.seed===4294967295?0:configuration.seed+1},
 {buildId:'0'.repeat(64)}])assert.throws(()=>receiveSessionConfiguration(JSON.stringify({...configuration,...change}),{
 assetId:expectedAssetId,current:configuration}));
const publicFiles=new Map(artifacts.map(file=>[file.name,file.path]));
let requests=0;
let browserInstantiationVerified=false;
let browserPreparedEngineVerified=false;
const preparedReceipts=[];
const helpers=new Set((await fs.readdir(path.resolve('outputs/partyboard/web'))).filter(name=>name.endsWith('.mjs')&&!name.endsWith('.test.mjs')));
const server=createServer((request,response)=>{
 const name=new URL(request.url,'http://127.0.0.1').pathname.slice(1);
 if(name===''){response.setHeader('Content-Type','text/html');response.end('<!doctype html><input type="file"><canvas id="canvas" width="640" height="480"></canvas>');return;}
 const helper=helpers.has(name);
 const file=name==='pipeline-seed.db'&&pipelineSeedPath?pipelineSeedPath:helper?path.resolve('outputs/partyboard/web',name):publicFiles.get(name);
 if(!file){response.writeHead(404);response.end();return;}
 response.setHeader('Content-Type',name.endsWith('.mjs')?'text/javascript':name.endsWith('.wasm')?'application/wasm':'application/octet-stream');
 requests++;createReadStream(file).on('error',()=>response.destroy()).pipe(response);
});
await new Promise((resolve,reject)=>{server.once('error',reject);server.listen(0,'127.0.0.1',resolve);});
try {
 const loaded=await loadBrowserRuntimePackage({baseURL:`http://127.0.0.1:${server.address().port}/`,
  supplementalArtifacts:configuration.runtimeArtifacts});
 assert.equal(requests,artifacts.length);
 assert.deepEqual(await fingerprintRuntimeFiles(loaded),browser);
 if(process.env.PARTYBOARD_PACKAGE_EDGE==='1'){
  const {chromium}=createRequire(import.meta.url)(process.env.PLAYWRIGHT_MODULE||'playwright');
  const edge=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});
  try{
   const peers=Number(process.env.PARTYBOARD_PACKAGE_EDGE_PEERS||1);assert.ok([1,2,4].includes(peers));
   for(let peer=0;peer<peers;peer++){
   const context=await edge.newContext(),page=await context.newPage();
   page.on('console',message=>{if(message.text().startsWith('Package preparation stage: '))console.log(message.text());});
   page.on('pageerror',error=>console.error('Package browser error:',error.message));
   await page.goto(`http://127.0.0.1:${server.address().port}/`);
   const prepareEngine=process.env.PARTYBOARD_PACKAGE_PREPARE==='1';
   if(prepareEngine){assert.ok(privateAssetPath,'Actual private asset required for engine preparation');await page.locator('input').setInputFiles(privateAssetPath);}
   const evidence=await page.evaluate(async ({configuration,prepareEngine,pipelineSeedHash})=>{
    const {receiveSessionConfiguration}=await import('/session-config.mjs');
    const retained=receiveSessionConfiguration(JSON.stringify(configuration),{assetId:configuration.assetId});
    const {loadBrowserRuntimePackage}=await import('/browser-runtime-package.mjs');
    const {prepareFingerprintedRuntimeFactory}=await import('/runtime-build.mjs');
    const files=await loadBrowserRuntimePackage({baseURL:location.origin+'/',supplementalArtifacts:retained.runtimeArtifacts});
    if(prepareEngine){
     const {prepare,browserStore,CHUNK}=await import('/transfer.mjs');
     console.info('Package preparation stage: asset-hash '+new Date().toISOString());
     const file=document.querySelector('input').files[0],asset=await prepare(file),store=await browserStore();
     if(asset.manifest.id!==retained.assetId)throw Error('Selected private asset differs from native measurement');
     console.info('Package preparation stage: asset-store '+new Date().toISOString());
     for(let i=0;i<asset.manifest.count;i++)await store.put(asset.manifest.id,i,{bytes:await file.slice(i*CHUNK,(i+1)*CHUNK).arrayBuffer(),hash:asset.hashes.slice(i*32,(i+1)*32)});
     await store.complete(asset.manifest);
     const {browserPlatformCallbacks}=await import('/browser-platform.mjs');
     const {prepareFingerprintedSessionRuntime}=await import('/session-runtime.mjs');
     let pipelineSeed;
     if(pipelineSeedHash){
      const response=await fetch('/pipeline-seed.db');if(!response.ok)throw Error('Pipeline seed fetch failed');
      pipelineSeed={blob:await response.blob(),sha256:pipelineSeedHash,scope:'observed-scenes-only'};
     }
     const platform=browserPlatformCallbacks({maxSteps:pipelineSeed?10000:60});
     console.info('Package preparation stage: engine-renderer '+new Date().toISOString());
     const runtime=await prepareFingerprintedSessionRuntime(store,retained.assetId,files,{
      runtimeBuild:{expectedBuildId:retained.buildId},sessionSeed:retained.seed,pipelineSeed,
      runtimeOptions:{canvas:document.querySelector('canvas'),print:()=>{},printErr:()=>{}},
      setup:platform.setup,prepareRenderer:platform.prepareRenderer,shutdown:platform.shutdown});
     console.info('Package preparation stage: ready '+new Date().toISOString());
     const evidence={buildId:runtime.buildId,phase:runtime.module._PartyBoard_GameLifecyclePhase(),frame:runtime.module._PartyBoard_GameFrameCount(),artifactCount:files.length};
     const {preparedRuntimeReport}=await import('/prepared-runtime-report.mjs');
     evidence.receipt=preparedRuntimeReport(runtime,retained);
     await runtime.dispose();return evidence;
    }
    const factory=await prepareFingerprintedRuntimeFactory(files,{expectedBuildId:retained.buildId});
    const module=await factory.createRuntime({canvas:document.querySelector('canvas'),print:()=>{},printErr:()=>{}});
    return {buildId:factory.buildId,phase:module._PartyBoard_GameLifecyclePhase(),frame:module._PartyBoard_GameFrameCount(),artifactCount:files.length};
   },{configuration,prepareEngine,pipelineSeedHash});
   const {receipt,...state}=evidence;
   assert.deepEqual(state,{buildId:browser.buildId,phase:prepareEngine?2:0,frame:0,artifactCount:artifacts.length});
   if(prepareEngine){assert.ok(receipt);if(preparedReceipts.length)assert.deepEqual(receipt,preparedReceipts[0]);preparedReceipts.push(receipt);}
   if(process.env.PARTYBOARD_PACKAGE_NATIVE_CHECKPOINT!==undefined){assert.ok(prepareEngine);assert.equal(receipt.checkpoint,Number(process.env.PARTYBOARD_PACKAGE_NATIVE_CHECKPOINT));}
   if(actualNativeReceipt){assert.ok(prepareEngine);assert.deepEqual(receipt,actualNativeReceipt,'Complete native/browser prepared receipts must match.');}
   browserInstantiationVerified=true;
   browserPreparedEngineVerified=prepareEngine;
   await context.close();
   console.log('Actual independent Edge package peer '+peer+' prepared and cleaned up.');
   }
  }finally{await edge.close();}
 }
}finally{await new Promise((resolve,reject)=>server.close(error=>error?reject(error):resolve()));}
const report={scope:'Measured common package of supplied native executable/DLLs/resources and browser wrapper/Wasm; canonical fingerprint parity only',
 ...browser,artifactCount:artifacts.length,nativeBrowserCanonicalParity:true,actualSharedHttpPackageLoaderParity:true,
 compiledNativeFullConfigurationAccepted:true,configurationSessionIsFixture:false,
 nativeSessionEntropyGenerated:true,retainedConfigurationDriftRejected:true,
 actualEdgeSharedPackageInstantiationVerified:browserInstantiationVerified,
 actualEdgeSharedPackagePreparedEngineVerified:browserPreparedEngineVerified,
 actualIndependentPreparedPeerCount:preparedReceipts.length,matchingPreparedReceipts:preparedReceipts.length>1,
 preparedReceipts,
 retainedNativeConfigurationUsed:!!retainedDescriptor,
 nativeBrowserPreparedCheckpointMatched:preparedReceipts.length>0&&process.env.PARTYBOARD_PACKAGE_NATIVE_CHECKPOINT!==undefined,
 nativeBrowserCompletePreparedReceiptMatched:preparedReceipts.length>0&&!!actualNativeReceipt,
 configurationAssetIsFixture:!privateAssetPath,nativePrivateAssetMeasured:!!privateAssetPath,
 resourceCount,resourcePathEncoding:'native/res/ followed by hex UTF-8 relative path with slash separators',
 nonExecutableResourcesEnumerated:true,loadedRuntimeAttestationVerified:false,crossPlatformCompatibilityVerified:false,productionStartVerified:false};
await fs.writeFile(process.env.PARTYBOARD_PACKAGE_REPORT||'outputs/validation/native-browser-measured-runtime-package-20261005.json',JSON.stringify(report,null,2)+'\n');
console.log('PASS measured native/browser artifact package parity: '+artifacts.length+' files; '+browser.buildId);
