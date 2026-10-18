const fs=require('node:fs');
const http=require('node:http');
const path=require('node:path');
const crypto=require('node:crypto');
const assert=require('node:assert/strict');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const browserLaunchOptions=()=>({
  ...(process.env.PARTYBOARD_TEST_BROWSER_EXECUTABLE
    ?{executablePath:process.env.PARTYBOARD_TEST_BROWSER_EXECUTABLE}
    :{channel:process.env.PARTYBOARD_TEST_BROWSER_CHANNEL||'msedge'}),
  headless:true,args:['--mute-audio'],
});
const browserLabel=process.env.PARTYBOARD_TEST_BROWSER_EXECUTABLE?'configured Chromium':(process.env.PARTYBOARD_TEST_BROWSER_CHANNEL||'Edge');

const runtime=path.resolve(process.argv[2]||'');
const overlays=path.resolve(process.argv[3]||'');
assert.ok(fs.existsSync(path.join(runtime,'contract.mjs')),'Production Emscripten wrapper is required.');
assert.ok(fs.existsSync(path.join(runtime,'contract.wasm')),'Production Emscripten Wasm is required.');
const modules=fs.readdirSync(overlays).filter(name=>name.endsWith('.wasm')).sort();
assert.equal(modules.length,91,'Expected all 91 current side modules.');
const expected=modules.map(name=>({name,size:fs.statSync(path.join(overlays,name)).size,
  sha256:crypto.createHash('sha256').update(fs.readFileSync(path.join(overlays,name))).digest('hex')}));
const pinball=modules.find(name=>name.toLowerCase()==='m444dll.wasm');
assert.ok(pinball,'Pinball overlay is included in the runtime package.');
const pinballExpected={name:pinball,sha256:crypto.createHash('sha256').update(fs.readFileSync(path.join(overlays,pinball))).digest('hex')};

const server=http.createServer((req,res)=>{
  if(req.url==='/'){
    res.setHeader('Content-Type','text/html; charset=utf-8');res.end('<!doctype html><meta charset="utf-8"><canvas></canvas>');return;
  }
  const file=req.url==='/contract.mjs'?'contract.mjs':req.url==='/contract.wasm'?'contract.wasm':
    req.url==='/runtime-build.mjs'?'runtime-build.mjs':null;
  if(!file){res.writeHead(404);res.end();return;}
  res.setHeader('Content-Type',file.endsWith('.wasm')?'application/wasm':'text/javascript; charset=utf-8');
  const filename=file==='runtime-build.mjs'?path.resolve(__dirname,'../web/runtime-build.mjs'):path.join(runtime,file);
  fs.createReadStream(filename).on('error',()=>{res.writeHead(500);res.end();}).pipe(res);
});

(async()=>{
  let browser;
  try{
    await new Promise((resolve,reject)=>server.listen(0,'127.0.0.1',resolve).once('error',reject));
    browser=await chromium.launch(browserLaunchOptions());
    const page=await browser.newPage();page.setDefaultTimeout(180000);
    const errors=[];page.on('pageerror',error=>errors.push(error.message));
    await page.goto(`http://127.0.0.1:${server.address().port}/`);
    const result=await page.evaluate(async({expected,pinball:pinballExpected})=>{
      const {prepareFingerprintedRuntimeFactory}=await import('/runtime-build.mjs');
      const wrapper=await fetch('/contract.mjs').then(response=>response.blob());
      const wasm=await fetch('/contract.wasm').then(response=>response.blob());
      const prepared=await prepareFingerprintedRuntimeFactory([
        {name:'contract.mjs',blob:wrapper},{name:'contract.wasm',blob:wasm},
      ]);
      let runtimeModuleAtInit;
      const runtime=await prepared.createRuntime({canvas:document.createElement('canvas'),noInitialRun:true,noExitRuntime:true,
        onRuntimeInitialized(){
          runtimeModuleAtInit=this;
        },
      });
      globalThis.partyboardRuntime=runtime;
      if(typeof runtime._PartyBoardBrowserSetInitialResolution!=='function')throw Error('Initial-resolution export missing.');
      const valid=runtime.ccall('PartyBoardBrowserSetInitialResolution','number',['number','number'],[960,540]);
      const invalid=runtime.ccall('PartyBoardBrowserSetInitialResolution','number',['number','number'],[1000,562]);
      if(valid!==0||invalid!==-2)throw Error(`Resolution validator returned ${valid}/${invalid}.`);
      const mounted=[];
      for(const item of expected){
        const path='/'+item.name;
        if(!runtime.FS.analyzePath(path).exists)throw Error(`Embedded side module missing: ${item.name}`);
        const bytes=runtime.FS.readFile(path);
        if(bytes.length!==item.size)throw Error(`Embedded side module size differs: ${item.name} (runtime ${bytes.length}, package ${item.size}).`);
        const hash=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)),b=>b.toString(16).padStart(2,'0')).join('');
        if(hash!==item.sha256)throw Error(`Embedded side module digest differs: ${item.name}`);
        mounted.push(item.name);
      }
      if(runtimeModuleAtInit!==runtime)throw Error('The generated Module captured at onRuntimeInitialized differs from the resolved factory result.');
      if(!runtimeModuleAtInit.FS?.analyzePath('/'+pinballExpected.name).exists)
        throw Error('Embedded pinball overlay was not available after Emscripten startup completed.');
      // The game asks for camel-case Dll while the packaged file is lowercase.
      const pinballUrl=runtimeModuleAtInit.locateFile('/m444Dll.wasm');
      if(!pinballUrl?.startsWith('blob:'))throw Error('Case-insensitive embedded pinball overlay did not resolve through the fallback Blob URL.');
      const pinballBytes=await fetch(pinballUrl).then(response=>{
        if(!response.ok)throw Error('Embedded pinball overlay Blob URL did not serve bytes.');
        return response.arrayBuffer();
      });
      const pinballSha256=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',pinballBytes)),b=>b.toString(16).padStart(2,'0')).join('');
      if(pinballSha256!==pinballExpected.sha256)throw Error('Resolved pinball Blob URL bytes differ from the embedded verified overlay.');
      runtime.partyboardReleaseRuntimeArtifacts();
      return {validResolution:valid,invalidResolution:invalid,embeddedModules:mounted.length,
        wasmBytes:runtime.HEAPU8?.buffer.byteLength??0,pinballCapturedAtRuntimeInit:true,
        pinballBlobVerified:pinballSha256===pinballExpected.sha256};
    },{expected,pinball:pinballExpected});
    assert.deepEqual(errors,[]);
    assert.equal(result.embeddedModules,91);
    assert.equal(result.pinballCapturedAtRuntimeInit,true);
    assert.equal(result.pinballBlobVerified,true);
    console.log(`PASS production browser runtime instantiated in muted ${browserLabel}; all 91 embedded overlays matched SHA-256, the game's camel-case m444Dll request resolved to the lowercase packaged file through the startup callback fallback, and aspect-aware resolution validation passed.`);
    console.log(JSON.stringify(result));
  }finally{
    await browser?.close();
    if(server.listening)await new Promise(resolve=>server.close(resolve));
  }
})().catch(error=>{console.error(error.stack||String(error));process.exitCode=1;});
