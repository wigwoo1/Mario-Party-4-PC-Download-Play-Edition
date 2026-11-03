'use strict';
// Real production preparation and WebGPU in an isolated browser. Controlled
// visibilitychange events exercise SDL's visibility handler; no game-state or
// RNG writes, no game execution, and no physical-device claim.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {spawn}=require('node:child_process');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const [descriptor,disc,output,mode='fixed']=process.argv.slice(2);
assert.ok(descriptor&&disc&&output,'Pass runtime descriptor, private disc path and fresh evidence directory');
assert.ok(['baseline','fixed'].includes(mode));assert.ok(!fs.existsSync(output));fs.mkdirSync(output,{recursive:true});
const port=Number(process.env.PARTYBOARD_TEST_PORT||8998);
const env={...process.env,PARTYBOARD_RUNTIME_PACKAGE:path.resolve(descriptor),PARTYBOARD_LISTEN_ADDRESS:'127.0.0.1',PARTYBOARD_TEST_PORT:String(port)};
for(const key of ['PARTYBOARD_TLS_CERTIFICATE','PARTYBOARD_TLS_KEY','PARTYBOARD_PUBLIC_ORIGIN','PARTYBOARD_RUNTIME_DIRECTORY','PARTYBOARD_TEST_RUNTIME_DIRECTORY'])delete env[key];
const server=spawn(process.execPath,[path.resolve(__dirname,'../web/serve.cjs')],{env,windowsHide:true,stdio:['ignore','pipe','pipe']});
let browser;const logs=[],errors=[];
(async()=>{
  const serverLog=[];server.stderr.on('data',b=>serverLog.push(String(b)));
  await new Promise((resolve,reject)=>{server.stdout.once('data',resolve);server.once('error',reject);server.once('exit',code=>reject(Error('Test server exited '+code)));});
  browser=await chromium.launch({channel:process.env.PARTYBOARD_TEST_BROWSER_CHANNEL||'chrome',headless:true,args:['--mute-audio']});
  const context=await browser.newContext(),page=await context.newPage();
  page.on('pageerror',error=>errors.push(error.stack||error.message));page.on('console',message=>logs.push(message.text()));
  await page.addInitScript(()=>{
    globalThis.visibilityProbe={stage:'loading',waiting:[],hidden:false,deviceLost:false};
    Object.defineProperty(document,'hidden',{configurable:true,get:()=>visibilityProbe.hidden});
    const request=GPUAdapter.prototype.requestDevice;
    GPUAdapter.prototype.requestDevice=async function(...args){const device=await request.apply(this,args);globalThis.visibilityProbeDevice=device;return device;};
  });
  await page.goto('http://127.0.0.1:'+port+'/');
  await page.locator('#file').setInputFiles(path.resolve(disc));
  // Avoid invoking the app's Host flow: this test owns one prepared runtime.
  const evaluation=page.evaluate(async()=>{
    const [{prepare},{localAssetStore},{loadBrowserRuntimePackage,loadSharedRuntimeInventory},{prepareFingerprintedSessionRuntime},{browserPlatformCallbacks},{loadBundledPipelineSeed}]=await Promise.all([
      import('/transfer.mjs'),import('/local-asset-store.mjs'),import('/browser-runtime-package.mjs'),import('/session-runtime.mjs'),import('/browser-platform.mjs'),import('/bundled-pipeline-seed.mjs')]);
    const baseURL=location.origin+'/',asset=await prepare(document.querySelector('#file').files[0]);
    const inventory=await loadSharedRuntimeInventory({baseURL}),files=await loadBrowserRuntimePackage({baseURL,supplementalArtifacts:inventory.runtimeArtifacts});
    const pipelineSeed=await loadBundledPipelineSeed({baseURL});
    document.querySelector('#game-view').hidden=false;const canvas=document.querySelector('#canvas');canvas.style.width='640px';canvas.style.height='480px';
    const hide=(value,stage)=>{visibilityProbe.hidden=value;if(stage)visibilityProbe.stage=stage;document.dispatchEvent(new Event('visibilitychange'));};
    let module,engine,checkpoint,drainingHidden=false;
    const capture=()=>({phase:engine.phase,frame:module._PartyBoard_GameFrameCount(),checkpoint:engine.preparedCheckpoint()});
    const callbacks=browserPlatformCallbacks({maxSteps:10000,progress:state=>{
      if(state.waitingForWindow)visibilityProbe.waiting.push(visibilityProbe.stage);
      else if(state.queued>0&&!drainingHidden){drainingHidden=true;hide(true,'draining');}
    }});
    const prepared=await prepareFingerprintedSessionRuntime(localAssetStore(asset),asset.manifest.id,files,{
      ...callbacks,sessionSeed:25,pipelineSeed,runtimeBuild:{expectedBuildId:inventory.buildId},
      runtimeOptions:{canvas,print:line=>console.info(line),printErr:line=>console.warn(line)},
      prepareRenderer:async(m,options)=>{
        module=m;engine=options.engine;checkpoint=engine.preparedCheckpoint();
        globalThis.captureVisibilityProbe=capture;hide(true,'initial');
        try{return await callbacks.prepareRenderer(m,options);}
        catch(error){visibilityProbe.failure={message:error.message,state:capture()};throw error;}
      },
    });
    const ready={...capture(),buildId:prepared.buildId,pipelines:prepared.rendererPreparation,waiting:visibilityProbe.waiting.slice()};
    visibilityProbe.stage='ready';
    hide(true,'cancellation');
    const abort=new AbortController();
    const cancelled=callbacks.prepareRenderer(module,{engine,signal:abort.signal}).then(
      ()=>{throw Error('Hidden preparation ignored cancellation');},error=>error.name);
    await new Promise(resolve=>setTimeout(resolve,250));abort.abort();
    if(await cancelled!=='AbortError')throw Error('Hidden preparation did not cancel cleanly');
    const cancellation=capture();hide(false);
    hide(true,'timeout');let timeoutMessage='';
    try{await browserPlatformCallbacks({rendererPauseTimeoutMs:100}).prepareRenderer(module,{engine});}
    catch(error){timeoutMessage=error.message;}
    if(!/timed out.*Restore this game tab/.test(timeoutMessage))throw Error('Hidden preparation timeout was not actionable');
    const timeoutState=capture();hide(false);
    // Destroy the actual WebGPU device while the window is also hidden. It
    // must remain a hard failure, rather than entering the suspension retry.
    prepared.module.partyBoardRendererLost=()=>{visibilityProbe.deviceLost=true;};
    hide(true,'device-loss');visibilityProbeDevice.destroy();await visibilityProbeDevice.lost;
    const deadline=performance.now()+5000;
    while(!visibilityProbe.deviceLost&&performance.now()<deadline)await new Promise(resolve=>setTimeout(resolve,10));
    if(!visibilityProbe.deviceLost)throw Error('Actual device-loss callback was not observed');
    const failed=await prepared.module.ccall('PartyBoardBrowserAdvancePreparedRenderer','number',[],[],{async:true});
    hide(false);
    if(failed!==-2)throw Error('Lost WebGPU device must return -2 even while hidden: '+failed);
    if(engine.preparedCheckpoint()!==checkpoint||module._PartyBoard_GameFrameCount()!==0)throw Error('Prepared simulation changed');
    await prepared.dispose();return {ready,cancellation,timeoutState,timeoutMessage,lostDeviceResult:failed,disposedPhase:engine.phase};
  }).then(value=>({value}),error=>({error:String(error)}));
  const snapshots=[];
  for(const stage of ['initial','draining']){
    await page.waitForFunction(stage=>visibilityProbe.failure||visibilityProbe.waiting.includes(stage),stage,{timeout:240000});
    const failure=await page.evaluate(()=>visibilityProbe.failure);
    if(failure){
      assert.equal(mode,'baseline');assert.match(failure.message,/AdvancePreparedRenderer \(-2\)/);assert.equal(failure.state.frame,0);
      const result=await evaluation;assert.ok(result.error);assert.ok(logs.some(line=>/window is paused or hidden/.test(line)));
      fs.writeFileSync(path.join(output,'acceptance.json'),JSON.stringify({status:'VERIFIED baseline hidden-window preparation failure',mode,failure,errors,muted:true,physicalDevices:false},null,2));return;
    }
    assert.equal(mode,'fixed','Baseline unexpectedly accepted suspension');
    const before=await page.evaluate(()=>captureVisibilityProbe());await new Promise(resolve=>setTimeout(resolve,250));
    const after=await page.evaluate(()=>captureVisibilityProbe());assert.deepEqual(after,before);assert.equal(after.phase,'Idle');assert.equal(after.frame,0);
    snapshots.push({stage,before,after});
    await page.evaluate(()=>{visibilityProbe.hidden=false;document.dispatchEvent(new Event('visibilitychange'));});
  }
  const result=await evaluation;assert.equal(result.error,undefined,result.error);assert.equal(result.value.ready.frame,0);assert.equal(result.value.ready.pipelines.queued,0);
  assert.ok(result.value.ready.pipelines.created>=1345);assert.equal(result.value.lostDeviceResult,-2);assert.equal(result.value.disposedPhase,'Disposed');assert.deepEqual(errors,[]);
  for(const state of [result.value.cancellation,result.value.timeoutState])assert.deepEqual(state,{phase:'Idle',frame:0,checkpoint:result.value.ready.checkpoint});
  fs.writeFileSync(path.join(output,'acceptance.json'),JSON.stringify({status:'VERIFIED actual browser hidden/restored preparation and device-loss rejection',mode,snapshots,...result.value,errors,muted:true,physicalDevices:false,scope:'Production runtime preparation with controlled document visibility events through real SDL; no gameplay or physical two-PC acceptance'},null,2));
})().catch(error=>{fs.writeFileSync(path.join(output,'failure.json'),JSON.stringify({error:error.stack||String(error)},null,2));console.error(error);process.exitCode=1;})
.finally(async()=>{fs.writeFileSync(path.join(output,'browser.log'),logs.join('\n')+'\n');fs.writeFileSync(path.join(output,'page-errors.json'),JSON.stringify(errors,null,2));if(browser)await browser.close();server.kill();});
