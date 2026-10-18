const inputWallClock=process.env.PARTYBOARD_TEST_INPUT_WALL_CLOCK==='1';
const recoveryBuildId=process.env.PARTYBOARD_TEST_RECOVERY_BUILD_ID||'',recoveryAssetId=process.env.PARTYBOARD_TEST_RECOVERY_ASSET_ID||'';
const inputCrossLoss=process.env.PARTYBOARD_TEST_INPUT_CROSS_LOSS==='1';
const postInitTrace=process.env.PARTYBOARD_TEST_POST_INIT_TRACE=== '1';
const alignStartClock=process.env.PARTYBOARD_TEST_ALIGN_START_CLOCK=== '1';
const externalEngine=process.env.PARTYBOARD_TEST_EXTERNAL_ENGINE=== '1';
const externalStop=process.env.PARTYBOARD_TEST_EXTERNAL_ENGINE_STOP=== '1';
const inputCrossStimulusMode=process.env.PARTYBOARD_TEST_INPUT_CROSS_STIMULUS_MODE||'stimulus';
const inputCrossStimulus=process.env.PARTYBOARD_TEST_INPUT_CROSS_STIMULUS==='1';
const inputCrossRtc=process.env.PARTYBOARD_TEST_INPUT_CROSS_RTC==='1';
const inputCrossPolls=Number(process.env.PARTYBOARD_TEST_INPUT_CROSS_POLLS||1);
const inputCrossPeer=Number(process.env.PARTYBOARD_TEST_INPUT_CROSS_PEER||0);
const inputMultiLossTest=process.env.PARTYBOARD_TEST_INPUT_MULTI_LOSS==='1';
const inputSuspendTest=process.env.PARTYBOARD_TEST_INPUT_SUSPEND==='1';
const inputRtcResumeTest=process.env.PARTYBOARD_TEST_INPUT_RTC_RESUME==='1';
const inputRtcLossTest=process.env.PARTYBOARD_TEST_INPUT_RTC_LOSS==='1';
const inputRtcTest=process.env.PARTYBOARD_TEST_INPUT_RTC==='1';
const inputQueueTest=process.env.PARTYBOARD_TEST_INPUT_QUEUE==='1';
const inputCancelTest=process.env.PARTYBOARD_TEST_INPUT_CANCEL==='1';
const inputExchangeTest=process.env.PARTYBOARD_TEST_INPUT_EXCHANGE==='1';
const clientCommitTest=process.env.PARTYBOARD_TEST_CLIENT_COMMIT=== '1';
// Executes standalone Wasm runtime contracts, not the game or asset-transfer app.
const http=require('node:http');
const fs=require('node:fs');
const path=require('node:path');
const assert=require('node:assert/strict');
const {validateBoardMovement}=require('./board-state-validation.cjs');
const {validatePipelineSceneVisits}=require('./pipeline-scene-validation.cjs');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const browserLaunchOptions=()=>({
  ...(process.env.PARTYBOARD_TEST_BROWSER_EXECUTABLE
    ?{executablePath:process.env.PARTYBOARD_TEST_BROWSER_EXECUTABLE}
    :{channel:process.env.PARTYBOARD_TEST_BROWSER_CHANNEL||'msedge'}),
  headless:true,args:['--mute-audio'],
});
if(!process.argv[2])throw Error('Pass the compiled browser contract directory');
if(postInitTrace)assert.ok(externalEngine&&alignStartClock&&process.env.PARTYBOARD_TEST_INPUT_SCRIPT,'Post-init trace requires external aligned engine and controller stimulus');
if(alignStartClock)assert.ok(externalEngine,'Clock alignment probe requires external engine lifecycle mode');
if(clientCommitTest)assert.ok(externalEngine&&alignStartClock&&!postInitTrace,'Client commit probe requires aligned external engine without post-init trace');
if(externalStop)assert.ok(externalEngine,'Active stop requires external engine lifecycle mode');
if(inputCrossPeer)assert.ok(Number.isInteger(inputCrossPolls)&&inputCrossPolls>=1&&inputCrossPolls<=12000&&[11,22].includes(inputCrossPeer)&&process.send&&inputQueueTest&&!inputRtcTest,'Cross-engine probe requires IPC and queue mode');
if(inputMultiLossTest)assert.ok(inputSuspendTest,'Multiple loss probe requires suspended recovery');
if(inputSuspendTest)assert.ok(inputRtcResumeTest,'Suspension probe requires RTC resume');
if(inputRtcResumeTest)assert.ok(inputRtcLossTest,'RTC resume probe requires input loss');
if(inputRtcLossTest)assert.ok(inputRtcTest,'RTC loss probe requires RTC input');
if(inputRtcTest)assert.ok(inputQueueTest,'RTC input probe requires session queue');
if(inputQueueTest)assert.ok(inputCancelTest,'Session queue probe requires pending input cancellation');
if(inputCancelTest)assert.ok(inputExchangeTest&&externalStop,'Input cancellation probe requires exchange and active-stop probes');
if(externalEngine)assert.equal(process.argv[3],'watcherboot','External engine lifecycle probe requires watcherboot mode');
const inputScript=process.env.PARTYBOARD_TEST_INPUT_SCRIPT?Array.from(fs.readFileSync(process.env.PARTYBOARD_TEST_INPUT_SCRIPT)):null;
const watcherStart=process.argv[3]==='watcherstart';
const watcherTitle=watcherStart||process.argv[3]==='watchertitle';
const watcherProbe=watcherTitle||process.argv[3]==='watcherboot';
const mentSelect=process.env.PARTYBOARD_TEST_MENT_SELECT==='1';
const mentCpu=process.env.PARTYBOARD_TEST_MENT_CPU==='1';
const mentBoard=process.env.PARTYBOARD_TEST_MENT_BOARD==='1';
const boardChoose=process.env.PARTYBOARD_TEST_BOARD_CHOOSE==='1';
const boardRun=process.env.PARTYBOARD_TEST_BOARD_RUN==='1';
const boardPlay=process.env.PARTYBOARD_TEST_BOARD_PLAY==='1';
const boardMove=process.env.PARTYBOARD_TEST_BOARD_MOVE==='1';
const instRun=process.env.PARTYBOARD_TEST_INST_RUN==='1';
const minigameRun=process.env.PARTYBOARD_TEST_MINIGAME_RUN==='1';
const resultRun=process.env.PARTYBOARD_TEST_RESULT_RUN==='1';
const boardReturn=process.env.PARTYBOARD_TEST_BOARD_RETURN==='1';
const expectedBoard=Number(process.env.PARTYBOARD_TEST_BOARD_ID||4);
const expectedMinigame=Number(process.env.PARTYBOARD_TEST_MINIGAME_ID||419);
const forceM411=process.env.PARTYBOARD_TEST_FORCE_M411==='1';
assert.ok([410,411,419].includes(expectedMinigame),'Unsupported expected minigame');
assert.ok(!forceM411||(expectedMinigame===411&&minigameRun),'M411 override requires explicit M411 minigame diagnosis');
assert.ok(expectedMinigame!==411||forceM411,'M411 probe requires explicitly labeled diagnostic override');
assert.ok([1,2,3,4,5,6,10].includes(expectedBoard),'Invalid expected board');
if(boardReturn)assert.ok(resultRun,'Board return requires results execution');
if(resultRun)assert.ok(minigameRun,'Results probe requires minigame execution');
if(minigameRun)assert.ok(instRun && inputScript,'Minigame probe requires instruction setup and recorded stimulus');
if(instRun)assert.ok(boardMove,'Instruction probe requires board movement probe');
if(boardMove)assert.ok(boardPlay,'Movement probe requires board input probe');
if(boardPlay)assert.ok(boardRun,'Board input probe requires board execution');
if(boardRun)assert.ok(boardChoose,'Board execution requires board confirmation probe');
if(boardChoose)assert.ok(mentBoard,'Board confirmation requires board setup probe');
if(mentBoard)assert.ok(mentCpu,'Board setup requires CPU setup probe');
if(mentCpu)assert.ok(mentSelect && process.env.PARTYBOARD_TEST_MENT_INPUT==='1' && process.env.PARTYBOARD_TEST_MENT_FRAMES==='1','CPU setup requires selection, input and frame probes');
let sampleLimit=watcherStart?(boardReturn?26004:minigameRun?(resultRun?20004:19004):instRun?17004:boardMove?15004:boardPlay?12004:boardRun?9004:boardChoose?6204:mentBoard?5604:mentCpu?4804:mentSelect?3204:2004):watcherTitle?1004:8;
if(process.env.PARTYBOARD_TEST_ONLINE_MENU==='1'&&watcherStart)sampleLimit=2404;
if(inputCrossPeer)sampleLimit=Math.max(8,inputCrossPolls+8+(inputCrossLoss?64:0));
if(process.env.PARTYBOARD_TEST_SAMPLE_LIMIT!==undefined) {
  assert.ok((watcherStart||(externalEngine&&postInitTrace))&&inputScript,'Custom sample limits require recorded watcher input or post-init stimulus');
  const requested=Number(process.env.PARTYBOARD_TEST_SAMPLE_LIMIT);
  assert.ok(Number.isSafeInteger(requested)&&requested>=sampleLimit&&requested<=30004,'Custom sample limit must extend the mode within engine bounds');
  sampleLimit=requested;
}
if(inputScript) {
  assert.ok(watcherProbe,'Input scripts require a watcher probe');
  const bytes=Buffer.from(inputScript);
  const summary=require('./compare-input-traces.cjs').compareTraces(bytes,bytes);
  assert.equal(summary.leftSamples,sampleLimit,'Input script must match the bounded probe length');
  assert.equal(bytes.readUInt32LE(8),1,'Input script must use the browser probe seed');
}
if(watcherProbe)process.argv[3]='frameprobe';
const persistenceProbe=process.argv[3]==='savepersist';
const guestProbe=process.argv[3]==='guestreload';
if(persistenceProbe || guestProbe)process.argv[3]='savereload';
const directory=path.resolve(process.argv[2]);
const generatedCardPath=path.resolve(__dirname,'../../../outputs/validation/browser-generated-card.raw');
const guestHash=guestProbe?require('node:crypto').createHash('sha256').update(fs.readFileSync(generatedCardPath)).digest('hex'):null;
const server=http.createServer((req,res)=>{
  if(req.method!=='GET'){res.writeHead(404);res.end();return;}
  if(watcherStart && ['/glyph-texture.mjs','/control-glyphs.mjs'].includes(req.url)) {
    res.setHeader('Content-Type','text/javascript');res.end(fs.readFileSync(path.join(__dirname,'../web',req.url.slice(1))));return;
  }
  if(['savereload','menuslot'].includes(process.argv[3]) && ['/host-save-store.mjs','/runtime-save.mjs'].includes(req.url)) {
    res.setHeader('Content-Type','text/javascript');
    res.end(fs.readFileSync(path.join(__dirname,'../web',req.url.slice(1))));return;
  }
  if(process.argv[3]==='savereload' && req.url==='/probe-card.raw') {
    res.setHeader('Content-Type','application/octet-stream');
    fs.createReadStream(generatedCardPath).on('error',()=>{res.statusCode=500;res.end();}).pipe(res);return;
  }
  if(req.url==='/'){res.setHeader('Content-Type','text/html');res.end('<!doctype html><title>Coroutine contract test</title>');return;}
  if(['savereload','menuslot','menuchoose','menuframes','boottransition','bootstart','boottitle','bootassets','bootframes','bootschedule','bootsetup','frameprobe','startupgame','renderdisc','gamediscasync','assetasync','assetmount','assetworker','assetworkerlarge'].includes(process.argv[3]) && ['/start-channels.mjs','/start-preparation-channel.mjs','/start-commit-channel.mjs','/runtime-build.mjs','/online-input-gate.mjs','/diagnostic-input.mjs','/realtime-input-recovery.mjs','/suspended-input-recovery.mjs','/input-channels.mjs','/session-input-exchange.mjs','/recovery-poll-replay.mjs','/paused-state-agreement.mjs','/session-input.mjs','/lockstep.mjs','/input-packet.mjs','/browser-input-exchange.mjs','/recovery-commit.mjs','/session-recovery.mjs','/reconnect-policy.mjs','/start-client.mjs','/start-host.mjs','/start-barrier.mjs','/engine-lifecycle.mjs','/pipeline-drain.mjs','/async-blob-fs.mjs','/transfer.mjs','/runtime-asset.mjs','/verified-asset-file.mjs','/synthetic-disc-test.mjs'].includes(req.url)) {
    res.setHeader('Content-Type','text/javascript');
    fs.createReadStream(path.join(__dirname,'../web',req.url.slice(1))).pipe(res);return;
  }
  if(((process.argv[3]||'').startsWith('assetworker') || process.argv[3]==='gamedisc') && req.url==='/disc-worker.mjs') {
    res.setHeader('Content-Type','text/javascript');
    res.end(`import create from '/contract.mjs';
      onmessage=async ({data:blob})=>{
        const output=[],errors=[];
        try {
          await create({arguments:${JSON.stringify(process.argv[3]==='gamedisc'?['/assets/verified-disc.ciso','--inspect-game']:['/assets/verified-disc.ciso'])},print:x=>output.push(x),printErr:x=>errors.push(x),
            preRun:[m=>{m.FS.mkdir('/assets');m.FS.mount(m.WORKERFS,{blobs:[{name:'verified-disc.ciso',data:blob}]},'/assets');}]});
          postMessage({output,errors});
        } catch(error){postMessage({output,errors:[...errors,String(error)]});}
      };`);return;
  }
  const name=(watcherStart || ['modesellink','boottransition','menuframes','menuchoose','menuslot','savereload'].includes(process.argv[3])) && req.url==='/modeseldll.wasm' ? 'modeseldll.wasm' : (watcherStart || ['bootlink','bootsetup','bootschedule','bootframes','bootassets','boottitle','bootstart','boottransition','menuframes','menuchoose','menuslot','savereload'].includes(process.argv[3])) && req.url==='/bootDll.wasm' ? 'bootDll.wasm' : {'/contract.mjs':'contract.mjs','/contract.wasm':'contract.wasm'}[req.url];
  if(!name){console.error(`Unmapped browser probe resource: ${req.url} (mode ${process.argv[3]}, watcherStart ${watcherStart})`);res.writeHead(404);res.end();return;}
  res.setHeader('Content-Type',name.endsWith('.wasm')?'application/wasm':'text/javascript');
  const artifactPath=path.join(directory,name);
  fs.createReadStream(artifactPath).on('error',error=>{console.error(`Missing browser probe artifact: ${artifactPath}: ${error.message}`);res.statusCode=500;res.end();}).pipe(res);
});
(async()=>{
  await new Promise((resolve,reject)=>{server.once('error',reject);server.listen(0,'127.0.0.1',resolve);});
  let browser, evaluationTimer;
  try{
    if(inputCrossLoss){
      const {verifyDigest}=require('./file-digest.cjs');
      const {fingerprintRuntimeFiles}=await import(require('node:url').pathToFileURL(path.resolve(__dirname,'../web/runtime-build.mjs')).href);
      const measured=await fingerprintRuntimeFiles(['contract.mjs','contract.wasm'].map(name=>({name,blob:new Blob([fs.readFileSync(path.join(directory,name))])})));
      if(measured.buildId!==recoveryBuildId)throw Error('Local file digest mismatch: runtime package');
      await verifyDigest(process.env.PARTYBOARD_LOCAL_CISO,recoveryAssetId);
      console.log('Independent local recovery digests verified before browser launch.');
    }
    browser=await chromium.launch(browserLaunchOptions());
    const page=await browser.newPage(), errors=[];
    if(inputCrossPeer){
      await page.exposeFunction('partyBoardSendInput',packet=>process.send({type:'input',packet}));
      await page.exposeFunction('partyBoardSendSignal',message=>process.send(message));
      process.on('message',message=>{if(message.type==='signal')page.evaluate(message=>globalThis.partyBoardReceiveSignal(message.description),message).catch(error=>errors.push(error.message));});
      process.on('message',message=>{if(message.type==='input')page.evaluate(message=>{
        if(globalThis.partyBoardReceiveInput)globalThis.partyBoardReceiveInput(message);
        else (globalThis.partyBoardPendingInput??=[]).push(message);
      },message).catch(error=>errors.push(error.message));});
    }
    page.on('pageerror',e=>errors.push(e.stack || e.message));
    if(['savereload','menuslot','menuchoose','menuframes','boottransition','bootstart','boottitle','bootassets','bootframes','bootschedule','frameprobe','fiberlog'].includes(process.argv[3]))page.on('console',message=>console.log('Frame probe: '+message.text()));
    await page.goto(`http://127.0.0.1:${server.address().port}/`);
    await page.evaluate(()=>{
      globalThis.partySetupKeyEvents=[];
      document.addEventListener('keydown',event=>{
        if(event.code==='KeyZ')globalThis.partySetupKeyEvents.push({code:event.code,target:event.target?.id||event.target?.tagName||null});
      },true);
    });
    if(persistenceProbe) {
      await page.evaluate(async()=>{
        const {openHostSaveStore}=await import('/host-save-store.mjs');
        const store=await openHostSaveStore();
        const response=await fetch('/probe-card.raw');
        if(!response.ok)throw Error('Generated probe card is missing');
        const bytes=new Uint8Array(await response.arrayBuffer());
        // Identity of the explicitly selected Rev 1 disc used by this probe.
        await store.commit('543b93edb8fc1facd2559011bf1fba9f090ecb5fc7193769a1b241b31c178fa9',0,bytes,{expectedRevision:0,role:'host'});
        store.close();
      });
      await page.reload();
    }
    if(process.argv[3]==='savereload' || process.argv[3]==='menuslot' || process.argv[3]==='menuchoose' || process.argv[3]==='menuframes' || process.argv[3]==='boottransition' || process.argv[3]==='bootstart' || process.argv[3]==='boottitle' || process.argv[3]==='bootassets' || process.argv[3]==='bootframes' || process.argv[3]==='bootschedule' || process.argv[3]==='bootsetup' || process.argv[3]==='frameprobe' || process.argv[3]==='startupgame' || process.argv[3]==='gamedisc' || process.argv[3]==='gamediscasync') {
      if(!process.env.PARTYBOARD_LOCAL_CISO)throw Error('Set PARTYBOARD_LOCAL_CISO for explicit local disc inspection');
      await page.evaluate(()=>{const input=document.createElement('input');input.type='file';document.body.appendChild(input);});
      await page.locator('input[type=file]').setInputFiles(process.env.PARTYBOARD_LOCAL_CISO);
    }
    if(process.env.PARTYBOARD_TEST_ONLINE_MENU==='1') {
      assert.ok(process.argv[3]==='menuframes'||watcherStart,'Online menu probe requires actual menu rendering');
      await page.evaluate(()=>{globalThis.testOnlineMenu=true;});
    }
    if(process.env.PARTYBOARD_CAPTURE_PIPELINES==='1')await page.evaluate(()=>{globalThis.testCapturePipelines=true;});
    const evaluation=page.evaluate(async({mode,persistenceProbe,guestProbe,guestHash,watcherProbe,watcherTitle,watcherStart,mentFrames,mentSelect,mentCpu,sampleLimit,boardRun,instRun,inputScript,minigameRun,resultRun,boardReturn,expectedBoard,expectedMinigame,forceM411,modeEntry,traceState,externalEngine,externalStop,alignStartClock,postInitTrace,clientCommitTest,inputExchangeTest,inputCancelTest,inputQueueTest,inputRtcTest,inputRtcLossTest,inputRtcResumeTest,inputSuspendTest,inputWallClock,inputMultiLossTest,inputCrossPeer,inputCrossPolls,inputCrossRtc,inputCrossLoss,recoveryBuildId,recoveryAssetId,inputCrossStimulus,inputCrossStimulusMode})=>{
      globalThis.testTraceState=traceState;
      globalThis.testModeEntry=modeEntry;
      globalThis.testMentFrames=mentFrames;
      globalThis.testMentSelect=mentSelect;
      globalThis.testMentCpu=mentCpu;
      globalThis.testSampleLimit=sampleLimit;
      globalThis.testExpectedMinigame=expectedMinigame;globalThis.testForceM411=forceM411; globalThis.testExpectedBoard=expectedBoard; globalThis.testBoardReturn=boardReturn; globalThis.testResultRun=resultRun; globalThis.testMinigameRun=minigameRun; globalThis.testInputScript=!!inputScript; globalThis.testBoardRun=boardRun; globalThis.testInstRun=instRun;
      if(watcherStart) {
        const {glyphTexture,gxRgba8}=await import('/glyph-texture.mjs');
        globalThis.testKeyboardGlyphs=Object.fromEntries(await Promise.all(['Z','X','C','V','E','Q','R'].map(async label=>[label,gxRgba8(await glyphTexture({binding:{kind:'key',label}}))])));
      }
      const saveReload=mode==='savereload';
      let restoredCard=null;
      if(saveReload) {
        if(persistenceProbe) {
          const {openHostSaveStore}=await import('/host-save-store.mjs');
          const store=await openHostSaveStore();
          try {
            const saved=await store.load('543b93edb8fc1facd2559011bf1fba9f090ecb5fc7193769a1b241b31c178fa9',0);
            if(!saved || saved.revision!==1)throw Error('Persisted game card is missing');
            restoredCard=saved.bytes;
          } finally {store.close();}
        } else {
          const response=await fetch('/probe-card.raw');
          if(!response.ok)throw Error('Generated probe card is missing');
          restoredCard=new Uint8Array(await response.arrayBuffer());
        }
      }
      let guestSession;
      if(guestProbe) {
        const {prepareGuestSaveSession}=await import('/runtime-save.mjs');
        const assetId='543b93edb8fc1facd2559011bf1fba9f090ecb5fc7193769a1b241b31c178fa9';
        guestSession=await prepareGuestSaveSession({bytes:restoredCard,sha256:guestHash,assetId,
          expectedAssetId:assetId,slot:0,path:'/MemoryCardA.USA.raw'});
        restoredCard=null;
      }
      const menuSlot=mode==='menuslot';
      const menuChoose=menuSlot || mode==='menuchoose';
      const menuFrames=menuChoose || mode==='menuframes';
      const transition=menuFrames || mode==='boottransition';
      const bootStart=transition || mode==='bootstart';
      const bootTitle=bootStart || mode==='boottitle';
      const bootAssets=bootTitle || mode==='bootassets';
      const renderBoot=bootAssets || mode==='bootframes';
      const scheduleBoot=renderBoot || mode==='bootschedule';
      const frameProbe=mode==='frameprobe';
      const startupGame=saveReload || scheduleBoot || mode==='bootsetup' || mode==='startupgame' || frameProbe;
      const renderDisc=mode==='renderdisc' || startupGame;
      if(renderDisc)mode=startupGame?'gamediscasync':'assetasync';
      let create;
      if(inputCrossPeer){
        const {prepareFingerprintedRuntimeFactory}=await import('/runtime-build.mjs');
        const files=await Promise.all(['contract.mjs','contract.wasm'].map(async name=>{const response=await fetch('/'+name);if(!response.ok)throw Error('Runtime artifact fetch failed: '+name);return {name,blob:await response.blob()};}));
        const factory=await prepareFingerprintedRuntimeFactory(files);
        if(inputCrossLoss&&factory.buildId!==recoveryBuildId)throw Error('Local file digest mismatch: browser runtime package');
        create=options=>factory.createRuntime(options);
        console.log('Independent immutable runtime package: '+factory.buildId);
      }else({default:create}=await import('/contract.mjs'));
      const output=[], errors=[];
      let runtimeAborted=false;
      const canvas=document.createElement('canvas'); canvas.id='canvas'; canvas.width=640; canvas.height=480; document.body.appendChild(canvas);
      let mountBytes, verifiedBlob;
      if(mode==='assetasync' || mode==='assetmount' || mode.startsWith('assetworker')) {
        const {prepare,browserStore,CHUNK,digest}=await import('/transfer.mjs');
        const {verifiedAssetBlob}=await import('/runtime-asset.mjs');
        const {syntheticCiso,syntheticLargeCiso}=await import('/synthetic-disc-test.mjs');
        const asset=await prepare(mode==='assetworkerlarge'?syntheticLargeCiso():new Blob([syntheticCiso()])), store=await browserStore();
        try {
          for(let i=0;i<asset.manifest.count;i++) {
            const bytes=await asset.file.slice(i*CHUNK,(i+1)*CHUNK).arrayBuffer();
            await store.put(asset.manifest.id,i,{bytes,hash:await digest(bytes)});
          }
          await store.complete(asset.manifest);
          const verified=await verifiedAssetBlob(store,asset.manifest.id);
          verifiedBlob=verified.blob;
          if(mode==='assetmount')mountBytes=new Uint8Array(await verifiedBlob.arrayBuffer());
        } finally {store.close();}
      }
      if(mode==='gamedisc' || mode==='gamediscasync')verifiedBlob=document.querySelector('input[type=file]').files[0];
      if(mode.startsWith('assetworker') || mode==='gamedisc') {
        const worker=new Worker('/disc-worker.mjs',{type:'module'});
        try {
          return await new Promise((resolve,reject)=>{
            const timer=setTimeout(()=>reject(Error('Disc worker timeout')),30000);
            worker.onmessage=event=>{clearTimeout(timer);resolve(event.data);};
            worker.onerror=event=>{clearTimeout(timer);reject(Error(event.message));};
            worker.postMessage(verifiedBlob);
          });
        } finally {worker.terminate();}
      }
      let asyncStats, mountedModule;
      let saveAdapter, saveStore, saveNotifications=0;
      const saveFailures=[];
      if(menuSlot) {
        const {openHostSaveStore}=await import('/host-save-store.mjs');
        const {prepareHostSaveSession}=await import('/runtime-save.mjs');
        saveStore=await openHostSaveStore();
        saveAdapter=await prepareHostSaveSession({store:saveStore,
          assetId:'543b93edb8fc1facd2559011bf1fba9f090ecb5fc7193769a1b241b31c178fa9',slot:0,
          path:'/MemoryCardA.USA.raw',role:'host'});
      }
      const asyncMount = (mode==='assetasync' || mode==='gamediscasync') ? (await import('/async-blob-fs.mjs')).mountAsyncBlob : null;
      globalThis.testExternalEngine=externalEngine;
      globalThis.testPostInitTrace=postInitTrace;
      try {
        await create({canvas,arguments:mode==='trace'?['/trace-test']:asyncMount?(mode==='gamediscasync'?['/assets/verified-disc.ciso',startupGame?'--startup-game':'--inspect-game',...(saveReload?['--reload-save']:frameProbe?[watcherStart?'--watcher-start':watcherTitle?'--watcher-title':watcherProbe?'--watcher':'--frames']:menuSlot?['--menu-slot']:menuChoose?['--menu-choose']:menuFrames?['--menu-frames']:transition?['--boot-transition']:bootStart?['--boot-start']:bootTitle?['--render-boot-title']:bootAssets?['--render-boot-assets']:renderBoot?['--render-boot']:scheduleBoot?['--schedule']:[])]:['/assets/verified-disc.ciso']):[],print:line=>{output.push(line);if(line.startsWith('Online Multiplayer'))(globalThis.onlineMenuMessages ||= []).push(line);if(scheduleBoot || frameProbe || mode==='fiberlog')console.log(line);},printErr:line=>{errors.push(line);if(scheduleBoot || frameProbe || mode==='fiberlog')console.log(line);},
          preRun:asyncMount?[module=>{mountedModule=module;globalThis.onlineMenuRuntime=module;if(globalThis.testOnlineMenu&&typeof module._HuWinMesSetPtr!=='function')throw Error('Online menu window text export missing');if(inputScript)module.FS.writeFile('/stimulus.pbtrace',new Uint8Array(inputScript));if(restoredCard)module.FS.writeFile('/MemoryCardA.USA.raw',restoredCard);if(guestSession)guestSession.preRun(module);if(saveAdapter)saveAdapter.preRun(module);asyncStats=asyncMount(module,verifiedBlob);}]:mode==='assetmount'?[module=>module.FS.writeFile('/verified-disc.ciso',mountBytes)]:[],
          onCardSaveCompleted:slot=>{
            if(guestSession)guestSession.onCardSaveCompleted(slot);
            if(!saveAdapter)return;
            if(slot!==0){saveFailures.push('Unexpected card slot');return;}
            saveNotifications++;
            saveAdapter.onCardSaveCompleted(slot);
          },
          onAbort:reason=>{runtimeAborted=true;throw Error(String(reason));}});
      } catch (error) {
        throw Error(JSON.stringify({output, errors, failure: error.stack || String(error)}));
      }
      if(externalEngine) {
        const until=performance.now()+35000;
        while(!output.includes('External engine platform preparation passed.') && performance.now()<until)
          await new Promise(resolve=>setTimeout(resolve,20));
        if(!output.includes('External engine platform preparation passed.'))throw Error('Platform preparation did not finish');
        mountedModule._PartyBoardBrowserRequireOnlineInput(1);
        if(mountedModule._PartyBoardBrowserOnlineInputRequired()!==1)throw Error('Online input gate export failed');
        mountedModule._PartyBoardBrowserRequireOnlineInput(0);
        if(mountedModule._PartyBoardBrowserOnlineInputRequired()!==0)throw Error('Local input mode restore failed');
        const {createEngineLifecycle,prepareEngineLifecycle}=await import('/engine-lifecycle.mjs');
        let runSettled=false;
        const lifecycleOptions={sessionSeed:1,shutdown:async module=>{
          if(!runSettled)throw Error("Cleanup before run completion");
          module._PartyBoardTestFinishExternalEngine();
        }};
        let engine;
        if(alignStartClock && mountedModule._PartyBoard_GameAlignStartClock()!==-1)throw Error("Premature clock alignment accepted");
        if(alignStartClock&&!postInitTrace) {
          const prepared=await prepareEngineLifecycle(mountedModule,lifecycleOptions);
          engine=prepared.engine;
          if(mountedModule._PartyBoardTestExternalFrames()!==0)throw Error('Preparation advanced simulation');
          if(prepared.checkpoint!==engine.preparedCheckpoint())throw Error('Prepared runtime checkpoint differs');
          console.log(`Real engine preparation helper passed: paused checkpoint ${prepared.checkpoint}.`);
        } else {
          engine=createEngineLifecycle(mountedModule,lifecycleOptions);
          await engine.initialize();
        }
        if(mountedModule._PartyBoard_GameConfigureSessionSeed(2)!==-1)throw Error('Initialized engine accepted a session seed');
        if(postInitTrace)mountedModule._PartyBoardTestBeginSessionTrace();
        if(alignStartClock) {
          if(postInitTrace)engine.alignStartClock();
          const checkpoint=engine.preparedCheckpoint();
          if(engine.preparedCheckpoint()!==checkpoint)throw Error('Prepared checkpoint changed while idle');
          console.log('Actual prepared selected-state checkpoint:',checkpoint);
          if(mountedModule._PartyBoard_GameAlignStartClock()!==-1)throw Error("Duplicate clock alignment accepted");
        }
        if(engine.phase!=='Idle')throw Error('Engine did not prepare idle');
        if(typeof mountedModule._PartyBoardTestPipelineCount==='function')
          console.log('Actual prepared pipeline counters:',JSON.stringify({queued:mountedModule._PartyBoardTestPipelineCount(0),created:mountedModule._PartyBoardTestPipelineCount(1)}));
        if(alignStartClock&&typeof mountedModule._PartyBoardTestAdvancePreparedRenderer==='function') {
          const checkpoint=engine.preparedCheckpoint();
          if(await mountedModule.ccall('PartyBoardTestAdvancePreparedRenderer','number',[],[],{async:true})!==0)
            throw Error('Prepared renderer-only step failed');
          if(engine.phase!=='Idle'||mountedModule._PartyBoardTestExternalFrames()!==0||engine.preparedCheckpoint()!==checkpoint)
            throw Error('Renderer-only step changed prepared simulation');
          console.log('Actual renderer-only step preserved frame zero and prepared checkpoint.');
          const {drainIdleEnginePipelines}=await import('/pipeline-drain.mjs');
          const receipt=await drainIdleEnginePipelines(engine,{
            read:()=>({queued:mountedModule._PartyBoardTestPipelineCount(0),created:mountedModule._PartyBoardTestPipelineCount(1)}),
            advance:async()=>{if(await mountedModule.ccall('PartyBoardTestAdvancePreparedRenderer','number',[],[],{async:true})!==0)throw Error('Pipeline renderer step rejected');},maxSteps:60});
          console.log('Actual idle-engine discovered pipeline drain:',JSON.stringify(receipt));
        }
        let recoveryChannel,recoveryRemoteChannel,crossStartChannel;
        const recoveryConnections=[];
        const createRecoveryPair=async()=>{
          const hostConnection=new RTCPeerConnection({iceServers:[]});
          const clientConnection=new RTCPeerConnection({iceServers:[]});
          recoveryConnections.push(hostConnection,clientConnection);
          const received=new Promise(resolve=>clientConnection.ondatachannel=event=>resolve(event.channel));
          const hostChannel=hostConnection.createDataChannel('actual-engine-recovery',{ordered:true});
          const gather=async pc=>{
            if(pc.iceGatheringState!=='complete')await new Promise(resolve=>{
              const changed=()=>{if(pc.iceGatheringState==='complete'){pc.removeEventListener('icegatheringstatechange',changed);resolve();}};
              pc.addEventListener('icegatheringstatechange',changed);changed();
            });return {type:pc.localDescription.type,sdp:pc.localDescription.sdp};
          };
          await hostConnection.setLocalDescription(await hostConnection.createOffer());
          await clientConnection.setRemoteDescription(await gather(hostConnection));
          await clientConnection.setLocalDescription(await clientConnection.createAnswer());
          await hostConnection.setRemoteDescription(await gather(clientConnection));
          const clientChannel=await received;
          if(clientChannel.readyState!=='open')await new Promise(resolve=>clientChannel.addEventListener('open',resolve,{once:true}));
          if(hostChannel.readyState!=='open')await new Promise(resolve=>hostChannel.addEventListener('open',resolve,{once:true}));
          return {clientChannel,hostChannel};
        };
        let exchangedSamples=0,exchangeOwner,inputQueue,inputChannels;
        let inputTime=0;const inputNow=()=>(inputSuspendTest||inputCrossLoss)&&!inputWallClock?inputTime:performance.now();
        const inputCheckpoints=[],inputPairs=[];let inputTransportError,inputBridge,replaceInputPeer,replyThrough=0n;
        if(inputExchangeTest) {
          const {installBrowserInputExchange}=await import('/browser-input-exchange.mjs');
          if(inputQueueTest) {
            const {SessionInput}=await import('/session-input.mjs');
            const {createSessionInputExchange}=await import('/session-input-exchange.mjs');
            const {encodeInput,decodeInput}=await import('/input-packet.mjs');
            const session=new Uint8Array(16).fill(1),owners=inputCrossPeer?(['fourhuman','characters','board','boardturn','fourstimulus'].includes(inputCrossStimulusMode)?[11,22,11,22]:[11,22,0,0]):[11,22,33,44];
            inputQueue=new SessionInput(session,owners);
            let bridge,crossChannel;
            if(inputCrossRtc){
              const {InputChannels}=await import('/input-channels.mjs');
              let replacementPC;let pc=new RTCPeerConnection({iceServers:[]});recoveryConnections.push(pc);
              let acceptStart;const startReceived=new Promise(resolve=>{acceptStart=resolve;});
              const received=new Promise(resolve=>pc.ondatachannel=event=>{if(event.channel.label==='actual-cross-engine-start')acceptStart(event.channel);else resolve(event.channel);});
              const gather=async()=>{if(pc.iceGatheringState!=='complete')await new Promise(resolve=>{const changed=()=>{if(pc.iceGatheringState==='complete'){pc.removeEventListener('icegatheringstatechange',changed);resolve();}};pc.addEventListener('icegatheringstatechange',changed);changed();});return {type:pc.localDescription.type,sdp:pc.localDescription.sdp};};
              globalThis.partyBoardReceiveSignal=async description=>{
                if(description.replacement)ensureReplacementPC();
                await pc.setRemoteDescription(description);
                if(description.type==='offer'){await pc.setLocalDescription(await pc.createAnswer());await globalThis.partyBoardSendSignal({type:'signal',description:{...await gather(),...(replacementPC?{replacement:true}:{})}});}
              };
              await globalThis.partyBoardSendSignal({type:'ready'});
              if(inputCrossPeer===11){if(inputCrossLoss)crossStartChannel=pc.createDataChannel('actual-cross-engine-start',{ordered:true});crossChannel=pc.createDataChannel('actual-cross-engine-input',{ordered:true});await pc.setLocalDescription(await pc.createOffer());await globalThis.partyBoardSendSignal({type:'signal',description:{...await gather(),...(replacementPC?{replacement:true}:{})}});}
              else {crossChannel=await received;if(inputCrossLoss)crossStartChannel=await startReceived;}
              if(crossChannel.readyState!=='open')await new Promise(resolve=>crossChannel.addEventListener('open',resolve,{once:true}));
              inputChannels=new InputChannels(inputQueue,{now:inputNow,received:()=>bridge?.notify(),changed:()=>bridge?.notify(),error:(_peer,error)=>{inputTransportError=error;bridge?.notify();}});
              inputChannels.attach(inputCrossPeer===11?22:11,crossChannel);globalThis.partyBoardCloseCrossInput=()=>crossChannel.close();
              let acceptReplacement;
                            let acceptState;
              const stateReceived=new Promise(resolve=>{acceptState=resolve;});
              const replacementReceived=new Promise(resolve=>{acceptReplacement=resolve;});pc.ondatachannel=event=>{if(event.channel.label==='actual-cross-recovery-state')acceptState(event.channel);else acceptReplacement(event.channel);};
              globalThis.partyBoardCompareCrossRecoveryState=async state=>{
                const channel=inputCrossPeer===11?pc.createDataChannel('actual-cross-recovery-state',{ordered:true}):await stateReceived;
                const envelope={version:1,identity:{session:Array.from(session),owners:owners.slice(),seed:1,profile:'unlocked-no-save-v1',buildId:recoveryBuildId,assetId:recoveryAssetId},state};
                const serialized=JSON.stringify(envelope,(_key,value)=>typeof value==='bigint'?value.toString():value);
                let resolveState,rejectState;const receivedState=new Promise((resolve,reject)=>{resolveState=resolve;rejectState=reject;});
                                let stateSeen=false,ackSeen=false;const complete=()=>{if(stateSeen&&ackSeen)resolveState();};
                channel.addEventListener('message',event=>{if(event.data==='ACK'){ackSeen=true;complete();}else if(event.data===serialized){stateSeen=true;channel.send('ACK');complete();}else rejectState(Error('Independent engine recovery state differs'));});
                if(channel.readyState!=='open')await new Promise(resolve=>channel.addEventListener('open',resolve,{once:true}));
                channel.send(serialized);const retry=setInterval(()=>{if(channel.readyState==='open'){channel.send(serialized);if(stateSeen)channel.send('ACK');}},20);
                const timeout=setTimeout(()=>rejectState(Error('Recovery state exchange timed out')),5000);
                try{await receivedState;}finally{clearInterval(retry);clearTimeout(timeout);}
              };
              
              const ensureReplacementPC=()=>{
                if(replacementPC)return;
                pc.close();pc=new RTCPeerConnection({iceServers:[]});replacementPC=pc;recoveryConnections.push(pc);
                pc.ondatachannel=event=>{if(event.channel.label==='actual-cross-recovery-state')acceptState(event.channel);else acceptReplacement(event.channel);};
              };
              globalThis.partyBoardReplaceCrossInput=async()=>{
                                ensureReplacementPC();
                if(inputCrossPeer===11){crossChannel=pc.createDataChannel('actual-cross-engine-replacement',{ordered:true});await pc.setLocalDescription(await pc.createOffer());await globalThis.partyBoardSendSignal({type:'signal',description:{...await gather(),replacement:true}});}
                else crossChannel=await replacementReceived;
                if(crossChannel.readyState!=='open')await new Promise(resolve=>crossChannel.addEventListener('open',resolve,{once:true}));
                inputChannels.attach(inputCrossPeer===11?22:11,crossChannel);
              };
            }
            if(inputRtcTest) {
              const {InputChannels}=await import('/input-channels.mjs');
              inputChannels=new InputChannels(inputQueue,{now:inputNow,received:()=>bridge?.notify(),changed:()=>bridge?.notify(),error:(_peer,error)=>{inputTransportError=error;bridge?.notify();}});
              const attachFixture=(pair,slot,peer)=>{
                pair.hostChannel.binaryType='arraybuffer';
                pair.hostChannel.addEventListener('message',event=>{
                  const sample=decodeInput(new Uint8Array(event.data));
                  if(!sample||sample.slot!==0){inputTransportError=Error('Invalid local RTC sample');return;}
                  if(sample.frame<=replyThrough){const pad=new Uint8Array(11);pad[10]=255;
                    pair.hostChannel.send(encodeInput({session,frame:sample.frame,checkpoint:sample.checkpoint,slot,pad}));}
                });
                inputChannels.attach(peer,pair.clientChannel);
              };
              for(const [slot,peer] of [[1,22],[2,33],[3,44]]) {
                const pair=await createRecoveryPair();inputPairs.push(pair);attachFixture(pair,slot,peer);
              }
              replaceInputPeer=async(index=0)=>{const pair=await createRecoveryPair();inputPairs[index]=pair;attachFixture(pair,index+1,[22,33,44][index]);};
            }
            bridge=createSessionInputExchange(inputQueue,{session,owners,peer:inputCrossPeer||11,now:inputNow,suspendOnPause:inputSuspendTest||inputCrossLoss,checkpoint:options=>options.checkpoint,send:packet=>{
              if(inputCrossPeer){if(inputCrossRtc&&decodeInput(packet).frame<BigInt(inputCrossPolls+(globalThis.partyBoardCrossResumeOne?64:0)))crossChannel.send(packet);globalThis.partyBoardSendInput(Array.from(packet)).catch(error=>{inputTransportError=error;inputQueue.cancel();bridge.notify();});return;}
              if(inputRtcTest){for(const pair of inputPairs)pair.clientChannel.send(packet);return;}
              const sample=decodeInput(packet);
              // Diagnostic remote peers reply to the first poll only. Their
              // checkpoint and pads are fixtures, not independent game state.
              if(sample.frame===0n)setTimeout(()=>{
                for(const [slot,peer] of [[1,22],[2,33],[3,44]]) {
                  const pad=new Uint8Array(11);pad[10]=255;
                  if(inputQueue.receive(peer,encodeInput({session,frame:sample.frame,checkpoint:sample.checkpoint,slot,pad}),performance.now())!=='Accepted')
                    throw Error('Diagnostic peer input rejected');
                }
                bridge.notify();
              },5);
            }});
            if(inputCrossPeer){
              globalThis.partyBoardReceiveInput=message=>{
                const result=inputQueue.receive(message.peer,new Uint8Array(message.packet),inputNow());
                if(!['Accepted','Duplicate'].includes(result)){inputTransportError=Error('Cross-engine input rejected: '+result);inputQueue.cancel();}
                bridge.notify();
              };
              for(const message of globalThis.partyBoardPendingInput??[])globalThis.partyBoardReceiveInput(message);
              globalThis.partyBoardPendingInput=[];
            }
            const {diagnosticPad}=await import('/diagnostic-input.mjs');
            inputBridge=bridge;
            exchangeOwner=installBrowserInputExchange(mountedModule,(pads,options)=>{if(!Number.isInteger(options.checkpoint)||options.checkpoint<0||options.checkpoint>0xffffffff)throw Error('Missing actual PAD boundary checkpoint');inputCheckpoints.push(options.checkpoint);console.log('Actual PAD checkpoint:',exchangedSamples,options.checkpoint);console.log('Actual PAD boundary:',JSON.stringify({poll:exchangedSamples,checkpoint:options.checkpoint,frand:options.frand,rand8:options.rand8}));if(![options.frand,options.rand8].every(value=>Number.isInteger(value)&&value>=0&&value<=0xffffffff))throw Error('Missing actual PAD RNG states');const poll=exchangedSamples++;
              if(inputCrossStimulus)owners.forEach((owner,slot)=>{if(owner===inputCrossPeer)pads.set(diagnosticPad(inputCrossPeer,poll,inputCrossStimulusMode,slot),slot*11);});
              return bridge.exchange(pads,options).then(merged=>{if(inputCrossPeer&&merged)console.log('Actual merged PAD:',JSON.stringify({poll,pads:Array.from(merged)}));return merged;});});
          } else {
            exchangeOwner=installBrowserInputExchange(mountedModule,async pads=>{
              if(!(pads instanceof Uint8Array)||pads.length!==44)throw Error('Browser exchange pad encoding differs');
              exchangedSamples++;
              if(inputCancelTest&&exchangedSamples===2)return new Promise(()=>{});
              await new Promise(resolve=>setTimeout(resolve,1));return pads;
            });
          }
        }
        if(externalStop) {
          const pair=await createRecoveryPair();recoveryChannel=pair.clientChannel;recoveryRemoteChannel=pair.hostChannel;
          if(mountedModule._PartyBoardTestExternalFrames()!==0)throw Error('RTC preparation advanced actual engine');
        }
        let execution;
        if(inputCrossRtc&&inputCrossLoss) {
          const {StartBarrier}=await import('/start-barrier.mjs');
          const {StartChannels}=await import('/start-channels.mjs');
          const {prepareStartAcknowledgment}=await import('/start-client.mjs');
          const {resumeAgreedHostRuntime}=await import('/start-host.mjs');
          const {bindClientStartPreparation}=await import('/start-preparation-channel.mjs');
          const snapshot={revision:1,membership:1,total:verifiedBlob.size,assetId:recoveryAssetId,
            players:['engine11','engine22'].map((peer,slot)=>({peer,slot,bytes:verifiedBlob.size,verified:true,shaders:true,ready:true}))};
          const runtime={engine,module:mountedModule,inputExchangeInstalled:true,sessionSeed:1,
            checkpoint:engine.preparedCheckpoint(),buildId:recoveryBuildId,assetId:recoveryAssetId,dispose:()=>engine.dispose()};
          const options={peer:'engine'+inputCrossPeer,buildId:recoveryBuildId,snapshot:()=>snapshot,prepare:async()=>runtime,requireOnlineInput:true};
          if(crossStartChannel.readyState!=='open')await new Promise(resolve=>crossStartChannel.addEventListener('open',resolve,{once:true}));
          const waitFor=async predicate=>{const deadline=performance.now()+10000;while(!predicate()){if(inputTransportError)throw inputTransportError;if(performance.now()>deadline)throw Error('Independent engine start agreement timed out');await new Promise(resolve=>setTimeout(resolve,1));}};
          if(inputCrossPeer===11) {
            let remoteBound=false;
            const ready=event=>{if(event.data==='start-listener-ready')crossStartChannel.send('start-listener-ready-ack');else if(event.data==='start-bound')remoteBound=true;};
            crossStartChannel.addEventListener('message',ready);
            const barrier=new StartBarrier(snapshot,{session:new Uint8Array(16).fill(1),seed:1,buildId:recoveryBuildId,checkpoint:runtime.checkpoint});
            const channels=new StartChannels(barrier,{snapshot:()=>snapshot,requireOnlineInput:true,error:(_peer,cause)=>{inputTransportError=cause;}});
            const local=await prepareStartAcknowledgment(barrier.proposal,options);
            if(!channels.acknowledgeLocal('engine11',local.acknowledgment))throw Error('Actual host acknowledgment rejected');
            let blocked=false;try{channels.prepareRuntimeCommit('engine11',runtime);}catch{blocked=true;}
            if(!blocked||engine.phase!=='Idle'||mountedModule._PartyBoardTestExternalFrames()!==0)throw Error('Host advanced before independent client acknowledgment');
            await waitFor(()=>remoteBound);crossStartChannel.removeEventListener('message',ready);
            channels.attach('engine22',crossStartChannel);
            await waitFor(()=>barrier.state==='Agreed');
            channels.sendRuntimeCommit('engine11',runtime);
            channels.handoff('engine11',runtime);
            execution=resumeAgreedHostRuntime(barrier,runtime,()=>snapshot,{requireOnlineInput:true}).then(()=>{runSettled=true;});
          } else {
            let receiver;
            if(engine.phase!=='Idle'||mountedModule._PartyBoardTestExternalFrames()!==0)throw Error('Client advanced before host proposal');
            const ready=event=>{if(event.data!=='start-listener-ready-ack'||receiver)return;clearInterval(retry);crossStartChannel.removeEventListener('message',ready);receiver=bindClientStartPreparation(crossStartChannel,options,{error:cause=>{inputTransportError=cause;}});crossStartChannel.send('start-bound');};
            crossStartChannel.addEventListener('message',ready);
            const retry=setInterval(()=>{if(crossStartChannel.readyState==='open')crossStartChannel.send('start-listener-ready');},100);
            crossStartChannel.send('start-listener-ready');
            try{await waitFor(()=>receiver?.committed);}finally{clearInterval(retry);crossStartChannel.removeEventListener('message',ready);}
            execution=receiver.handoff().execution.then(()=>{runSettled=true;});
          }
          console.log('Independent actual engine start agreement passed: real RTC proposal/ack/commit and required online input.');
        } else if(alignStartClock&&!postInitTrace) {
          const {StartBarrier}=await import('/start-barrier.mjs');
          const {resumeAgreedHostRuntime}=await import('/start-host.mjs');
          const checkpoint=engine.preparedCheckpoint();
          const snapshot={revision:1,membership:1,total:100,assetId:'a'.repeat(64),players:
            ['host','guest1','guest2','guest3'].map((peer,slot)=>({peer,slot,bytes:100,verified:true,shaders:true,ready:true}))};
          const barrier=new StartBarrier(snapshot,{session:new Uint8Array(16).fill(1),seed:1,buildId:'b'.repeat(64),checkpoint});
          const runtime={engine,sessionSeed:1,checkpoint};
          for(const peer of ['host','guest1','guest2'])
            barrier.acknowledge(peer,{...barrier.proposal,type:'session-start-ack'},snapshot);
          let blocked=false;
          try {resumeAgreedHostRuntime(barrier,runtime,()=>snapshot);}catch {blocked=true;}
          if(!blocked||engine.phase!=='Idle'||mountedModule._PartyBoardTestExternalFrames()!==0)
            throw Error('Incomplete agreement advanced actual engine');
          barrier.acknowledge('guest3',{...barrier.proposal,type:'session-start-ack'},snapshot);
          if(clientCommitTest) {
            const {resumeCommittedClientRuntime}=await import('/start-client.mjs');
            const prepared={runtime,acknowledgment:{...barrier.proposal,type:'session-start-ack'}};
            const commit={...barrier.proposal,type:'session-start-commit'};
            const options={peer:'guest1',buildId:commit.buildId,snapshot:()=>snapshot};
            let rejected=false;
            try {resumeCommittedClientRuntime({...commit,seed:2},prepared,options);}catch {rejected=true;}
            if(!rejected||engine.phase!=='Idle'||mountedModule._PartyBoardTestExternalFrames()!==0)
              throw Error('Mismatched client commit advanced actual engine');
            execution=resumeCommittedClientRuntime(commit,prepared,options).then(()=>{runSettled=true;});
            console.log('Real client commit gate passed: mismatch stayed paused; matching commit resumed.');
          } else {
            execution=resumeAgreedHostRuntime(barrier,runtime,()=>snapshot).then(()=>{runSettled=true;});
            console.log('Real host gate passed: four acknowledgments required before engine resume.');
          }
        } else execution=engine.resume().then(()=>{runSettled=true;});
        if(inputCancelTest) {
          const deadline=performance.now()+Math.max(5000,inputCrossPolls*40+10000);
          while(exchangedSamples<(inputCrossPeer?inputCrossPolls+1:2)&&!runSettled&&performance.now()<deadline)
            await new Promise(resolve=>setTimeout(resolve,1));
          if(exchangedSamples!==(inputCrossPeer?inputCrossPolls+1:2)||engine.phase!=='Running'||runSettled)throw Error('Engine never waited for pending input');
          if(inputCrossPeer)console.log('Cross-engine suspended PAD:',JSON.stringify(inputBridge.pendingState(),(_key,value)=>typeof value==='bigint'?value.toString():value));
          if(inputQueueTest) {
            if(inputTransportError)throw inputTransportError;
            if(inputQueue.status(inputNow()).frame!==BigInt(inputCrossPeer?inputCrossPolls:1))throw Error('Missing peer input advanced PAD queue');
            const frozenFrame=mountedModule._PartyBoardTestExternalFrames();
            await new Promise(resolve=>setTimeout(resolve,30));
            if(mountedModule._PartyBoardTestExternalFrames()!==frozenFrame||runSettled)throw Error('Missing peer input advanced engine');
          }
          if(inputCrossLoss){
            const pendingBefore=inputBridge.pendingState(),frozen=mountedModule._PartyBoardTestExternalFrames();
            globalThis.partyBoardCloseCrossInput();const lossDeadline=performance.now()+5000;
            while(inputQueue.status(inputNow()).state!=='Paused'&&performance.now()<lossDeadline)await new Promise(resolve=>setTimeout(resolve,1));
            await new Promise(resolve=>setTimeout(resolve,30));
            if(inputQueue.status(inputNow()).state!=='Paused'||runSettled||mountedModule._PartyBoardTestExternalFrames()!==frozen||JSON.stringify(inputBridge.pendingState(),(_k,v)=>typeof v==='bigint'?v.toString():v)!==JSON.stringify(pendingBefore,(_k,v)=>typeof v==='bigint'?v.toString():v))throw Error('Cross-engine loss failed to preserve suspended execution');
            console.log('Actual cross-engine RTC loss passed: same pending PAD retained; execution frozen.');
            await globalThis.partyBoardReplaceCrossInput();
            if(!inputChannels.ready(inputCrossPeer===11?22:11)||inputQueue.status(inputNow()).state!=='Paused'||runSettled||mountedModule._PartyBoardTestExternalFrames()!==frozen)throw Error('Cross-engine replacement advanced unverified engine');
            console.log('Actual cross-engine RTC replacement passed: new open channel; engine remains suspended pending verification.');
            await globalThis.partyBoardCompareCrossRecoveryState(inputBridge.pendingState());
            if(inputQueue.status(inputNow()).state!=='Paused'||runSettled||mountedModule._PartyBoardTestExternalFrames()!==frozen)throw Error('Recovery state exchange advanced engine');
            console.log('Actual cross-engine RTC recovery state passed: independent pending poll/checkpoint/RNG matched over RTC; still suspended.');
            console.log('Actual cross-engine recovery identity passed: fixture session/owners/seed/profile/build/asset identity matched.');
            globalThis.partyBoardCrossResumeOne=true;
            const token=inputQueue.status(inputNow());
            if(!inputQueue.recovered(inputCrossPeer===11?22:11,token,inputNow())||!inputQueue.resume(token,inputNow()))throw Error('Cross-engine verified resume rejected');
            const pacingStarted=performance.now();
            inputBridge.resend();const resumeDeadline=performance.now()+15000;
            while(inputBridge.pendingState()?.frame!==BigInt(inputCrossPolls+64)&&!runSettled&&performance.now()<resumeDeadline)await new Promise(resolve=>setTimeout(resolve,1));
            if(runSettled||inputQueue.status(inputNow()).frame!==BigInt(inputCrossPolls+64)||inputBridge.pendingState()?.frame!==BigInt(inputCrossPolls+64))throw Error('Cross-engine resumed execution did not advance 64 polls: '+JSON.stringify({settled:runSettled,phase:engine.phase,queue:inputQueue.status(inputNow()),pending:inputBridge.pendingState()},(_key,value)=>typeof value==='bigint'?value.toString():value));
            const pacingElapsed=performance.now()-pacingStarted;
            if(pacingElapsed<62*1000/60)throw Error('Resumed actual engine exceeded normal 60Hz pacing: '+pacingElapsed);
            console.log('Actual resumed engine pacing:',JSON.stringify({polls:64,elapsedMs:pacingElapsed,minimumMs:62*1000/60,mode:'normal-browser'}));
            console.log('Actual cross-engine RTC resume passed: original suspended callback completed; 64 merged polls advanced without restart.');
            globalThis.partyBoardCloseCrossInput();
            while(inputQueue.status(inputNow()).state!=='Paused'&&performance.now()<resumeDeadline)await new Promise(resolve=>setTimeout(resolve,1));
            const resumedFrozen=mountedModule._PartyBoardTestExternalFrames();
            const {createRealtimeInputRecovery}=await import('/realtime-input-recovery.mjs');let menus=0;
            const recovery=createRealtimeInputRecovery(inputQueue,inputBridge,{execution,channels:inputChannels,intervalMs:10,error:cause=>{inputTransportError=cause;},now:inputNow,transportReady:()=>false,dispose:()=>engine.dispose(),returnToMenu:async()=>{menus++;}});
            inputTime=59999;await recovery.tick();
            if(runSettled||menus||mountedModule._PartyBoardTestExternalFrames()!==resumedFrozen)throw Error('Cross-engine recovery expired early');
            inputTime=60000;const cleanupDeadline=performance.now()+5000;
            while(!menus&&!inputTransportError&&performance.now()<cleanupDeadline)await new Promise(resolve=>setTimeout(resolve,1));
            if(inputTransportError)throw inputTransportError;
            if(!menus)throw Error('Cross-engine automatic expiry did not complete');
            await execution;await recovery.tick();await recovery.close();
            if(menus!==1||engine.phase!=='Disposed'||inputQueue.status(inputNow()).state!=='ReturnToMenu'||inputQueue.status(inputNow()).frame!==BigInt(inputCrossPolls+64))throw Error('Cross-engine expiry cleanup failed');
            console.log('Actual cross-engine RTC deadline passed: retained at 59999ms; disposed/menu once at 60000ms (injected monotonic clock).');
          }
          if(inputRtcLossTest) {
            inputPairs[0].hostChannel.close();
            if(inputMultiLossTest)inputPairs[1].hostChannel.close();
            if(inputSuspendTest) {
              const deadline=performance.now()+5000;
              while((inputQueue.status(inputNow()).state!=='Paused'||(inputMultiLossTest&&inputChannels.ready(33)))&&performance.now()<deadline)await new Promise(resolve=>setTimeout(resolve,1));
              const frozen=mountedModule._PartyBoardTestExternalFrames();
              const {createRealtimeInputRecovery}=await import('/realtime-input-recovery.mjs');
              let menuReturns=0;
              const recovery=createRealtimeInputRecovery(inputQueue,inputBridge,{execution,channels:inputChannels,intervalMs:10,error:cause=>{inputTransportError=cause;},now:inputNow,transportReady:peer=>peer?inputChannels.ready(peer):[22,33,44].every(peer=>inputChannels.ready(peer)),
                dispose:()=>engine.dispose(),returnToMenu:async()=>menuReturns++});
              const token=recovery.attempt(22);
              await replaceInputPeer();
              if(runSettled||engine.phase!=='Running'||mountedModule._PartyBoardTestExternalFrames()!==frozen)throw Error('Suspended input advanced engine');
              if(await recovery.restore({...token,checkpoint:(token.checkpoint+1)>>>0},async()=>true))throw Error('Altered pending checkpoint resumed');
              for(const field of ['frand','rand8'])if(await recovery.restore({...token,[field]:(token[field]+1)>>>0},async()=>true))throw Error('Altered RNG resumed suspended input');
              replyThrough=1n;
              let finalToken=token;
              if(inputMultiLossTest) {
                if(await recovery.restore(token,async accepted=>accepted.checkpoint===inputCheckpoints[1]))throw Error('One recovered peer resumed two missing peers');
                if(inputQueue.status(inputNow()).state!=='Paused'||runSettled||mountedModule._PartyBoardTestExternalFrames()!==frozen)throw Error('Partial recovery advanced engine');
                await replaceInputPeer(1);finalToken=recovery.attempt(33);
              }
              if(!await recovery.restore(finalToken,async accepted=>accepted.checkpoint===inputCheckpoints[1]))throw Error('Suspended recovery coordinator rejected current checkpoint');
              if(inputMultiLossTest)console.log('Actual multiple input loss passed: first peer stayed suspended; both peers permitted completion.');
              while(inputQueue.status(inputNow()).frame<2n&&!runSettled&&performance.now()<deadline)await new Promise(resolve=>setTimeout(resolve,1));
              if(inputQueue.status(inputNow()).frame!==2n||runSettled||inputTransportError)throw Error('Suspended input did not complete');
              console.log('Actual suspended input recovery passed: same Asyncify call completed after RTC replacement.');
              while(inputBridge.pendingState()?.frame!==2n&&!runSettled&&performance.now()<deadline)await new Promise(resolve=>setTimeout(resolve,1));
              if(inputBridge.pendingState()?.frame!==2n)throw Error('Engine did not reach next pending input');
              const lossStarted=performance.now();inputPairs[0].hostChannel.close();
              while(inputQueue.status(inputNow()).state!=='Paused'&&performance.now()<deadline)await new Promise(resolve=>setTimeout(resolve,1));
              if(inputWallClock)await new Promise(resolve=>setTimeout(resolve,1000));
              else inputTime=59999;await recovery.tick();
              if(runSettled||engine.phase!=='Running'||menuReturns!==0)throw Error('Suspended recovery expired early');
              inputTime=60000;
              while(menuReturns===0&&!inputTransportError&&performance.now()<(inputWallClock?lossStarted+65000:deadline))await new Promise(resolve=>setTimeout(resolve,inputWallClock?50:1));
              if(inputTransportError)throw inputTransportError;await execution;
              if(inputWallClock){const elapsed=performance.now()-lossStarted;if(elapsed<60000||elapsed>=65000||mountedModule._PartyBoardTestExternalFrames()!==3)throw Error('Real-time suspended engine deadline failed: '+elapsed);console.log('Actual suspended engine real wall-clock deadline passed: '+elapsed.toFixed(1)+' ms.');}
              if(engine.phase!=='Disposed'||menuReturns!==1)throw Error('Suspended deadline cleanup failed');
              await recovery.tick();await recovery.close();
              if(menuReturns!==1)throw Error('Suspended menu return duplicated');
              console.log(inputWallClock?'Actual suspended recovery deadline passed: real monotonic timer disposed engine and returned menu once.':'Actual suspended recovery deadline passed: automatic timer retained at 59999ms; disposed at 60000ms without manual tick.');
            } else {
            await execution;
            if(engine.phase!=='Idle'||inputQueue.status(inputNow()).state!=='Paused')throw Error('Input channel loss did not release engine into paused state');
            console.log('Actual RTC input loss passed: pending PAD wait released before explicit disposal.');
            if(inputRtcResumeTest) {
              const before=engine.pausedState(),token=inputQueue.status(inputNow());
              await replaceInputPeer();
              if(JSON.stringify(engine.pausedState())!==JSON.stringify(before))throw Error('Replacement signaling changed paused state');
              if(!inputQueue.recovered(22,token,inputNow())||!inputQueue.resume(token,inputNow()))throw Error('Explicit queue recovery rejected');
              replyThrough=1n;runSettled=false;
              execution=engine.resume().then(()=>{runSettled=true;});
              await execution;
              if(inputTransportError)throw inputTransportError;
              if(inputCheckpoints[1]===inputCheckpoints[2]||inputQueue.status(inputNow()).frame!==1n||
                  inputQueue.status(inputNow()).state!=='ReturnToMenu'||engine.phase!=='Idle')
                throw Error('Changed actual checkpoint was not rejected safely');
              console.log('Actual RTC resume safety passed: changed pending checkpoint rejected; queue did not advance.');
            }
          }
            }
          await engine.dispose();await execution;
          if(inputQueueTest&&inputQueue.status(inputNow()).frame!==BigInt(inputCrossPeer?inputCrossPolls+(inputCrossLoss?64:0):inputSuspendTest?2:1))throw Error('Cancellation advanced PAD queue');
          inputChannels?.close();
          recoveryConnections.forEach(connection=>connection.close());
          console.log('Actual pending input cancellation passed: stop released Asyncify wait before cleanup.');
          output.push('Active engine stop passed: cleanup waited for run completion.');
        } else if(externalStop) {
          const untilRunning=performance.now()+5000;
          while(mountedModule._PartyBoardTestExternalFrames()<1 && !runSettled && performance.now()<untilRunning)
            await new Promise(resolve=>setTimeout(resolve,1));
          if(runSettled||engine.phase!=='Running')throw Error('Engine ended before active-stop test');
          const beforeStop=mountedModule._PartyBoardTestExternalFrames();
          if(beforeStop<1||beforeStop>=5)throw Error('Active stop outside bounded running window');
          const {createSessionRecovery}=await import('/session-recovery.mjs');
          const channel=recoveryChannel;
          let recoveryTime=0,menuReturns=0;
          const recoverySession={channel,execution,runtime:{engine,dispose:()=>engine.dispose()}};
          const recovery=createSessionRecovery(recoverySession,
            {owners:[1,2,0,0],hostPeer:1,now:()=>recoveryTime,returnToMenu:async()=>menuReturns++});
          recoveryRemoteChannel.close();
          await execution;
          if(recovery.state!=='Paused'||engine.phase!=='Idle')throw Error('Recovery did not pause actual engine');
          const pausedFrame=mountedModule._PartyBoardTestExternalFrames();
          const pausedState=engine.pausedState();
          if(pausedState.frame!==pausedFrame||JSON.stringify(engine.pausedState())!==JSON.stringify(pausedState))
            throw Error('Actual paused state was not stable');
          if(mountedModule._PartyBoard_GamePausedState(4)!==-1)throw Error('Invalid paused-state field accepted');
          console.log('Actual paused recovery state:',JSON.stringify(pausedState));
          const attempt=recovery.attempt;
          const replacementPair=await createRecoveryPair();
          const replacement=replacementPair.clientChannel;
          if(mountedModule._PartyBoardTestExternalFrames()!==pausedFrame)throw Error('Replacement RTC signaling advanced paused engine');
          const {createRecoveryCommit,matchesRecoveryCommit}=await import('/recovery-commit.mjs');
          const recoveryIdentity={session:'01'.repeat(16),buildId:'b'.repeat(64),assetId:'a'.repeat(64),
            profile:'unlocked-no-save-v1',seed:1,revision:1,membership:1,
            players:[{peer:'host',slot:0},{peer:'guest',slot:1}]};
          // Input pump is not wired to this diagnostic engine yet. Keep its
          // separate fixture counter explicit rather than equating game frames.
          const recoveryInput={state:'Paused',frame:0n,epoch:attempt.epoch,revision:attempt.revision};
          const commit=createRecoveryCommit(recoveryIdentity,attempt,pausedState,recoveryInput);
          if(await recovery.verifyRecoveredHost(attempt,()=>matchesRecoveryCommit({...commit,frand:(commit.frand+1)>>>0},
              recoveryIdentity,attempt,engine.pausedState(),recoveryInput)))throw Error('Altered RNG recovery commit accepted');
          if(engine.phase!=='Idle')throw Error('Mismatched recovery commit advanced actual engine');
          const receivedCommit=new Promise(resolve=>replacement.addEventListener('message',event=>resolve(JSON.parse(event.data)),{once:true}));
          replacementPair.hostChannel.send(JSON.stringify(commit));
          const recoveryCommit=await receivedCommit;
          if(!await recovery.verifyRecoveredHost(attempt,()=>matchesRecoveryCommit(recoveryCommit,recoveryIdentity,attempt,engine.pausedState(),recoveryInput)))
            throw Error('Actual paused state recovery commit differed');
          runSettled=false;
          if(!recovery.resumeRecoveredHost(attempt,replacement,()=>matchesRecoveryCommit(recoveryCommit,recoveryIdentity,attempt,engine.pausedState(),recoveryInput)))
            throw Error('Actual recovery commit resume rejected');
          const resumedExecution=Promise.resolve(recoverySession.execution).then(()=>{runSettled=true;});
          const resumeDeadline=performance.now()+15000;
          while(mountedModule._PartyBoardTestExternalFrames()<=pausedFrame&&engine.phase==='Running'&&performance.now()<resumeDeadline)
            await new Promise(resolve=>setTimeout(resolve,1));
          if(mountedModule._PartyBoardTestExternalFrames()<=pausedFrame||engine.phase!=='Running')throw Error('Actual engine did not continue after recovery');
          replacementPair.hostChannel.close();await resumedExecution;
          if(recovery.state!=='Paused'||engine.phase!=='Idle')throw Error('Second loss did not pause resumed engine');
          console.log('Real engine recovery resume passed: RTC commit matched actual paused state; initialized runtime continued without frame reset.');
          recoveryTime=59999;await recovery.tick();
          if(engine.phase!=='Idle'||menuReturns!==0)throw Error('Recovery released actual engine early');
          recoveryTime=60000;await recovery.tick();await recovery.tick();
          if(engine.phase!=='Disposed'||menuReturns!==1)throw Error('Recovery deadline cleanup differs');
          console.log('Real engine recovery passed: active pause, retention until deadline, cleanup/menu once.');
          recoveryConnections.forEach(connection=>connection.close());
          output.push('Active engine stop passed: cleanup waited for run completion.');
        } else {
          await execution;
          if(engine.phase!=='Idle')throw Error('Engine did not return idle');
          if(typeof mountedModule._PartyBoardTestPipelineCount==='function')
            console.log('Actual post-execution pipeline counters:',JSON.stringify({queued:mountedModule._PartyBoardTestPipelineCount(0),created:mountedModule._PartyBoardTestPipelineCount(1),frame:mountedModule._PartyBoardTestExternalFrames()}));
          if(mountedModule._PartyBoard_GameAlignStartClock()!==-1)throw Error('Late clock alignment accepted');
          await engine.dispose();
        }
        await engine.dispose();
        if(engine.phase!=='Disposed')throw Error('Engine cleanup did not complete');
        if(inputExchangeTest) {
          if(exchangedSamples<1)throw Error('Actual game never awaited input exchange');
          console.log('Browser input exchange hook passed:',exchangedSamples,inputRtcTest?'awaited four-pad samples (real RTC, synthetic peer pads).':inputQueueTest?'awaited four-pad samples (session queue fixture).':'awaited four-pad samples (pass-through fixture).');
          exchangeOwner.close();
        }
      }
      if(mode==='modesellink' || mode==='bootlink' || mode==='gamediscasync' || mode==='assetasync' || mode==='webgpu' || mode==='worker' || mode==='aurora') {
        const completion=mode==='modesellink' ? 'Mode-select link passed:' : mode==='bootlink' ? 'Boot overlay link passed:' : renderDisc ? 'Disc renderer passed:' : mode==='gamediscasync' ? 'Browser DOL lifecycle passed:' : mode==='assetasync' ? 'Browser asset mount passed:' : mode==='aurora' ? 'Aurora initialization passed:' : mode==='worker' ? 'Render worker passed:' : 'WebGPU probe passed:';
        const deadline=performance.now()+(globalThis.testOnlineMenu?120000:boardRun?(sampleLimit>20004?780000:sampleLimit>17004?540000:sampleLimit>12004?420000:300000):mentFrames?(sampleLimit>4804?180000:120000):modeEntry?130000:menuSlot||watcherStart?45000:35000);
        let lastDiagnostic=performance.now();
        while(!runtimeAborted && !output.some(line=>line.startsWith(completion)) && performance.now()<deadline) {
          await new Promise(resolve=>setTimeout(resolve,20));
          if(mentFrames && mountedModule.FS.analyzePath('/startup.trace').exists)globalThis.mentTraceBytes=mountedModule.FS.stat('/startup.trace').size;
          if(mentFrames && performance.now()-lastDiagnostic>5000) {
            lastDiagnostic=performance.now();
            const trace=mountedModule.FS.readFile('/startup.trace');
            const samples=(trace.length-16)/68, recentA=[];
            for(let sample=Math.max(0,samples-12);sample<samples;sample++)
              if(trace[16+sample*68+21]&1)recentA.push(sample);
            console.log('Game setup diagnostic: '+JSON.stringify({traceBytes:trace.length,samples,recentA}));
          }
        }
      }
      const cardFiles=[];
      if(guestSession) {
        await guestSession.flush();
        if(guestSession.status.state!=='ready' || guestSession.status.persistent!==false)
          throw Error('Guest session did not remain ephemeral');
        await guestSession.close();
      }
      if(saveAdapter) {
        const revision=await saveAdapter.flush();
        if(!saveNotifications || revision!==saveNotifications || saveFailures.length)
          throw Error('Automatic save completion failed: '+JSON.stringify({revision,saveNotifications,saveFailures}));
        const saved=await saveStore.load('543b93edb8fc1facd2559011bf1fba9f090ecb5fc7193769a1b241b31c178fa9',0);
        const current=mountedModule.FS.readFile('/MemoryCardA.USA.raw');
        if(saved.bytes.length!==current.length || saved.bytes.some((byte,i)=>byte!==current[i]))
          throw Error('Automatically persisted card differs from completed game card');
        await saveAdapter.close();saveStore.close();
        console.log('Automatic game save persisted: '+revision+' completed save(s), full card bytes verified.');
      }
      if(menuSlot || watcherStart) {
        const visit=dir=>{
          for(const name of mountedModule.FS.readdir(dir)) {
            if(name==='.' || name==='..')continue;
            const file=dir+'/'+name, stat=mountedModule.FS.stat(file);
            if(mountedModule.FS.isDir(stat.mode))visit(file);
            else cardFiles.push({path:file,bytes:stat.size});
          }
        };
        for(const dir of ['/USA','/EUR','/JAP']) {
          if(mountedModule.FS.analyzePath(dir).exists)visit(dir);
        }
        for(const name of mountedModule.FS.readdir('/')) {
          if(/^MemoryCard.*\.raw$/.test(name)) {
            const file='/'+name, header=new Uint8Array(0x6000), stream=mountedModule.FS.open(file,'r');
            try { mountedModule.FS.read(stream,header,0,header.length,0); }
            finally { mountedModule.FS.close(stream); }
            const entries=[];
            // Report both directory copies; this is an inventory, not checksum validation.
            for(const block of [1,2])for(let index=0;index<127;index++) {
              const offset=block*0x2000+index*0x40;
              if(header[offset]===0xff)continue;
              const nameBytes=header.slice(offset+8,offset+40);
              const end=nameBytes.indexOf(0);
              entries.push({directory:block,index,game:new TextDecoder().decode(header.slice(offset,offset+4)),maker:new TextDecoder().decode(header.slice(offset+4,offset+6)),name:new TextDecoder().decode(nameBytes.slice(0,end<0?32:end)),blocks:(header[offset+0x38]<<8)|header[offset+0x39]});
            }
            cardFiles.push({path:file,bytes:mountedModule.FS.stat(file).size,entries});
          }
        }
      }
      let cardExport;
      if(menuSlot) {
        const bytes=mountedModule.FS.readFile('/MemoryCardA.USA.raw'), parts=[];
        for(let i=0;i<bytes.length;i+=16384)parts.push(String.fromCharCode(...bytes.subarray(i,i+16384)));
        cardExport=btoa(parts.join(''));
      }
      if(mode==='assetasync') {
        const fs=mountedModule.FS;
        const stream=fs.open('/assets/verified-disc.ciso','r');
        let blocked=false;
        try {asyncStats.dispose();}catch(error){blocked=/Close disc streams/.test(error.message);}
        if(!blocked)throw Error('Open disc did not block disposal');
        fs.close(stream);
        asyncStats.dispose();asyncStats.dispose();
        if(fs.analyzePath('/assets').exists)throw Error('Disc mount directory was not removed');
        output.push('Async Blob disposal passed: open-stream guard and repeat cleanup');
      }
      let mentButtonEdges=[];
      let mentRightSamples=0;
      let boardRightSamples=0;
      if(mentFrames) {
        const trace=mountedModule.FS.readFile('/startup.trace');
        let previous=false;
        for(let offset=16,index=0;offset+68<=trace.length;offset+=68,index++) {
          const pressed=(trace[offset+21]&1)!==0; // PAD_BUTTON_A = 0x100.
          if(index>=2800 && trace[offset+22]>=50 && trace[offset+22]<128)mentRightSamples++;
          if(index>=5200 && trace[offset+22]>=50 && trace[offset+22]<128)boardRightSamples++;
          if(pressed && !previous)mentButtonEdges.push(index);
          previous=pressed;
        }
        // Preserve the frame captured by C++ before teardown destroys the canvas.
        if(!globalThis.bootProbeImage)globalThis.bootProbeImage=document.getElementById('canvas').toDataURL('image/png');
      }
      const traceBytes=watcherProbe ? Array.from(mountedModule.FS.readFile('/startup.trace')) : null;
      let pipelineExport;
      const pipelineSceneVisits=globalThis.pipelineSceneVisits||{};
      if(globalThis.testCapturePipelines && mentFrames){
        const requiredScene=boardReturn?'Board return frames passed:':resultRun?'Results frames passed:':minigameRun?'Minigame frames passed:':instRun?'Instruction frames passed:':boardRun?'Board frames passed:':'Game setup frames passed:';
        if(runtimeAborted || !output.some(line=>line.startsWith(requiredScene)))
          throw Error('Pipeline capture rejected: required scene and owned cleanup were not verified ('+requiredScene+'). Last output: '+output.slice(-8).join('\n'));
      }
      if(globalThis.testCapturePipelines){const bytes=mountedModule.FS.readFile('/pipeline_cache.db');const parts=[];for(let i=0;i<bytes.length;i+=16384)parts.push(String.fromCharCode(...bytes.subarray(i,i+16384)));pipelineExport=btoa(parts.join(''));}
      return {output,errors,asyncStats,cardFiles,cardExport,pipelineExport,pipelineSceneVisits,mentButtonEdges,mentRightSamples,boardRightSamples,traceBytes,threeThrowInputStates:globalThis.threeThrowInputStates||[],instructionInputStates:globalThis.instructionInputStates||[],minigameInputStates:globalThis.minigameInputStates||[],selectedMinigame:globalThis.selectedMinigame||null,boardSnapshots:globalThis.boardSnapshots||[],bootImage:globalThis.bootProbeImage};
    },{mode:process.argv[3]||'',persistenceProbe,guestProbe,guestHash,watcherProbe,watcherTitle,watcherStart,mentSelect,mentCpu,sampleLimit,boardRun,instRun,inputScript,minigameRun,resultRun,boardReturn,expectedBoard,expectedMinigame,forceM411,traceState:process.env.PARTYBOARD_TEST_TRACE_STATE==='1',modeEntry:process.env.PARTYBOARD_TEST_MODE_ENTRY==='1',externalEngine,externalStop,alignStartClock,postInitTrace,clientCommitTest,inputExchangeTest,inputCancelTest,inputQueueTest,inputRtcTest,inputRtcLossTest,inputRtcResumeTest,inputSuspendTest,inputWallClock,inputMultiLossTest,inputCrossPeer,inputCrossPolls,inputCrossRtc,inputCrossLoss,recoveryBuildId,recoveryAssetId,inputCrossStimulus,inputCrossStimulusMode,mentFrames:process.env.PARTYBOARD_TEST_MENT_FRAMES==='1'});
    const pendingResult=Promise.race([evaluation,new Promise((_,reject)=>{evaluationTimer=setTimeout(()=>{console.error('Browser contract exceeded its time limit');if(process.platform==='win32')require('node:child_process').execFileSync('taskkill',['/PID',String(process.pid),'/T','/F'],{stdio:'ignore'});reject(Error('Browser contract timeout'));},process.env.PARTYBOARD_TEST_ONLINE_MENU==='1'?150000:externalEngine?Math.max(inputWallClock?85000:55000,sampleLimit*(inputCrossPeer?40:25)+35000):process.env.PARTYBOARD_TEST_MODE_ENTRY==='1'?180000:sampleLimit>20004?810000:boardReturn?810000:minigameRun?570000:boardMove?450000:boardRun?330000:mentBoard?210000:process.env.PARTYBOARD_TEST_MENT_FRAMES==='1'?150000:55000);})]);
    // Input waits can fail before we await runtime completion. Mark rejection
    // handled now; awaiting the original promise below still propagates it.
    pendingResult.catch(()=>{});
    if(watcherStart && !inputScript) { await page.waitForFunction(()=>globalThis.watcherKeyboardReady,{},{timeout:35000}); await page.locator('#canvas').click(); await page.keyboard.press('Enter',{delay:120}); }
    if(process.env.PARTYBOARD_TEST_ONLINE_MENU==='1'&&watcherStart) {
      const wait=prefix=>page.waitForFunction(prefix=>globalThis.onlineMenuMessages?.some(line=>line.startsWith(prefix)),prefix,{timeout:60000});
      const capture=async index=>{
        await page.waitForTimeout(200);
        await page.locator('#canvas').screenshot({path:path.resolve(__dirname,'../../../outputs/validation/browser-online-menu-'+index+'.png')});
      };
      await page.waitForFunction(()=>globalThis.onlineMenuRuntime?._PartyBoard_GameFrameCount()>=1600,{},{timeout:60000});
      await page.locator('#canvas').screenshot({path:path.resolve(__dirname,'../../../outputs/validation/browser-online-menu-intro.png')});
      await page.locator('#canvas').click();
      await page.keyboard.press('z',{delay:300});
      await page.waitForTimeout(400);
      await page.locator('#canvas').screenshot({path:path.resolve(__dirname,'../../../outputs/validation/browser-online-menu-after-intro.png')});
      console.log('Online intro input diagnostic:',await page.evaluate(()=>{const bytes=globalThis.onlineMenuRuntime.FS.readFile('/startup.trace');let count=0;for(let offset=16+68*1500;offset+68<=bytes.length;offset+=68)if(bytes[offset+21]&1)count++;return {samples:(bytes.length-16)/68,pressedSamples:count};}));
      await wait('Online Multiplayer entry: available');
      await page.keyboard.press('s',{delay:120});
      await page.keyboard.press('z',{delay:120});
      await wait('Online Multiplayer menu: opened');await capture(0);
      await page.keyboard.press('z',{delay:120});
      await wait('Online Multiplayer menu: Host requested');await capture(1);
      await page.keyboard.press('x',{delay:120});await page.waitForTimeout(200);
      await page.keyboard.press('s',{delay:120});await capture(2);
      await page.keyboard.press('z',{delay:120});
      await wait('Online Multiplayer menu: Join requested');
      await page.keyboard.press('x',{delay:120});await page.waitForTimeout(200);
      await page.keyboard.press('x',{delay:120});
      await wait('Online Multiplayer menu: closed');await capture(3);
      await page.keyboard.press('x',{delay:120});
    }
    if(process.env.PARTYBOARD_TEST_MENT_INPUT==='1' && !inputScript) {
      assert.equal(process.env.PARTYBOARD_TEST_MENT_FRAMES,'1');
      // Pace real keyboard presses against recorded retraces, not wall-clock speed.
      // Trace header is 16 bytes; each four-controller sample occupies 68 bytes.
      // Begin before the observed Party Mode prompt can stop advancing retraces.
      for(const sample of (mentSelect?[800,900,1000,1100,1200,1400,1700,2000]:[800,900,1000,1100,1200])) {
        await page.waitForFunction(bytes=>globalThis.mentTraceBytes>=bytes,16+68*sample,{timeout:5000});
        await page.locator('#canvas').evaluate(canvas=>{canvas.tabIndex=0;canvas.focus();});
        await page.keyboard.press('z',{delay:120});
        if(sample===800)console.log('Party setup keyboard event:',JSON.stringify(await page.evaluate(()=>({active:document.activeElement?.id||document.activeElement?.tagName||null,events:globalThis.partySetupKeyEvents.slice(-2)}))));
      }
      console.log('Party setup input: sent browser Z/A presses.');
      if(mentSelect) {
        await page.waitForFunction(bytes=>globalThis.mentTraceBytes>=bytes,16+68*3000,{timeout:60000});
        await page.keyboard.down('d');
        try {
          await page.waitForFunction(bytes=>globalThis.mentTraceBytes>=bytes,16+68*3032,{timeout:10000});
        } finally {
          await page.keyboard.up('d');
        }
        if(mentCpu) {
          for(const sample of (mentBoard?[3100,3300,3600,3900,4200,4500,4800]:[3100,3300,3600,3900,4200,4500])) {
            await page.waitForFunction(bytes=>globalThis.mentTraceBytes>=bytes,16+68*sample,{timeout:20000});
            await page.keyboard.press('z',{delay:120});
          }
          if(mentBoard && process.env.PARTYBOARD_TEST_BOARD_CHOOSE==='1') {
            await page.waitForFunction(bytes=>globalThis.mentTraceBytes>=bytes,16+68*5200,{timeout:20000});
            await page.keyboard.down('d');
            try {
              await page.waitForFunction(bytes=>globalThis.mentTraceBytes>=bytes,16+68*5232,{timeout:10000});
            } finally {
              await page.keyboard.up('d');
            }
            await page.waitForFunction(bytes=>globalThis.mentTraceBytes>=bytes,16+68*5400,{timeout:20000});
            await page.keyboard.press('z',{delay:120});
            if(boardRun) {
              for(const sample of [6200,6400,6600,6800,7000,7200,7500,7800]) {
                await page.waitForFunction(bytes=>globalThis.mentTraceBytes>=bytes,16+68*sample,{timeout:30000});
                await page.keyboard.press('z',{delay:120});
              }
              if(boardPlay) {
                for(let sample=9000;sample<=(boardMove?14800:11800);sample+=200) {
                  await page.waitForFunction(bytes=>globalThis.mentTraceBytes>=bytes,16+68*sample,{timeout:60000});
                  await page.keyboard.press('z',{delay:120});
                }
              }
            }
          }
        }
      }
    }
    const result=await pendingResult;
    if(result.pipelineExport){
      validatePipelineSceneVisits(result.pipelineSceneVisits,{minigameRun,selectedMinigame:result.selectedMinigame});
      const target=process.env.PARTYBOARD_PIPELINE_CAPTURE_OUTPUT;if(!target)throw Error('Explicit pipeline export path required.');
      const bytes=Buffer.from(result.pipelineExport,'base64');
      fs.writeFileSync(target,bytes);
      fs.writeFileSync(target+'.json',JSON.stringify({version:1,scope:'observed-scenes-only',fullCoverage:false,
        sha256:require('node:crypto').createHash('sha256').update(bytes).digest('hex'),bytes:bytes.length,
        sampleLimit,expectedBoard,expectedMinigame,selectionScope:forceM411?'diagnostic-forced-M411':'normal-watcher',endpoint:boardReturn?'board-return':resultRun?'results':minigameRun?'minigame':instRun?'instructions':'board',
        selectedMinigame:result.selectedMinigame,simulationSceneVisits:result.pipelineSceneVisits,
        note:'Simulation visits identify observed overlays; they do not prove every shader variant or complete gameplay coverage.'},null,2)+'\n');
      delete result.pipelineExport;console.log('Actual scene pipeline cache exported:',target);
    }
    if(process.env.PARTYBOARD_TEST_ONLINE_MENU==='1') {
      for(const marker of ['Online Multiplayer entry: available','Online Multiplayer menu: opened','Online Multiplayer menu: Host requested','Online Multiplayer menu: Join requested','Online Multiplayer menu: closed'])
        assert.ok(result.output.some(line=>line.startsWith(marker)),JSON.stringify(result));
      if(watcherStart)for(const marker of ['Window keyboard glyph installed: bank 2 uses Z','Window keyboard glyph installed: bank 3 uses X'])
        assert.ok(result.output.some(line=>line.startsWith(marker)),JSON.stringify(result));
      const images=process.argv[3]==='menuframes'?await page.evaluate(()=>globalThis.onlineMenuImages):null;
      if(images)assert.equal(images.length,4);
      for(let index=0;index<(images?.length||0);index++)fs.writeFileSync(path.resolve(__dirname,'../../../outputs/validation/browser-online-menu-'+index+'.png'),Buffer.from(images[index].split(',')[1],'base64'));
      console.log('Online menu interaction passed: Host/Join, unavailable actions and return to mode selection. Transport is not connected.');
    }
    if(result.bootImage) {
      const outputPath=process.env.PARTYBOARD_TEST_IMAGE_OUTPUT?path.resolve(process.env.PARTYBOARD_TEST_IMAGE_OUTPUT):path.resolve(__dirname,'../../../outputs/validation/browser-'+process.argv[3]+'-frame.png');
      fs.writeFileSync(outputPath,Buffer.from(result.bootImage.split(',')[1],'base64'));
      delete result.bootImage;
      console.log('Boot image: '+outputPath);
    }
    if(process.env.PARTYBOARD_TEST_TRACE_OUTPUT) {
      assert.ok(result.traceBytes,'Trace export requires a watcher probe');
      fs.writeFileSync(process.env.PARTYBOARD_TEST_TRACE_OUTPUT,Buffer.from(result.traceBytes),{flag:'wx'});
    }
    delete result.traceBytes;
    if(process.env.PARTYBOARD_TEST_MINIGAME_INPUT_OBSERVER==='1') {
      assert.ok(result.minigameInputStates.length>0,'Diagnostic minigame must observe human input');
      fs.writeFileSync(path.resolve(__dirname,'../../../outputs/validation/browser-minigame-input-states.json'),JSON.stringify(result.minigameInputStates,null,2)+'\n');
      if(process.env.PARTYBOARD_TEST_MINIGAME_MOVEMENT==='1') {
        const active=result.minigameInputStates.filter(state=>state.axisX>0);
        assert.ok(active.length>30,'Human callback must accept sustained positive stick input');
        assert.equal(new Set(active.map(state=>state.player)).size,1,'Observe one human player');
        assert.ok(active.every(state=>state.pad===0 && Number.isFinite(state.x) && Number.isFinite(state.z)),'Human pad and positions must be valid');
        assert.ok(active.some(state=>Math.hypot(state.x-active[0].x,state.z-active[0].z)>1),'Human position must change while accepting stick input');
        console.log('Verified minigame human movement: '+active.length+' positive-input observations.');
      }
    }
    if(process.env.PARTYBOARD_TEST_THREE_THROW_INPUT_OBSERVER==='1') {
      fs.writeFileSync(path.resolve(__dirname,'../../../outputs/validation/browser-three-throw-input-states.json'),JSON.stringify(result.threeThrowInputStates,null,2)+'\n');
      console.log('Three Throw input observations: '+result.threeThrowInputStates.length);
      const states=result.threeThrowInputStates;
      assert.ok(states.length>30,'Human action observer must record gameplay');
      assert.ok(states.every(state=>state.pad===0&&Number.isFinite(state.y)),'Human pad and height must be valid');
      assert.ok(states.some(state=>state.buttons&0x100),'Human must accept Jump input');
      assert.ok(states.some(state=>state.buttons&0x200),'Human must accept Shoot input');
      assert.ok(states.some(state=>state.jump&&state.y>1),'Human must leave the ground');
      assert.ok(states.some((state,index)=>index>0&&states[index-1].ballHeld&&!state.ballHeld),'Human must release a basketball');
    }
    if(process.env.PARTYBOARD_TEST_INSTRUCTION_INPUT_OBSERVER==='1') {
      fs.writeFileSync(path.resolve(__dirname,'../../../outputs/validation/browser-instruction-input-states.json'),JSON.stringify(result.instructionInputStates,null,2)+'\n');
      console.log('Instruction input observations: '+result.instructionInputStates.length);
    }
    if(instRun) {
      assert.ok(result.selectedMinigame,'Instruction entry must identify selected minigame');
      fs.writeFileSync(path.resolve(__dirname,'../../../outputs/validation/browser-selected-minigame.json'),JSON.stringify(result.selectedMinigame,null,2)+'\n');
    }
    if(process.env.PARTYBOARD_TEST_MINIGAME_AUDIT==='1') {
      for(let id=416;id<=424;id++)assert.ok(result.output.some(line=>line.startsWith(`Minigame module load passed: m${id} `)),`Missing minigame load audit ${id}`);
    }
    if(process.env.PARTYBOARD_TEST_BOARD_AUDIT==='1') {
      for(const id of ['01','02','03','05','06','10'])assert.ok(result.output.some(line=>line.startsWith(`Board module load passed: w${id} `)),`Missing board load audit ${id}`);
    }
    if(process.env.PARTYBOARD_TEST_THREE_THROW_AUDIT==='1')assert.ok(result.output.some(line=>line.startsWith('Three Throw module load passed:')),'Missing Three Throw load audit');
    if(process.env.PARTYBOARD_TEST_FOUR_PLAYER_AUDIT==='1')for(let id=401;id<=415;id++)assert.ok(result.output.some(line=>line.startsWith(`Four-player module load passed: m${id} `)),`Missing four-player module ${id}`);
    if(process.env.PARTYBOARD_TEST_COMPETITIVE_AUDIT==='1')for(let id=425;id<=449;id++)assert.ok(result.output.some(line=>line.startsWith(`Competitive module load passed: m${id} `)),`Missing competitive module ${id}`);
    if(process.env.PARTYBOARD_TEST_MODE_ENTRY==='1') { fs.writeFileSync(path.resolve(__dirname,'../../../outputs/validation/browser-mode-entry-diagnostics.json'),JSON.stringify({errors:result.errors,pageErrors:errors,output:result.output},null,2)+'\n'); assert.ok(result.output.some(line=>line.startsWith('Minigame Mode entry passed:')),JSON.stringify({errors:result.errors,pageErrors:errors})); }
    if(process.env.PARTYBOARD_TEST_MODE_MENU_AUDIT==='1')for(const name of ['mgmodedll', 'mpexDll', 'option', 'present', 'mstoryDll', 'mstory2Dll', 'mstory3Dll', 'mstory4Dll', 'selmenuDll', 'staffDll', 'subchrselDll', 'ztardll'])assert.ok(result.output.some(line=>line.startsWith(`Mode menu load passed: /${name}.wasm `)),`Missing mode menu ${name}`);
    if(process.env.PARTYBOARD_TEST_REMAINING_MINIGAME_AUDIT==='1')for(const id of [450,451,453,455,456,457,458,459,460,461,462,463])assert.ok(result.output.some(line=>line.startsWith(`Remaining minigame load passed: m${id} `)),`Missing remaining minigame ${id}`);
    clearTimeout(evaluationTimer);
    if(process.env.PARTYBOARD_TEST_MENT_INPUT==='1' && !inputScript) {
      const expectedPresses=boardMove?54:boardPlay?39:boardRun?24:mentSelect?(mentBoard?(process.env.PARTYBOARD_TEST_BOARD_CHOOSE==='1'?16:15):mentCpu?14:8):5;
      assert.equal(result.mentButtonEdges.filter(sample=>sample>=1500).length,expectedPresses,'Game input trace must receive all A presses');
      console.log('Party setup recorded A edges: '+JSON.stringify(result.mentButtonEdges));
      if(mentSelect)assert.ok(result.mentRightSamples>0,'Game input must receive positive left-stick X from D');
      if(mentBoard && process.env.PARTYBOARD_TEST_BOARD_CHOOSE==='1')assert.ok(result.boardRightSamples>0,'Board selection must receive directional input');
    }
    if(result.cardExport) { fs.writeFileSync(generatedCardPath,Buffer.from(result.cardExport,'base64'));delete result.cardExport; }
    if(boardRun) {
      fs.writeFileSync(path.resolve(__dirname,'../../../outputs/validation/browser-board-state.json'),JSON.stringify(result.boardSnapshots,null,2)+'\n');
      assert.ok(result.boardSnapshots.length>0,'Board observer must capture state');
      console.log('Final board state: '+JSON.stringify(result.boardSnapshots.at(-1)));
      if(boardMove)console.log('Verified first board phase movement: '+JSON.stringify(validateBoardMovement(boardReturn?result.boardSnapshots.filter(state=>state.turn===1):result.boardSnapshots)));
      if(boardReturn)assert.ok(result.boardSnapshots.some(state=>state.turn>=2),'Board return must advance to the next turn');
    }
    if(process.argv[3]==='menuslot') console.log('Temporary card files: '+JSON.stringify(result.cardFiles));
    const settingsInfo = new Set([
      "[info] [partyboard::config] Saving config to '/config.json'",
      "[info] [partyboard::config] Loading config from '/config.json'",
      "[info] [partyboard::config] Loading config from '/missing-config.json'",
      "[info] [partyboard::config] Loading config from '/broken-config.json'",
      '[error] [partyboard::config] Malformed configuration JSON; keeping current settings',
      '[info] [partyboard::config] Config file did not exist, staying with defaults'
    ]);
    const unexpectedStderr = process.argv[3] === 'settings'
      ? result.errors.filter(line => !settingsInfo.has(line))
      : ['savereload','menuslot','menuchoose','menuframes','boottransition','bootstart','boottitle','bootassets','bootframes','bootschedule','bootsetup','frameprobe','startupgame','renderdisc','gamediscasync','gamedisc','assetasync','assetmount','assetworker','assetworkerlarge'].includes(process.argv[3])
        ? result.errors.filter(line => line !== '[info] [partyboard::version] Loaded game disc is GMPE01' && !(process.argv[3]==='frameprobe' && (['Input trace: recording, seed 1',`Input trace: recording complete (${sampleLimit} samples)`].includes(line) || /^Input trace: closed after \d+ samples$/.test(line))))
        : result.errors;
    if(process.env.PARTYBOARD_TEST_MENT_FRAMES==='1')assert.ok(result.output.some(line=>line.startsWith(boardReturn?'Board return frames passed:':resultRun?'Results frames passed:':minigameRun?'Minigame frames passed:':instRun?'Instruction frames passed:':boardRun?'Board frames passed:':'Game setup frames passed:')),JSON.stringify(result));
    if(result.asyncStats)console.log('Async Blob reads: '+JSON.stringify(result.asyncStats));
    if(['renderdisc','gamediscasync','gamedisc','assetasync','assetmount','assetworker','assetworkerlarge'].includes(process.argv[3]))
      assert.ok(result.output.includes('Browser disc I/O passed: end seek, short read, EOF and rewind.'), JSON.stringify(result));
    const checkedStderr=inputScript?unexpectedStderr.filter(line=>line!=='Input script: controller stimulus only; recording NEW checkpoints'):unexpectedStderr;
    if(inputScript)assert.ok(result.errors.includes('Input script: controller stimulus only; recording NEW checkpoints'),'Engine must confirm input script activation');
    assert.deepEqual(errors,[],JSON.stringify(result)); assert.deepEqual(checkedStderr,[],JSON.stringify(result));
    if(process.argv[3]==='aurora') {
      assert.ok(result.output.some(line=>line.startsWith('Aurora initialization passed:')), JSON.stringify(result));
      assert.ok(result.output.some(line=>line.startsWith('Aurora frames passed:')), JSON.stringify(result));
      assert.ok(result.output.some(line=>line.startsWith('Aurora pixels passed:')), JSON.stringify(result));
      assert.ok(result.output.some(line=>line.startsWith('Aurora GX passed:')), JSON.stringify(result));
      assert.ok(result.output.some(line=>line.startsWith('Aurora texture passed:')), JSON.stringify(result));
      assert.ok(result.output.some(line=>line.startsWith('Aurora assets passed:')), JSON.stringify(result));
      assert.ok(result.output.some(line=>line.startsWith('Aurora card passed:')), JSON.stringify(result));
      assert.ok(result.output.some(line=>line.startsWith('Aurora RmlUi passed:')), JSON.stringify(result));
      assert.ok(result.output.some(line=>line.startsWith('Aurora text passed:')), JSON.stringify(result));
      assert.ok(result.output.some(line=>line.startsWith('Aurora pacing passed:')), JSON.stringify(result));
      assert.ok(result.output.some(line=>line.startsWith('Aurora retrace passed:')), JSON.stringify(result));
      assert.ok(result.output.some(line=>line.startsWith('Aurora menu passed:')), JSON.stringify(result));
      assert.ok(result.output.some(line=>line.startsWith('Aurora document passed:')), JSON.stringify(result));
      console.log('PASS: Edge initialized and shut down the actual Aurora runtime.');
    } else if(externalEngine) {
      assert.ok(result.output.some(line=>line.startsWith('External engine lifecycle passed:')),JSON.stringify(result));
      assert.ok(result.errors.some(line=>externalStop?(inputCrossPeer?line===`Input trace: closed after ${inputCrossPolls+4+(inputCrossLoss?64:0)} samples`:/^Input trace: closed after [4-7] samples$/.test(line)):line===`Input trace: closed after ${sampleLimit} samples`),JSON.stringify(result));
      if(externalStop)assert.ok(result.output.includes('Active engine stop passed: cleanup waited for run completion.'),JSON.stringify(result));
      console.log('PASS: JavaScript awaited real engine initialization and bounded execution, then cleaned up.');
    } else if(process.argv[3]==='savereload' || process.argv[3]==='menuslot' || process.argv[3]==='menuchoose' || process.argv[3]==='menuframes' || process.argv[3]==='boottransition' || process.argv[3]==='bootstart' || process.argv[3]==='boottitle' || process.argv[3]==='bootassets' || process.argv[3]==='bootframes' || process.argv[3]==='bootschedule' || process.argv[3]==='bootsetup' || process.argv[3]==='frameprobe' || process.argv[3]==='startupgame') {
      if(process.argv[3]==='frameprobe') {
        const completed=result.output.find(line=>line.startsWith('Bounded game loop passed:'));
        assert.ok(completed,JSON.stringify(result));
        const frames=Number(completed.match(/passed: (\d+) frames/)[1]);
        assert.ok(frames>0 && frames<=sampleLimit);
        // PadReadVSync also records initialization retraces; samples are not frames.
        const closure=result.errors.find(line=>/^Input trace: closed after \d+ samples$/.test(line));
        assert.ok(closure,JSON.stringify(result));
        assert.ok(Number(closure.match(/after (\d+) samples/)[1])<=sampleLimit);
        if(watcherTitle)assert.ok(frames>=900);
        if(watcherProbe)assert.ok(result.output.some(line=>line.startsWith('Watcher boot passed:')),JSON.stringify(result));
        else assert.ok(result.output.includes('Browser overlay startup failed; stopping game loop.'),JSON.stringify(result));
        if(process.env.PARTYBOARD_TEST_INST_LOAD==='1')assert.ok(result.output.some(line=>line.startsWith('Instruction module load passed:')),JSON.stringify(result));
        assert.ok(!result.output.some(line=>line.includes('memory free error')),JSON.stringify(result));
        assert.ok(!result.output.some(line=>/Buffer mapping (Aborted|CallbackCancelled)|Timed out draining browser staging/.test(line)),JSON.stringify(result));
      }
      if(['menuslot','menuchoose','menuframes','boottransition','bootstart','boottitle','bootassets','bootframes','bootschedule'].includes(process.argv[3])) {
        assert.ok(result.output.some(line=>line.startsWith('Boot scheduler passed:')),JSON.stringify(result));
        assert.ok(!result.output.some(line=>line.includes('memory free error')),JSON.stringify(result));
      }
      if(process.argv[3]==='savereload') assert.ok(result.output.some(line=>line.startsWith('Fresh save reload passed:')),JSON.stringify(result));
      if(process.argv[3]==='menuslot') {
        assert.ok(result.output.some(line=>line.startsWith('Menu slot probe completed:')),JSON.stringify(result));
        assert.ok(result.output.some(line=>line.startsWith('Browser save readback passed:')),JSON.stringify(result));
        const card=result.cardFiles.find(file=>file.path==='/MemoryCardA.USA.raw');
        assert.ok(card,JSON.stringify(result.cardFiles));
        for(const directory of [1,2]) {
          assert.ok(card.entries.some(entry=>entry.directory===directory && entry.name==='MARIPA4BOX0' && entry.game==='GMPE' && entry.maker==='01' && entry.blocks===2),JSON.stringify(card));
        }
      }
      if(process.argv[3]==='menuchoose') assert.ok(result.output.some(line=>line.startsWith('Menu selection probe completed:')),JSON.stringify(result));
      if(process.argv[3]==='menuframes') assert.ok(result.output.some(line=>line.startsWith('Mode-select rendering passed:')),JSON.stringify(result));
      if(process.argv[3]==='boottransition') assert.ok(result.output.some(line=>line.startsWith('Mode-select setup passed:')),JSON.stringify(result));
      if(process.argv[3]==='bootstart') assert.ok(result.output.some(line=>line.startsWith('Boot Start passed:')),JSON.stringify(result));
      if(process.argv[3]==='boottitle') assert.ok(result.output.some(line=>line.startsWith('Boot long probe passed:')),JSON.stringify(result));
      if(process.argv[3]==='bootassets') assert.ok(result.output.some(line=>line.startsWith('Boot assets passed:')),JSON.stringify(result));
      if(process.argv[3]==='bootframes') assert.ok(result.output.some(line=>line.startsWith('Boot rendering passed:')),JSON.stringify(result));
      if(['menuslot','menuchoose','menuframes','boottransition','bootstart','boottitle','bootassets','bootframes','bootschedule','bootsetup'].includes(process.argv[3])) assert.ok(result.output.some(line=>line.startsWith('Boot setup passed:')),JSON.stringify(result));
      assert.ok(result.output.some(line=>line.startsWith('Game system initialization passed:')),JSON.stringify(result));
      if(watcherProbe) assert.ok(result.output.some(line=>line.startsWith('Dynamic sprite cleanup passed:')),JSON.stringify(result));
      if(watcherStart) assert.deepEqual(result.cardFiles, [], 'No-save startup must not create card storage');
      if(watcherStart) assert.ok(result.output.some(line=>line.includes('Session profile: all rewards unlocked; memory card bypassed')), 'Expected current no-save menu module');
      assert.ok(result.output.some(line=>line.startsWith('Disc renderer passed:')),JSON.stringify(result));
      console.log('PASS: Real game system initialization and heap allocation in Edge.');
      if(persistenceProbe) console.log('PASS: Actual generated game card restored from IndexedDB after page reload; game checksum and stats load succeeded.');
    } else if(process.argv[3]==='gamedisc' || process.argv[3]==='gamediscasync') {
      assert.ok(result.output.some(line=>line.startsWith('Browser DOL lifecycle passed:')), JSON.stringify(result));
      assert.ok(result.output.some(line=>line.startsWith('Browser bootstrap assets passed:')), JSON.stringify(result));
      console.log(result.output.find(line=>line.startsWith('Browser bootstrap assets passed:')));
      assert.ok(result.output.some(line=>line.startsWith('Browser local game disc passed:')), JSON.stringify(result));
      console.log(result.output.find(line=>line.startsWith('Browser local game disc passed:')));
    } else if(process.argv[3]==='renderdisc' || process.argv[3]==='assetasync' || process.argv[3]==='assetmount' || (process.argv[3]||'').startsWith('assetworker')) {
      if(process.argv[3]==='renderdisc') assert.ok(result.output.includes('Disc renderer passed: red before async disc reads, green after reads, clean shutdown.'), JSON.stringify(result));
      if(process.argv[3]==='assetasync' || process.argv[3]==='renderdisc') assert.ok(result.asyncStats.reads>0, JSON.stringify(result));
      assert.ok(result.output.some(line=>line.startsWith('Browser DOL bounds passed:')), JSON.stringify(result));
      assert.ok(result.output.some(line=>line.startsWith('Browser asset mount passed:')), JSON.stringify(result));
      if(process.argv[3]==='assetworkerlarge') {
        const heap=Number(result.output.find(line=>line.startsWith('Browser heap bytes: '))?.split(': ')[1]);
        assert.ok(heap>0&&heap<64*1024*1024,JSON.stringify(result));
        console.log('PASS: 128 MiB CISO reads use '+heap+' bytes of Wasm linear memory (under 64 MiB).');
      }
      console.log('PASS: Edge mounted verified IndexedDB assets in Wasm and read them through Aurora DVD.');
    } else if(process.argv[3]==='fiberlog') {
      assert.ok(result.output.some(line=>line.startsWith('Fiber logger passed:')),JSON.stringify(result));
      console.log('PASS: Actual logger and heap traversal completed on a browser fiber.');
    } else if(process.argv[3]==='modesellink') {
      assert.ok(result.output.some(line=>line.startsWith('Mode-select link passed:')),JSON.stringify(result));
      console.log('PASS: Actual mode-select overlay imports resolve against the browser engine; setup not executed.');
    } else if(process.argv[3]==='bootlink') {
      assert.ok(result.output.some(line=>line.startsWith('Boot overlay link passed:')), JSON.stringify(result));
      console.log('PASS: Boot overlay imports resolved against the real engine; ObjectSetup was not executed.');
    } else if(process.argv[3]==='overlaygate') {
      assert.ok(result.output.some(line=>line.startsWith('Browser overlay gate passed:')), JSON.stringify(result));
      console.log('PASS: Browser overlay loader rejects unsupported execution.');
    } else if(process.argv[3]==='relassets') {
      assert.ok(result.output.some(line=>line.startsWith('REL assets passed:')), JSON.stringify(result));
      console.log('PASS: Edge rejected incomplete REL assets and verified resource cleanup.');
    } else if(process.argv[3]==='inputmode') {
      assert.ok(result.output.includes('Browser input mode passed: local pads preserved; unwired online input rejected.'),JSON.stringify(result));
      console.log('PASS: Edge preserves local input and rejects unavailable online input.');
    } else if(process.argv[3]==='trace') {
      assert.ok(result.output.includes('Input trace integration tests passed'), JSON.stringify(result));
      console.log('PASS: Edge executed actual input trace record/replay and divergence checks.');
    } else if(process.argv[3]==='disc') {
      assert.ok(result.output.some(line=>line.startsWith('Browser disc passed:')), JSON.stringify(result));
      assert.ok(result.output.some(line=>line.startsWith('Browser valid disc passed:')), JSON.stringify(result));
      assert.ok(result.output.some(line=>line.startsWith('Browser malformed DOL passed:')), JSON.stringify(result));
      assert.ok(result.output.some(line=>line.startsWith('Browser DVD passed:')), JSON.stringify(result));
      assert.ok(result.output.some(line=>line.startsWith('Browser CISO passed:')), JSON.stringify(result));
      console.log('PASS: Edge verified disc validation, file reads and Aurora DVD callbacks.');
    } else if(process.argv[3]==='settings') {
      assert.ok(result.output.some(line=>line.startsWith('Browser settings passed:')), JSON.stringify(result));
      assert.ok(result.output.some(line=>line.startsWith('Browser malformed config passed:')), JSON.stringify(result));
      console.log('PASS: Edge executed actual settings persistence and validation in temporary memory.');
    } else if(process.argv[3]==='dependencies') {
      assert.ok(result.output.some(line=>line.startsWith('Browser dependencies passed:')));
      assert.ok(result.output.some(line=>line.startsWith('Browser PNG passed:')));
      console.log('PASS: Edge executed compiled renderer dependency libraries.');
    } else if(process.argv[3]==='worker') {
      assert.ok(result.output.some(line=>line.startsWith('Render worker passed:')));
      console.log('PASS: Edge executed the actual render worker inline contract.');
    } else if(process.argv[3]==='webgpu') {
      assert.ok(result.output.some(line=>line.startsWith('WebGPU probe passed:')));
      assert.ok(result.output.some(line=>line.startsWith('WebGPU render passed:')));
      assert.ok(result.output.some(line=>line.startsWith('WebGPU shader passed:')));
      assert.ok(result.output.some(line=>line.startsWith('WebGPU surface passed:')));
      console.log('PASS: Edge created WebGPU adapter/device/queue through C++ bindings.');
    } else if(process.argv[3]==='sdl') {
      assert.ok(result.output.some(line=>line.startsWith('SDL browser probe passed:')));
      console.log('PASS: Edge initialized SDL3 window and gamepad subsystem.');
    } else if(process.argv[3]==='overlays') {
      assert.ok(result.output.some(line=>line.startsWith('Overlay probe passed:')));
      assert.ok(result.output.some(line=>line.startsWith('Overlay reset passed:')));
      assert.ok(result.output.includes('Overlay loader requires explicit state reset; dlclose is insufficient.'));
      console.log('PASS: Edge executed side-module isolation and retained-state checks.');
    } else if(process.argv[3]==='scheduler') {
      assert.ok(result.output.some(line=>line.startsWith('Scheduler contract passed:')));
      console.log('PASS: Edge executed the game process scheduler contract.');
    } else {
    assert.ok(result.output.some(line=>line.startsWith('Coroutine contract passed:')));
    assert.ok(result.output.some(line=>line.startsWith('Coroutine lifecycle passed:')));
    console.log('PASS: Edge executed WebAssembly coroutine contract with three stacks and 300 work yields.');
    }
  }finally{
    clearTimeout(evaluationTimer);
    if(browser)await browser.close();
    server.closeAllConnections();
    await new Promise(resolve=>server.close(resolve));
  }
})().catch(error=>{console.error(error);process.exitCode=1;}).finally(()=>{if(inputCrossPeer&&process.connected)process.disconnect();});























