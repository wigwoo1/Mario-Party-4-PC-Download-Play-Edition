const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const assert=require('node:assert/strict');
(async()=>{
  const browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});
  try {
    const page=await browser.newPage(), errors=[];
    page.on('pageerror',error=>errors.push(error.message));
    await page.goto(process.env.PARTYBOARD_TEST_URL||'http://127.0.0.1:8785');
    const id=await page.evaluate(async()=>{
      const {prepare,browserStore,CHUNK,digest}=await import('/transfer.mjs');
      const asset=await prepare(new Blob([Uint8Array.from({length:600123},(_,i)=>i%251)]));
      const store=await browserStore();
      for(let index=0;index<asset.manifest.count;index++) {
        const bytes=await asset.file.slice(index*CHUNK,(index+1)*CHUNK).arrayBuffer();
        await store.put(asset.manifest.id,index,{bytes,hash:await digest(bytes)});
      }
      await store.complete(asset.manifest); store.close(); return asset.manifest.id;
    });
    await page.reload();
    const result=await page.evaluate(async id=>{
      const {browserStore}=await import('/transfer.mjs');
      const {verifiedAssetBlob}=await import('/runtime-asset.mjs');
      const store=await browserStore(), progress=[];
      try {
        const {blob}=await verifiedAssetBlob(store,id,{progress:(n,total)=>progress.push([n,total])});
        const bytes=new Uint8Array(await blob.arrayBuffer());
        const exact=bytes.length===600123&&bytes.every((value,index)=>value===index%251);
        const original=await store.get(id,0), corrupted=original.bytes.slice(0);
        new Uint8Array(corrupted)[0]^=1;
        await store.put(id,0,{...original,bytes:corrupted});
        let corruptionRejected=false;
        try {await verifiedAssetBlob(store,id);} catch(error) {corruptionRejected=/integrity/.test(error.message);}
        await store.put(id,0,original);
        const recovered=await verifiedAssetBlob(store,id);
        let incompleteRejected=false;
        try {await verifiedAssetBlob(store,'0'.repeat(64));} catch(error) {incompleteRejected=/not complete/.test(error.message);}
        return {exact,progress:progress.at(-1),corruptionRejected,incompleteRejected,recoveredSize:recovered.blob.size};
      } finally {store.close();}
    },id);
    assert.deepEqual(errors,[]);
    assert.deepEqual(result,{exact:true,progress:[600123,600123],corruptionRejected:true,incompleteRejected:true,recoveredSize:600123});
    console.log('PASS: Edge IndexedDB asset handoff survives reload, verifies exact bytes and rejects stored corruption.');
  } finally {await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
