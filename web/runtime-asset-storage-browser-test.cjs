'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),{spawn}=require('node:child_process');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const {browserStorageObservation}=require('../tools/native-rtc-probe/browser-storage-observation.cjs');
(async()=>{
 const output=path.resolve(process.env.PARTYBOARD_TEST_OUTPUT_DIRECTORY||'outputs/validation/asset-storage-'+Date.now());
 assert.equal(fs.existsSync(output),false,'Preserve previous storage evidence');fs.mkdirSync(output,{recursive:true});
 const env={...process.env,PARTYBOARD_PORT:'8955'};
 for(const key of ['PARTYBOARD_RUNTIME_PACKAGE','PARTYBOARD_RUNTIME_DIRECTORY','PARTYBOARD_TEST_RUNTIME_DIRECTORY','PARTYBOARD_TLS_CERTIFICATE','PARTYBOARD_TLS_KEY','PARTYBOARD_LISTEN_ADDRESS'])delete env[key];
 let server,browser,session,page;
 try{
  server=spawn(process.execPath,[path.join(__dirname,'serve.cjs')],{env,windowsHide:true,stdio:['ignore','pipe','pipe']});
  await new Promise((resolve,reject)=>{server.stdout.once('data',resolve);server.once('error',reject);server.once('exit',code=>reject(Error('Server exited '+code)));});
  browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});page=await browser.newPage();
  await page.goto('http://127.0.0.1:8955/');const origin=new URL(page.url()).origin;
  const asset=await page.evaluate(async()=>{
   const {prepare,browserStore,CHUNK,digest}=await import('/transfer.mjs');
   const source=new Blob([Uint8Array.from({length:64*1024*1024},(_,i)=>i%251)]),prepared=await prepare(source);
   const store=await browserStore();
   for(let index=0;index<prepared.manifest.count;index++){
    const bytes=await source.slice(index*CHUNK,(index+1)*CHUNK).arrayBuffer();
    await store.put(prepared.manifest.id,index,{bytes,hash:await digest(bytes)});
   }
   await store.complete(prepared.manifest);store.close();return prepared.manifest;
  });
  const before=await browserStorageObservation(page,{files:true});
  session=await page.context().newCDPSession(page);
  // Lower the test origin's quota to reproduce real insufficient headroom.
  // Never disable quota enforcement or raise the browser's normal limit.
  const limitedQuota=before.protocol.usage+16*1024*1024;
  assert.ok(limitedQuota<before.protocol.quota);
  await session.send('Storage.overrideQuotaForOrigin',{origin,quotaSize:limitedQuota});
  const limited=await page.evaluate(async id=>{
   const {browserStore}=await import('/transfer.mjs');
   const {verifiedAssetBlob,verifyStoredAsset}=await import('/runtime-asset.mjs');const store=await browserStore();
   try{
    let failure;try{const result=await verifiedAssetBlob(store,id);await result.dispose?.();}catch(error){failure={name:error.name,message:error.message};}
    if(!failure)throw Error('The limited quota must reject a full gameplay copy');
    const progress=[],manifest=await verifyStoredAsset(store,id,{progress:(bytes,total)=>progress.push([bytes,total])});
    return {copyFailure:failure,manifest,progress:progress.at(-1)};
   }finally{store.close();}
  },asset.id);
  assert.equal(limited.copyFailure.name,'QuotaExceededError');assert.deepEqual(limited.manifest,asset);assert.deepEqual(limited.progress,[asset.size,asset.size]);
  const afterPreflight=await browserStorageObservation(page,{files:true});
  assert.deepEqual(afterPreflight.temporaryFiles,[]);assert.equal(afterPreflight.estimate.usageDetails.fileSystem||0,0);
  await session.send('Storage.overrideQuotaForOrigin',{origin});
  const mounted=await page.evaluate(async id=>{
   const {browserStore,CHUNK}=await import('/transfer.mjs');const {verifiedAssetBlob}=await import('/runtime-asset.mjs');const store=await browserStore();
   try{
    const original=await store.get(id,0),corrupted=original.bytes.slice(0);new Uint8Array(corrupted)[0]^=1;await store.put(id,0,{...original,bytes:corrupted});
    let corruptionRejected=false;try{const result=await verifiedAssetBlob(store,id);await result.dispose?.();}catch(error){corruptionRejected=/integrity/.test(error.message);}
    await store.put(id,0,original);const result=await verifiedAssetBlob(store,id);let exact=true;
    try{for(let offset=0;offset<result.blob.size;offset+=CHUNK){const bytes=new Uint8Array(await result.blob.slice(offset,offset+CHUNK).arrayBuffer());if(!bytes.every((value,index)=>value===(offset+index)%251)){exact=false;break;}}}
    finally{await result.dispose?.();}
    return {corruptionRejected,exact};
   }finally{store.close();}
  },asset.id);
  assert.deepEqual(mounted,{corruptionRejected:true,exact:true});
  const after=await browserStorageObservation(page,{files:true});assert.deepEqual(after.temporaryFiles,[]);assert.equal(after.protocol.overrideActive,false);
  const report={status:'passed',scope:'real Edge IndexedDB/OPFS, 64 MiB synthetic asset; lowered quota only',asset,limitedQuota,before,limited,afterPreflight,mounted,after};
  fs.writeFileSync(path.join(output,'acceptance.json'),JSON.stringify(report,null,2));console.log('PASS: bounded preflight succeeds without a second file under low quota; real mount still rejects corruption, reproduces exact bytes and cleans up.');
 }catch(error){fs.writeFileSync(path.join(output,'failure.json'),JSON.stringify({error:error.stack},null,2));throw error;}
 finally{await session?.detach().catch(()=>{});await browser?.close();server?.kill();}
})().catch(error=>{console.error(error);process.exitCode=1;});
