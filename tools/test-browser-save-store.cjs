const http=require('node:http'),fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const server=http.createServer((req,res)=>{
  if(req.url==='/'){res.end('<!doctype html><title>Save persistence contract</title>');return;}
  if(!['/host-save-store.mjs','/runtime-save.mjs'].includes(req.url)){res.writeHead(404);res.end();return;}
  res.setHeader('Content-Type','text/javascript');
  res.end(fs.readFileSync(path.join(__dirname,'../web',req.url.slice(1))));
});
(async()=>{
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  let browser;
  try {
    browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});
    const page=await browser.newPage(), errors=[];
    page.on('pageerror',error=>errors.push(String(error)));
    const url=`http://127.0.0.1:${server.address().port}/`;
    await page.goto(url);
    const created=await page.evaluate(async()=>{
      const {openHostSaveStore}=await import('/host-save-store.mjs');
      const store=await openHostSaveStore(), asset='a'.repeat(64), bytes=new Uint8Array(8192).fill(17);
      const first=await store.commit(asset,0,bytes,{expectedRevision:0,role:'host'});
      bytes.fill(99);
      if((await store.load(asset,0)).bytes[0]!==17)throw Error('Snapshot alias');
      for(const role of ['guest',undefined]) {
        let rejected=false;
        try {await store.commit(asset,0,bytes,{expectedRevision:1,role});}catch{rejected=true;}
        if(!rejected)throw Error('Guest write accepted');
      }
      const second=await openHostSaveStore();
      const raced=await Promise.allSettled([
        store.commit(asset,0,new Uint8Array(8192).fill(23),{expectedRevision:1,role:'host'}),
        second.commit(asset,0,new Uint8Array(8192).fill(23),{expectedRevision:1,role:'host'})
      ]);
      if(raced.filter(result=>result.status==='fulfilled').length!==1)throw Error('Stale write race');
      if(await store.load('b'.repeat(64),0) || await store.load(asset,1))throw Error('Identity isolation');
      store.close();second.close();
      return first.revision;
    });
    assert.equal(created,1);
    await page.reload();
    const restored=await page.evaluate(async()=>{
      const {openHostSaveStore}=await import('/host-save-store.mjs');
      const store=await openHostSaveStore(),asset='a'.repeat(64), saved=await store.load(asset,0);
      if(saved.revision!==2 || saved.bytes.some(byte=>byte!==23))throw Error('Reload mismatch');
      store.close();
      const db=await new Promise((resolve,reject)=>{const r=indexedDB.open('partyboard-host-saves-v1',1);r.onsuccess=()=>resolve(r.result);r.onerror=()=>reject(r.error);});
      await new Promise((resolve,reject)=>{
        const tx=db.transaction('cards','readwrite'),cards=tx.objectStore('cards');
        cards.get(asset+':0').onsuccess=event=>{const record=event.target.result;record.bytes[0]^=1;cards.put(record);};
        tx.oncomplete=resolve;tx.onabort=()=>reject(tx.error);
      });
      db.close();
      const damaged=await openHostSaveStore();
      let rejected=false;
      try {await damaged.load(asset,0);}catch(error){rejected=error.message.includes('integrity');}
      damaged.close();
      if(!rejected)throw Error('Corrupt persisted save accepted');
      return saved.revision;
    });
    assert.equal(restored,2);assert.deepEqual(errors,[]);
    await page.evaluate(async()=>{
      const {openHostSaveStore}=await import('/host-save-store.mjs');
      const {prepareHostCard,prepareHostSaveSession,prepareGuestSaveSession}=await import('/runtime-save.mjs');
      const store=await openHostSaveStore(),assetId='c'.repeat(64),path='/MemoryCardA.USA.raw';
      let bytes=new Uint8Array(8192).fill(31);
      const FS={readFile:()=>bytes,writeFile:(name,value)=>{if(name!==path)throw Error('Wrong path');bytes=value.slice();}};
      const options={store,assetId,slot:0,path,FS,role:'host'};
      const adapter=await prepareHostCard(options);
      const first=adapter.completedSave();
      bytes.fill(42);
      const second=adapter.completedSave();
      bytes.fill(99);
      const results=await Promise.all([first,second]);
      if(results[0].revision!==1 || results[1].revision!==2)throw Error('Unserialized saves');
      if((await store.load(assetId,0)).bytes[0]!==42)throw Error('Snapshot changed during commit');
      await adapter.close();
      let rejected=false;try{adapter.completedSave();}catch{rejected=true;}
      if(!rejected)throw Error('Closed adapter accepted save');
      const fresh=await prepareHostCard(options);
      if(bytes[0]!==42)throw Error('Pre-start restore failed');
      await store.commit(assetId,0,new Uint8Array(8192).fill(73),{expectedRevision:2,role:'host'});
      rejected=false;try{await fresh.completedSave();}catch{rejected=true;}
      if(!rejected)throw Error('Conflict not surfaced');
      rejected=false;try{await fresh.flush();}catch{rejected=true;}
      if(!rejected)throw Error('Flush hid persistence failure');
      if((await store.load(assetId,0)).bytes[0]!==73)throw Error('Conflict overwrote save');
      rejected=false;try{await prepareHostCard({...options,role:'guest'});}catch{rejected=true;}
      if(!rejected)throw Error('Guest adapter accepted');
      bytes.fill(99);
      const session=await prepareHostSaveSession(options);
      if(session.status.state!=='prepared' || bytes[0]!==99)throw Error('Premature runtime access');
      session.preRun({FS});
      if(bytes[0]!==73 || session.status.state!=='ready')throw Error('Session restore failed');
      bytes.fill(81);session.onCardSaveCompleted(0);
      if(session.status.state!=='saving' || session.status.pending!==1)throw Error('Pending status missing');
      if(await session.flush()!==4 || session.status.state!=='ready')throw Error('Completion status missing');
      session.onCardSaveCompleted(1);
      if(session.status.state!=='error')throw Error('Callback failure hidden');
      rejected=false;try{await session.flush();}catch{rejected=true;}
      if(!rejected)throw Error('Session flush hid callback failure');
      rejected=false;try{session.preRun({FS});}catch{rejected=true;}
      if(!rejected)throw Error('Double runtime attachment accepted');
      try{await session.close();}catch{}
      const hostSnapshot=await store.load(assetId,0);
      const guestOptions={bytes:hostSnapshot.bytes,sha256:hostSnapshot.sha256,assetId,expectedAssetId:assetId,slot:0,path};
      const guest=await prepareGuestSaveSession(guestOptions);
      hostSnapshot.bytes.fill(0);
      guest.preRun({FS});
      if(bytes[0]!==81 || guest.status.persistent!==false)throw Error('Guest snapshot isolation failed');
      bytes.fill(92);guest.onCardSaveCompleted(0);await guest.flush();
      if((await store.load(assetId,0)).bytes[0]!==81)throw Error('Guest changed persistent host card');
      await guest.close();
      if(guest.status.state!=='closed')throw Error('Guest close failed');
      guestOptions.bytes=(await store.load(assetId,0)).bytes;
      for(const overrides of [{expectedAssetId:'d'.repeat(64)},{sha256:'0'.repeat(64)},{slot:2},{bytes:new Uint8Array(3)}]) {
        rejected=false;try{await prepareGuestSaveSession({...guestOptions,...overrides});}catch{rejected=true;}
        if(!rejected)throw Error('Invalid guest snapshot accepted');
      }
      store.close();
    });
    assert.deepEqual(errors,[]);
    console.log('PASS: Runtime card adapter restores before startup, serializes snapshots, rejects guests and surfaces stale-save failures.');
    console.log('PASS: IndexedDB host saves survive page reload, isolate asset/slot identities, reject guest/stale writes and detect corruption.');
  } finally {if(browser)await browser.close();server.closeAllConnections();await new Promise(resolve=>server.close(resolve));}
})().catch(error=>{console.error(error);process.exitCode=1;});
