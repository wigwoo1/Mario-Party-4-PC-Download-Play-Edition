const http=require('node:http');
const fs=require('node:fs');
const path=require('node:path');
const assert=require('node:assert/strict');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
if(!process.argv[2])throw Error('Pass the Asyncify-enabled browser contract directory');
const directory=path.resolve(process.argv[2]);
const files=new Map(['transfer.mjs','runtime-asset.mjs','verified-asset-file.mjs','runtime-mount.mjs',
  'async-blob-fs.mjs','synthetic-disc-test.mjs','session-runtime.mjs','engine-lifecycle.mjs'].map(name=>['/'+name,path.join(__dirname,name)]));
for(const name of ['contract.mjs','contract.wasm'])files.set('/'+name,path.join(directory,name));
const server=http.createServer((req,res)=>{
  if(req.method==='GET'&&req.url==='/'){
    res.setHeader('Content-Type','text/html');res.end('<!doctype html><title>Runtime mount lifecycle</title>');return;
  }
  const file=req.method==='GET'&&files.get(req.url);
  if(!file){res.writeHead(404);res.end();return;}
  res.setHeader('Content-Type',file.endsWith('.wasm')?'application/wasm':'text/javascript');
  fs.createReadStream(file).on('error',()=>{res.statusCode=500;res.end();}).pipe(res);
});
(async()=>{
  let browser;
  try {
    await new Promise((resolve,reject)=>{server.once('error',reject);server.listen(0,'127.0.0.1',resolve);});
    browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});
    const page=await browser.newPage(),errors=[];
    page.on('pageerror',error=>errors.push(error.message));
    await page.goto(`http://127.0.0.1:${server.address().port}/`);
    const result=await page.evaluate(async()=>{
      const {prepare,browserStore,CHUNK,digest}=await import('/transfer.mjs');
      const {syntheticCiso}=await import('/synthetic-disc-test.mjs');
      const {prepareMountedRuntime}=await import('/runtime-mount.mjs');
      const {prepareSessionRuntime}=await import('/session-runtime.mjs');
      const {default:create}=await import('/contract.mjs');
      const asset=await prepare(new Blob([syntheticCiso()])),store=await browserStore();
      try {
        for(let i=0;i<asset.manifest.count;i++) {
          const bytes=await asset.file.slice(i*CHUNK,(i+1)*CHUNK).arrayBuffer();
          await store.put(asset.manifest.id,i,{bytes,hash:await digest(bytes)});
        }
        await store.complete(asset.manifest);
        const output=[],events=[];let stream;
        const handle=await prepareMountedRuntime(store,asset.manifest.id,
          options=>create({...options,print:line=>output.push(line),printErr:line=>output.push(line)}),{
            shutdown:async module=>{events.push('shutdown');if(stream){module.FS.close(stream);stream=undefined;}},
          });
        const module=handle.module;
        stream=module.FS.open('/assets/verified-disc.ciso','r');
        const mounted=stream.node.asyncBlob===handle.stats&&module.FS.stat('/assets/verified-disc.ciso').size===65536;
        let openBlocked=false;
        try {handle.stats.dispose();}catch(error){openBlocked=/Close disc streams/.test(error.message);}
        await Promise.all([handle.dispose(),handle.dispose()]);
        await handle.dispose();
        const sessionEvents=[];let sessionStream;
        const prepared=await prepareSessionRuntime(store,asset.manifest.id,
          options=>create({...options,print:()=>{},printErr:()=>{}}),{sessionSeed:7,
            setup:async module=>{sessionEvents.push('setup');sessionStream=module.FS.open('/assets/verified-disc.ciso','r');},
            shutdown:async module=>{sessionEvents.push('shutdown');module.FS.close(sessionStream);},
            prepareEngine:async(module,options)=>{
              if(options.sessionSeed!==7||module.FS.stat('/assets/verified-disc.ciso').size!==65536)throw Error('Session mount handoff differs');
              sessionEvents.push('prepare');return {checkpoint:42,sessionSeed:7,engine:{phase:'Idle'},
                dispose:async()=>{sessionEvents.push('dispose');await options.shutdown(module);}};
            },
          });
        const sessionModule=prepared.module,sessionIdle=prepared.engine.phase==='Idle'&&prepared.assetId===asset.manifest.id;
        await Promise.all([prepared.dispose(),prepared.dispose()]);
        const sessionReleased=prepared.module===undefined&&!sessionModule.FS.analyzePath('/assets').exists;
        return {sessionIdle,sessionReleased,sessionEvents,mounted,openBlocked,shutdownCount:events.length,
          released:handle.module===undefined&&!module.FS.analyzePath('/assets').exists,
          gameAutoStarted:output.some(line=>/Disc renderer passed|Game system initialization passed/.test(line))};
      } finally {store.close();}
    });
    assert.deepEqual(errors,[]);
    assert.deepEqual(result,{sessionIdle:true,sessionReleased:true,sessionEvents:['setup','prepare','dispose','shutdown'],mounted:true,openBlocked:true,shutdownCount:1,released:true,gameAutoStarted:false});
    console.log('PASS: Real Edge/Wasm filesystem mounts verified IndexedDB fixture without game startup; open-stream guard and ordered idempotent shutdown/unmount pass. Composed session handoff passes with a controlled engine preparation callback (no game launch claim).');
  } finally {if(browser)await browser.close();await new Promise(resolve=>server.close(resolve));}
})().catch(error=>{console.error(error);process.exitCode=1;});

