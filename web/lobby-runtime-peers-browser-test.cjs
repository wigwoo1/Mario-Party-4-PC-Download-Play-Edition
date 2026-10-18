// Exercise the retained diagnostic transport controls explicitly.
const manualURL=value=>{const url=new URL(value);url.searchParams.set('manual','1');return url.href;};
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const fs=require('node:fs');const {spawn,execFileSync}=require('child_process');const assert=require('node:assert/strict');const path=require('node:path');
const {serializeSettledObservations}=require('../tools/serialize-settled-observations.cjs');
const {cursorKey,setupMenuOwnerIndex,turnSetupAction,retryObservedSetupAction,turnLengths,partySetupKeys}=require('../tools/party-turn-setup.cjs');
const {DEFAULT_FULL_MATCH_TIMEOUT_MS,fullMatchHarnessTimeout,fullMatchTimeoutFromEnvironment}=require('../tools/drive-production-full-match.cjs');
// Driver direction plans use WASD names; actual default movement uses arrow events.
const movementKey={w:'ArrowUp',a:'ArrowLeft',s:'ArrowDown',d:'ArrowRight'};
const pressKeyboard=(page,key,options)=>page.keyboard.press(movementKey[key]??key,options);
const requestedFullMatchTimeoutMs=fullMatchTimeoutFromEnvironment(process.env);
const fullMatchHarnessTimeoutMs=fullMatchHarnessTimeout(requestedFullMatchTimeoutMs??DEFAULT_FULL_MATCH_TIMEOUT_MS);
async function observe(page,evaluate){
 let deadline;
 try{return await Promise.race([page.evaluate(evaluate),new Promise((_,reject)=>{deadline=setTimeout(()=>reject(Error('Browser observation did not respond within 30 seconds.')),30000);})]);}
 finally{clearTimeout(deadline);}
}
(async()=>{
 const outputDirectory=path.resolve(process.env.PARTYBOARD_TEST_OUTPUT_DIRECTORY||path.resolve(__dirname,'../../../outputs/validation'));
 fs.mkdirSync(outputDirectory,{recursive:true});
 if(process.env.PARTYBOARD_TEST_GAME_FULL_MATCH==='1')for(const name of ['PARTYBOARD_TEST_COMMITTED_GAME','PARTYBOARD_TEST_GAME_BOOT_MENU','PARTYBOARD_TEST_GAME_PARTY_BOARD'])assert.equal(process.env[name],'1','Full-match validation requires actual committed boot and Party setup.');
 const requestedFullMatchTurns=Number(process.env.PARTYBOARD_TEST_GAME_FULL_MATCH_TURNS||20);
 if(process.env.PARTYBOARD_TEST_GAME_PARTY_BOARD==='1')assert.ok(turnLengths.includes(requestedFullMatchTurns),'Requested Party length must be selectable from 5 through 50 turns in five-turn steps.');
 if(process.env.PARTYBOARD_RUNTIME_DIRECTORY){
  for(const name of ['contract.mjs','contract.wasm']){
   const artifact=path.resolve(process.env.PARTYBOARD_RUNTIME_DIRECTORY,name);
   const handle=fs.openSync(artifact,'r');
   try{
    const header=Buffer.alloc(8),length=fs.readSync(handle,header,0,8,0);
    assert.equal(length,8,'Runtime artifact is empty or truncated: '+name);
    if(name.endsWith('.wasm'))assert.deepEqual(header,Buffer.from([0,97,115,109,1,0,0,0]),'Invalid runtime Wasm header');
   }finally{fs.closeSync(handle);}
  }
  const buildDirectory=path.resolve(process.env.PARTYBOARD_RUNTIME_DIRECTORY,'..');
  if(fs.existsSync(path.join(buildDirectory,'setup-link-options.rsp'))){
   const manifest=path.join(outputDirectory,'gameplay-preflight-packaging.json');
   const audit=execFileSync(process.execPath,[path.resolve(__dirname,'../tools/verify-embedded-overlays.cjs'),buildDirectory,manifest,path.resolve(process.env.PARTYBOARD_RUNTIME_DIRECTORY,'contract.wasm')],{encoding:'utf8'});
   console.log(audit.trim());
   const helpers=execFileSync(process.execPath,[path.resolve(__dirname,'../tools/verify-inline-helper-imports.cjs'),manifest],{encoding:'utf8'});
   console.log(helpers.trim());
  }
 }
 const port=Number(process.env.PARTYBOARD_TEST_PORT||8799);assert.ok(Number.isInteger(port)&&port>0&&port<=65535,'Valid test server port required');
 const server=spawn(process.execPath,[__dirname+'/serve.cjs'],{env:{...process.env,PARTYBOARD_TEST_PORT:String(port)},stdio:['ignore','pipe','inherit']});let browser,browserServer,failed=false,timer,completedNaturalCycle=false,completedFullMatchProgress=false;const pages=[],gameMarkers=[],runtimeWarnings=[];
 const pageErrors=[],consoleErrors=[];
 let releaseInspection=()=>{};
 try{
  await new Promise((resolve,reject)=>{
   const cleanup=()=>{server.stdout.removeListener('data',ready);server.removeListener('error',failed);server.removeListener('exit',exited);};
   const ready=()=>{cleanup();resolve();},failed=error=>{cleanup();reject(error);},exited=code=>failed(Error('Test server exited before readiness: '+code));
   server.stdout.once('data',ready);server.once('error',failed);server.once('exit',exited);
  });browserServer=await chromium.launchServer({...(process.env.PARTYBOARD_TEST_BROWSER_EXECUTABLE
    ?{executablePath:process.env.PARTYBOARD_TEST_BROWSER_EXECUTABLE}
    :{channel:process.env.PARTYBOARD_TEST_BROWSER_CHANNEL||'msedge'}),headless:true,args:['--mute-audio',...(process.env.PARTYBOARD_TEST_DIRECT_ICE==='1'?['--disable-features=WebRtcHideLocalIpsWithMdns']:[]),...(process.env.PARTYBOARD_BROWSER_INSPECTION_FILE?['--remote-debugging-port=0']:[])]});
  releaseInspection=await require('../tools/retain-browser-inspection.cjs').retainBrowserServerInspection(process.env.PARTYBOARD_BROWSER_INSPECTION_FILE,browserServer);
  browser=await chromium.connect(browserServer.wsEndpoint());
  const count=Number(process.env.PARTYBOARD_PEERS||2);assert.ok(count===2||count===4);
  const guestResolutions=process.env.PARTYBOARD_TEST_GUEST_RESOLUTIONS?JSON.parse(process.env.PARTYBOARD_TEST_GUEST_RESOLUTIONS):null;
  if(guestResolutions){
    assert.ok(Array.isArray(guestResolutions)&&guestResolutions.length===count-1,'One selected resolution per guest required');
    for(const size of guestResolutions)assert.ok(['640x480','960x720','1280x960','1920x1440','2560x1920'].includes(size),'Supported guest resolution required');
  }
 const pinballDiagnostics=[];
 const tracePinball=process.env.PARTYBOARD_TEST_PINBALL_DIAGNOSTICS==='1';
 if(tracePinball)fs.writeFileSync(path.join(outputDirectory,'browser-pinball-diagnostics.json'),'[]\n');
 const errors=[],lifecycleEvents=[];const recordLifecycle=(event)=>{lifecycleEvents.push({...event,at:Date.now()});if(lifecycleEvents.length>500)lifecycleEvents.shift();fs.writeFileSync(path.join(outputDirectory,'browser-production-peer-lifecycle-events.json'),JSON.stringify(lifecycleEvents,null,2)+'\n');};
 browser.on('disconnected',()=>recordLifecycle({scope:'browser',type:'disconnected'}));
 for(let i=0;i<count;i++){const context=await browser.newContext();gameMarkers[i]=[];runtimeWarnings[i]=[];
    context.on('close',()=>recordLifecycle({scope:'context',peer:i,type:'close'}));
    if(tracePinball)await context.addInitScript(()=>{globalThis.testPinballDiagnostics=true;});
    if(process.env.PARTYBOARD_TEST_FRAME_STAGE_DIAGNOSTICS==='1')await context.addInitScript(()=>{globalThis.testFrameStageDiagnostics=true;});
    if(process.env.PARTYBOARD_TEST_REWIND_DIAGNOSTICS==='1')await context.addInitScript(()=>{globalThis.testAsyncifyRewindDiagnostics=true;});
    await context.addInitScript(()=>{
      const Original=globalThis.RTCPeerConnection;
      globalThis.testPeerConnections=[];globalThis.testPeerConnectionEvents=[];
      globalThis.testPeerLifecycleEvents=[];
      const recordLifecycle=(type,detail={})=>{
        testPeerLifecycleEvents.push({type,time:performance.now(),visibility:document.visibilityState,readyState:document.readyState,...detail});
        if(testPeerLifecycleEvents.length>200)testPeerLifecycleEvents.shift();
      };
      for(const type of ['pagehide','pageshow','beforeunload','freeze','resume'])addEventListener(type,event=>recordLifecycle(type,{persisted:!!event.persisted}));
      document.addEventListener('visibilitychange',()=>recordLifecycle('visibilitychange'));
      addEventListener('error',event=>recordLifecycle('error',{message:String(event.message||'').slice(0,500),filename:event.filename?new URL(event.filename,location.href).pathname:'',line:event.lineno||0}));
      addEventListener('unhandledrejection',event=>recordLifecycle('unhandledrejection',{reason:String(event.reason?.stack||event.reason||'').slice(0,1000)}));
      globalThis.RTCPeerConnection=class extends Original{
        constructor(...args){
          super(...args);this.testConnectionId=testPeerConnections.length;testPeerConnections.push(this);
          this.testIceCandidates=[];
          this.addEventListener("icecandidate",event=>{if(event.candidate)this.testIceCandidates.push({type:event.candidate.type,protocol:event.candidate.protocol,addressKind:event.candidate.address?.endsWith(".local")?"mdns":event.candidate.address?.includes(":")?"ipv6":"ipv4"});});
          for(const type of ['connectionstatechange','iceconnectionstatechange','signalingstatechange','icegatheringstatechange','icecandidateerror'])
            this.addEventListener(type,event=>this.testRecordConnectionEvent(type,type==='icecandidateerror'?{errorCode:event.errorCode||0}:{}));
        }
        testRecordConnectionEvent(type,extra={},stack){
          testPeerConnectionEvents.push({id:this.testConnectionId,type,time:performance.now(),connection:this.connectionState,ice:this.iceConnectionState,signaling:this.signalingState,...extra,stack});
          if(testPeerConnectionEvents.length>200)testPeerConnectionEvents.shift();
        }
        close(){this.testRecordConnectionEvent('explicit-close',{},new Error('Connection close caller').stack);return super.close();}
      };
    });
    const page=await context.newPage();pages.push(page);
    page.on('crash',error=>recordLifecycle({scope:'page',peer:i,type:'crash',error:String(error).slice(0,1000)}));
    page.on('close',()=>recordLifecycle({scope:'page',peer:i,type:'close'}));
    if(tracePinball)page.on('console',message=>{
      const text=message.text();if(!text.startsWith('Pinball diagnostic: '))return;
      pinballDiagnostics.push({peer:i,observation:text});
      if(pinballDiagnostics.length>1200)pinballDiagnostics.shift();
      fs.writeFileSync(path.join(outputDirectory,'browser-pinball-diagnostics.json'),JSON.stringify(pinballDiagnostics,null,2)+'\n');
    });
    page.on('console',message=>{if(message.type()==='warning'){runtimeWarnings[i].push(message.text());if(runtimeWarnings[i].length>200)runtimeWarnings[i].shift();}});
    // Preserve the resource path behind otherwise ambiguous browser console
    // errors. Query strings may carry invitations, so never print them here.
    page.on('response',response=>{if(response.status()>=400)console.error('Browser peer '+i+' HTTP '+response.status()+': '+new URL(response.url()).pathname);});
    page.on('requestfailed',request=>console.error('Browser peer '+i+' request failed: '+new URL(request.url()).pathname+'; '+request.failure()?.errorText));
    page.on('pageerror',e=>{errors.push(e.message);pageErrors.push({peer:i,message:e.message,stack:String(e.stack||'').slice(0,16000)});});page.on('console',message=>{const text=message.text();if(message.type()==='error'){consoleErrors.push({peer:i,text});if(consoleErrors.length>1000)consoleErrors.shift();const location=message.location();let locationPath='';try{locationPath=location.url?new URL(location.url).pathname:'';}catch{}console.error('Browser peer '+i+': '+text+(locationPath?' at '+locationPath:''));}if(text.includes('Start New OVL ')||text.includes('Online Multiplayer entry:')||text.includes('M411 startup:')||text.includes('objman>ObjectSetup end')||text.includes('Browser overlay startup failed')||text.includes('Party flow ')||text.includes('Party setup confirmation ')||text.includes('Party setup presentation ')||text.includes('Party setup retry choice=')){gameMarkers[i].push(text);if(gameMarkers[i].length>1000)gameMarkers[i].shift();}});await page.goto(manualURL(`http://127.0.0.1:${port}`));await page.evaluate(async()=>{globalThis.partyBoardApp=await import('/app.mjs');});}
  const host=pages[0];
  if(process.env.PARTYBOARD_TEST_STUN_SERVER){
    await Promise.all(pages.map(async page=>{
      await page.locator('details:has(#stun) > summary').click();
      await page.locator('#stun').fill(process.env.PARTYBOARD_TEST_STUN_SERVER);
    }));
  }
  pages.forEach((page,index)=>page.on('console',message=>{const text=message.text();
    if(text.startsWith('Party board selection ')||text.includes('Degenerate camera ')){gameMarkers[index].push(text);console.log('Actual peer '+index+': '+text);}
  }));
  let previousProgress, telemetryPending=false;
  timer=setInterval(()=>{
    if(telemetryPending)return;
    telemetryPending=true;
    return Promise.all(pages.map(page=>observe(page,()=>({status:document.querySelector('#status')?.textContent,transport:globalThis.partyBoardApp?.lobbyTransportDiagnostics?.(),rtcEvents:globalThis.testPeerConnectionEvents??[],pageLifecycle:globalThis.testPeerLifecycleEvents??[],rtcConnections:(globalThis.testPeerConnections??[]).map(connection=>({connectionState:connection.connectionState,iceConnectionState:connection.iceConnectionState,signalingState:connection.signalingState,iceGatheringState:connection.iceGatheringState,candidates:connection.testIceCandidates??[],localCandidateCount:(connection.localDescription?.sdp.match(/^a=candidate:/gm)||[]).length,remoteCandidateCount:(connection.remoteDescription?.sdp.match(/^a=candidate:/gm)||[]).length})),heapFailure:globalThis.partyBoardApp?.lobbyHeapFailure(),game:globalThis.partyBoardApp?.lobbyGameState(),inputTiming:globalThis.partyBoardApp?.lobbyInputTiming?.(),activeFrameStage:globalThis.partyBoardActiveFrameStage??null,rendererBeginStage:globalThis.partyBoardRendererBeginStage??null,stagingWait:globalThis.partyBoardStagingWait??null,framePacingTiming:globalThis.partyBoardFramePacingTiming??null,frameStageTiming:globalThis.partyBoardFrameStageTiming??null,rendererStageTiming:globalThis.partyBoardRendererStageTiming??null,rendererSubmissionTiming:globalThis.partyBoardRendererSubmissionTiming??null,rendererStagingTiming:globalThis.partyBoardRendererStagingTiming??null})))).then(peers=>{
    const now=Date.now(),elapsed=previousProgress?(now-previousProgress.time)/1000:null;
    const framesPerSecond=peers.map((peer,index)=>{
      const previous=previousProgress?.peers[index]?.game;
      return elapsed&&peer.game?.input.state==='Running'&&previous?.input.state==='Running'&&peer.game.frame>=previous.frame?
        Number(((peer.game.frame-previous.frame)/elapsed).toFixed(2)):null;
    });
    console.log(JSON.stringify({progress:peers.map(peer=>peer.status),frames:peers.map(peer=>peer.game?.frame??null),gameProgress:peers.map(peer=>peer.game?.progress??null),framesPerSecond,heapSnapshots:peers.map(peer=>peer.heapFailure??null),inputExchangeTiming:peers.map(peer=>peer.inputTiming??null),framePacingTiming:peers.map(peer=>peer.framePacingTiming),frameStageTiming:peers.map(peer=>peer.frameStageTiming),rendererStageTiming:peers.map(peer=>peer.rendererStageTiming),rendererSubmissionTiming:peers.map(peer=>peer.rendererSubmissionTiming),rendererStagingTiming:peers.map(peer=>peer.rendererStagingTiming)}));
    fs.writeFileSync(path.join(outputDirectory,'browser-last-responsive-peer-observation.json'),JSON.stringify({time:now,peers},null,2)+'\n');
    // Retain pre-failure channel and frame-stage evidence even when cleanup
    // replaces the latest observation with an ended session.
    fs.appendFileSync(path.join(outputDirectory,'browser-peer-observation-history.jsonl'),JSON.stringify({time:now,peers})+'\n');
    previousProgress={time:now,peers};
  }).catch(error=>console.error('Browser telemetry unavailable: '+String(error))).finally(()=>{telemetryPending=false;});
  },20000);
  let expectedPipelineSeed=null;
  if(process.env.PARTYBOARD_PIPELINE_SEED){
   console.log('Selecting known shader seed for the actual host.');
   const bytes=fs.readFileSync(process.env.PARTYBOARD_PIPELINE_SEED),sha256=require('node:crypto').createHash('sha256').update(bytes).digest('hex');
   const created=Number(process.env.PARTYBOARD_EXPECT_PIPELINES||40);assert.ok(Number.isSafeInteger(created)&&created>0,'Expected pipeline count must be a positive integer');
   expectedPipelineSeed={sha256,created};
   if(process.env.PARTYBOARD_TEST_SHADER_PICKER==='1'){
    await host.locator('#pipeline-file').setInputFiles(process.env.PARTYBOARD_PIPELINE_SEED);
    await host.waitForFunction(()=>document.querySelector('#status').textContent.startsWith('Known shader definitions selected.'));
   }else await host.evaluate(async({bytes,sha256})=>(await import('/app.mjs')).setLobbyPipelineSeed({blob:new Blob([new Uint8Array(bytes)]),sha256}),{bytes:Array.from(bytes),sha256});
  }
  console.log('Selecting the host local game file.');
  await host.locator('#file').setInputFiles(process.env.PARTYBOARD_LOCAL_CISO);
  for(const [guestIndex,guest] of pages.slice(1).entries()){
    console.log('Creating actual invitation for guest '+(guestIndex+1)+'.');
    await host.locator('#host').click();
    await host.waitForFunction(()=>!document.querySelector('#answer').disabled&&document.querySelector('#local').value.length>0,{},{timeout:240000});
    await guest.locator('#remote').fill(await host.locator('#local').inputValue());await guest.locator('#join').click();
    await guest.waitForFunction(()=>document.querySelector('#local').value.length>0);
    await host.locator('#remote').fill(await guest.locator('#local').inputValue());await host.locator('#answer').click();
    await guest.waitForFunction(()=>!document.querySelector('#accept').disabled);await guest.locator('#accept').click();
    if(guestResolutions){
      assert.equal(await guest.locator('#guest-resolution').isEnabled(),true,'Guest can select before renderer preparation');
      await guest.locator('#guest-resolution').selectOption(guestResolutions[guestIndex]);
      assert.equal(await guest.locator('#guest-resolution').inputValue(),guestResolutions[guestIndex]);
    }
    if(process.env.PARTYBOARD_TEST_LOBBY_MUSIC==='1'){
      // Join is the user gesture that must start background music. Do not
      // hide an automatic-start failure with an extra Play music click here.
      const playing=track=>{const audio=document.querySelector('#lobby-audio');return audio?.currentSrc.endsWith(track)&&!audio.paused&&audio.readyState>=2&&audio.currentTime>0&&audio.error===null;};
      await guest.waitForFunction(playing,'wifi-menu.opus',{timeout:30000});
      const firstTime=await guest.evaluate(()=>document.querySelector('#lobby-audio').currentTime);
      await guest.waitForFunction(time=>document.querySelector('#lobby-audio').currentTime>time+0.5,firstTime,{timeout:10000});
      // Seek the real decoded HTML audio near its end; the production ended
      // handler must select the second file and then wrap to the first.
      for(const track of ['currently-spectating.opus','wifi-menu.opus']){
        await guest.waitForFunction(()=>{const audio=document.querySelector('#lobby-audio');return Number.isFinite(audio.duration)&&audio.duration>1;},{},{timeout:30000});
        await guest.evaluate(()=>{const audio=document.querySelector('#lobby-audio');if(!Number.isFinite(audio.duration)||audio.duration<=1)throw Error('Decoded track duration required.');audio.currentTime=audio.duration-0.2;});
        await guest.waitForFunction(playing,track,{timeout:30000});
        const trackTime=await guest.evaluate(()=>document.querySelector('#lobby-audio').currentTime);
        await guest.waitForFunction(time=>document.querySelector('#lobby-audio').currentTime>time+0.5,trackTime,{timeout:10000});
      }
      console.log('Actual guest Join automatically started audio; both Opus clocks advanced and the playlist naturally wrapped. Audible listening remains unverified.');
    }
  }
  if(process.env.PARTYBOARD_TEST_STUN_SERVER){
    const iceEvidence=await Promise.all(pages.map(page=>page.evaluate(()=>testPeerConnections.map(connection=>({
      candidateTypes:[...new Set(connection.testIceCandidates.map(candidate=>candidate.type))].sort(),
      candidateCount:connection.testIceCandidates.length,
      gatheringState:connection.iceGatheringState,
      connectionState:connection.connectionState,
    })))));
    assert.equal(iceEvidence.length,count);
    for(const [peerIndex,connections] of iceEvidence.entries()){
      assert.ok(connections.length>0,'Peer '+peerIndex+' must create production peer connections');
      if(peerIndex===0)for(const [connectionIndex,connection] of connections.entries())
        assert.ok(connection.candidateTypes.includes('srflx'),
          'Same-machine offerer connection '+connectionIndex+' must gather a server-reflexive candidate');
    }
    fs.writeFileSync(path.join(outputDirectory,'browser-production-stun-candidate-types.json'),JSON.stringify(iceEvidence,null,2)+'\n');
    console.log('Production lobby offerers gathered server-reflexive candidates; all peer candidate types were retained without addresses. Same-machine answerer candidate pruning is not cross-machine evidence.');
  }
  await Promise.all(pages.slice(1).map(guest=>guest.waitForFunction(()=>document.querySelector('#status').textContent.startsWith('Assets checked and prepared.'),{},{timeout:900000})));
  console.log('Actual host-to-guest CISO download verified.');
  // Separate contexts and modules; each prepares its own verified asset source.
  for(const [index,page] of pages.entries()){console.log('Preparing actual engine for peer '+index+'.');await page.locator('#prepare-game').click();await page.waitForFunction(()=>/initialized and paused|preparation failed/.test(document.querySelector('#status').textContent),{},{timeout:240000});assert.match(await page.locator('#status').textContent(),/initialized and paused/);}
  if(guestResolutions){
    const evidence=[];
    for(const [index,guest] of pages.slice(1).entries()){
      const [width,height]=guestResolutions[index].split('x').map(Number);
      const actual=await observe(guest,()=>({width:document.querySelector('#canvas').width,height:document.querySelector('#canvas').height}));
      assert.deepEqual(actual,{width,height},'Actual guest framebuffer must match loading-page selection');
      assert.equal(await guest.locator('#guest-resolution').isDisabled(),true,'Prepared resolution selection must be locked');
      evidence.push({peer:index+1,selected:guestResolutions[index],actual});
    }
    fs.writeFileSync(path.join(outputDirectory,'browser-production-guest-resolutions.json'),JSON.stringify(evidence,null,2)+'\n');
  }
  await host.waitForFunction(()=>Object.keys(partyBoardApp.lobbyPreparedReports()).length===Number(document.querySelectorAll('#lobby-players progress').length));
  const reports=await host.evaluate(async()=>(await import('/app.mjs')).lobbyPreparedReports());
  assert.equal(Object.keys(reports).length,count);for(const report of Object.values(reports))assert.deepEqual(reports.host,report);assert.equal(reports.host.frame,0);
  assert.equal(reports.host.pipelineSeed,expectedPipelineSeed?.sha256??null);
  if(process.env.PARTYBOARD_TEST_PINNED_RUNTIME==='1'){
    const configuration=await host.evaluate(()=>partyBoardApp.lobbySessionConfiguration());
    assert.match(configuration.buildId,/^[a-f0-9]{64}$/);
    assert.equal(configuration.buildId,reports.host.buildId);
    for(const page of pages)assert.deepEqual(await page.evaluate(()=>partyBoardApp.lobbySessionConfiguration()),configuration);
    console.log('Actual host and guests retain the measured invitation package pin; every prepared receipt matches it.');
  }
  for(const page of pages){const state=await page.evaluate(async()=>(await import('/app.mjs')).lobbyRuntimeState());assert.equal(state.phase,'Idle');assert.equal(state.buildId,reports.host.buildId);assert.equal(state.checkpoint,reports.host.checkpoint);}
  for(const page of pages){
    const installed=await page.evaluate(async()=>(await import('/app.mjs')).connectLobbyGameInput());assert.equal(installed.installed,true);
    const state=await page.evaluate(async()=>(await import('/app.mjs')).lobbyRuntimeState());
    if(expectedPipelineSeed){assert.equal(state.pipelineSeed.sha256,expectedPipelineSeed.sha256);assert.equal(state.rendererPreparation.created,expectedPipelineSeed.created);assert.equal(state.rendererPreparation.queued,0);}
    assert.equal(state.inputExchangeInstalled,true);assert.equal(state.phase,'Idle');assert.equal(state.frame,0);
    assert.equal(state.checkpoint,reports.host.checkpoint);
  }
  console.log('Actual Wasm online-input requirement installed on all paused lobby modules.');
  if(process.env.PARTYBOARD_TEST_KNOWN_SHADER_READY==='1'){
    for(const page of pages){await page.locator('#ready-game').click();assert.match(await page.locator('#status').textContent(),/^Ready: /);}
    for(const page of pages)await page.waitForFunction(()=>partyBoardApp.lobbyReadyState()?.canStart===true);
    for(const page of pages){const ready=await page.evaluate(async()=>(await import('/app.mjs')).lobbyReadyState());assert.ok(ready.players.every(player=>player.ready&&player.shaders));}
    console.log('Known shader cache prepared on each actual GPU before explicit player Ready; no full coverage claim.');
    if(process.env.PARTYBOARD_TEST_COMMITTED_GAME==='1'){
      await host.locator('#start-agreement').click();
      await host.waitForFunction(()=>partyBoardApp.lobbyStartState()?.state==='Agreed');
      await host.locator('#start-game').click();
      if(process.env.PARTYBOARD_TEST_LOBBY_MUSIC==='1')for(const guest of pages.slice(1))await guest.waitForFunction(()=>{const audio=document.querySelector('#lobby-audio');return audio.paused&&!audio.hasAttribute('src')&&document.querySelector('#music-toggle').disabled;},{},{timeout:30000});
      for(const page of pages)await page.waitForFunction(()=>{const state=partyBoardApp.lobbyGameState();return state?.phase==='Running'&&state.frame>=120&&BigInt(state.input.frame)>=120n&&state.input.state==='Running';},{},{timeout:60000});
      const running=await Promise.all(pages.map(page=>page.evaluate(async()=>(await import('/app.mjs')).lobbyGameState())));
      for(const state of running){assert.equal(state.phase,'Running');assert.ok(state.frame>=120);assert.ok(BigInt(state.input.frame)>=120n);assert.equal(state.input.state,'Running');}
      const observations=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyInputObservations())));
      const shared=observations[0].filter(record=>observations.every(records=>records.some(other=>other.frame===record.frame)));
      assert.ok(shared.length>=30,'At least thirty completed common input polls required.');
      for(const record of shared)for(const records of observations)assert.deepEqual(records.find(other=>other.frame===record.frame),record);
      console.log('Actual matching completed input/RNG/checkpoint polls: '+shared.length);
      console.log('Actual committed production game liveness: '+JSON.stringify(running));
      if(process.env.PARTYBOARD_TEST_GAME_BOOT_MENU==='1'){
        for(const page of pages)await page.waitForFunction(()=>partyBoardApp.lobbyGameState()?.frame>=950,{},{timeout:90000});
        await host.locator('#canvas').click();const deadline=Date.now()+120000;
        while(!gameMarkers.every(markers=>markers.some(text=>text.includes('Start New OVL 74 ')))){
          assert.ok(Date.now()<deadline,'Every actual game must reach mode selection.');
          await pressKeyboard(host,'Enter',{delay:120});await host.waitForTimeout(1000);
        }
        // The overlay's animated introduction is a separate simulation stage.
        // Report real frame progress instead of treating overlay load as menu readiness.
        const menuDeadline=Date.now()+600000;
        let menuFrames=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState().frame)));
        const menuProgressAt=pages.map(()=>Date.now());let menuReportAt=Date.now();
        while(!gameMarkers.every(markers=>markers.some(text=>text.includes('Online Multiplayer entry: available')))){
          const frames=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState().frame)));
          for(let index=0;index<frames.length;index++){
            if(frames[index]>menuFrames[index])menuProgressAt[index]=Date.now();
            assert.ok(Date.now()-menuProgressAt[index]<90000,'Peer '+index+' mode introduction made no simulation progress for 90 seconds.');
          }
          menuFrames=frames;
          assert.ok(Date.now()<menuDeadline,'Every actual game must open mode selection within ten minutes.');
          if(Date.now()-menuReportAt>=10000){console.log('Actual mode introduction frames: '+JSON.stringify(frames));menuReportAt=Date.now();}
          // The Party/online mode menu is driven by Player 1 (pad 0). Sending
          // A on every browser also presses pad 1 and can perturb the live
          // menu; let the host's assigned controller advance it alone.
          await pressKeyboard(host,'z',{delay:120});await host.waitForTimeout(500);
        }
        const records=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyInputObservations())));
        const common=records[0].filter(record=>records.every(other=>other.some(item=>item.frame===record.frame)));
        assert.ok(common.length>=30,'Actual mode-selection state comparison required.');
        for(const record of common)for(const other of records)assert.deepEqual(other.find(item=>item.frame===record.frame),record);
        console.log('Actual synchronized boot to mode-selection markers: '+JSON.stringify(gameMarkers));
        if(process.env.PARTYBOARD_TEST_GAME_PARTY_BOARD==='1'){
          const selectedBoard=Number(process.env.PARTYBOARD_TEST_BOARD_INDEX||0);
          assert.ok(Number.isInteger(selectedBoard)&&selectedBoard>=0&&selectedBoard<6,'Valid ordinary board selection required.');
          const boardInventory=JSON.parse(fs.readFileSync(path.resolve(__dirname,'../../../outputs/validation/browser-usa-scene-inventory.json'),'utf8'));
          const selectedOverlay=boardInventory.overlays.find(scene=>scene.name.toLowerCase()==='w0'+(selectedBoard+1)+'dll')?.overlay;
          assert.ok(Number.isInteger(selectedOverlay),'Selected board must exist in authoritative inventory.');
          const selectionOrder=[1,2,0,3,4,5];let boardChosen=false,boardSelectionConfirmed=false,turnLengthSelected=false,setupPort=null;
          // Navigate normal setup with each assigned player's real A key.
          for(const page of pages)await page.locator('#canvas').click();
          // Click the player-1 page last so subsequent keyboard input goes to
          // the canvas owning the menu's pad 0, even in multi-page Chromium.
          await host.locator('#canvas').click();
          const boardDeadline=Date.now()+180000;
          let postSettingsAdvanceSent=false;
          while(!gameMarkers.every(markers=>markers.some(text=>text.includes('Start New OVL '+selectedOverlay+' ')))){
            assert.ok(Date.now()<boardDeadline,'Every production game must enter selected Party board.');
            // Confirming the setup dialog returns into the game's final Party
            // presentation, which waits for one more Player 1 A before its
            // board transition. Send that action only after the game reports
            // a successful setup result and the live configuration still
            // matches on every peer.
            if(turnLengthSelected&&!postSettingsAdvanceSent&&gameMarkers.every(markers=>markers.some(text=>text.startsWith('Party setup confirmation result=0 ')))){
              const confirmedStates=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState())));
              if(confirmedStates.every(state=>state.progress?.overlay===70&&state.progress?.maxTurn===requestedFullMatchTurns)){
                const presentationDeadline=Date.now()+15000;
                while(Date.now()<presentationDeadline&&!gameMarkers.every(markers=>markers.some(text=>text.startsWith('Party setup presentation waiting for Player 1 A '))))await host.waitForTimeout(100);
                assert.ok(gameMarkers.every(markers=>markers.some(text=>text.startsWith('Party setup presentation waiting for Player 1 A '))),'Every peer must reach the observable final Party presentation wait.');
                // TARGET_PC logs immediately before fn_1_1968 sets the message
                // and resets the window key queue. Wait for 30 live game frames
                // after that marker so the confirmation press cannot be eaten by
                // the reset that precedes HuWinMesWait.
                const waitMarker=gameMarkers[0].filter(text=>text.startsWith('Party setup presentation waiting for Player 1 A ')).at(-1);
                const waitMarkerFrame=Number(/frame=(\d+)/.exec(waitMarker)?.[1]);
                assert.ok(Number.isInteger(waitMarkerFrame),'Final Party presentation must report its entry frame.');
                const advanceOwner=setupMenuOwnerIndex(confirmedStates,setupPort);
                await pages[advanceOwner].locator('#canvas').click();
                const inputReadyDeadline=Date.now()+5000;
                let inputReady=false;
                while(Date.now()<inputReadyDeadline){
                  const states=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState())));
                  assert.ok(states.every(state=>state.progress?.overlay===70&&state.progress?.maxTurn===requestedFullMatchTurns),'Final Party wait must remain stable while its A input becomes ready.');
                  if(states.every(state=>Number(state.frame)>=waitMarkerFrame+30)){inputReady=true;break;}
                  await host.waitForTimeout(100);
                }
                assert.ok(inputReady,'Game must advance past the message key-reset window before final A.');
                await pressKeyboard(pages[advanceOwner],'z',{delay:120});
                postSettingsAdvanceSent=true;
                const releaseDeadline=Date.now()+5000;
                while(Date.now()<releaseDeadline&&!gameMarkers.every(markers=>markers.some(text=>text.startsWith('Party setup presentation released '))))await host.waitForTimeout(100);
                assert.ok(gameMarkers.every(markers=>markers.some(text=>text.startsWith('Party setup presentation released '))),'Final Party presentation must accept the owning Player 1 A on every peer before board handoff.');
                console.log('Party setup final presentation accepted Player 1 A from owning peer '+advanceOwner+' after matching yes results.');
                continue;
              }
            }
            if(!gameMarkers[0].some(text=>text.includes('Party board selection opened.'))){
              // Before setup telemetry exists, connected players must confirm
              // the Party-mode transition from their respective local pads.
              await Promise.all(pages.map(page=>pressKeyboard(page,'z',{delay:120})));
              await host.waitForTimeout(1000);
              continue;
            }
            if(gameMarkers[0].some(text=>text.includes('Party board selection opened.'))){
              const ready=gameMarkers[0].filter(text=>text.startsWith('Party board selection ready: board=')).at(-1);
              if(!ready){await host.waitForTimeout(200);continue;}
              const current=Number(ready.match(/board=(\d+)/)[1]);
              if(current!==selectedBoard){await pressKeyboard(host,selectionOrder.indexOf(current)<selectionOrder.indexOf(selectedBoard)?'d':'a',{delay:120});await host.waitForTimeout(1000);continue;}
              if(!gameMarkers.every(markers=>markers.some(text=>text==='Party board selection ready: board='+selectedBoard))){await host.waitForTimeout(200);continue;}
              boardChosen=true;
            }
            if(boardChosen&&!boardSelectionConfirmed){
              const selectorStates=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState())));
              const selectorPorts=selectorStates.map(state=>state.progress?.menuPort);
              assert.ok(selectorPorts.every(port=>Number.isInteger(port)&&port>=0&&port<4),'Board selector must expose its active controller port.');
              setupPort=selectorPorts[0];
              assert.ok(selectorPorts.every(port=>port===setupPort),'Peers must agree on the board-selector controller port.');
              const owner=setupMenuOwnerIndex(selectorStates,setupPort);
              await pressKeyboard(pages[owner],'z',{delay:120}); // Board selector accepts A; B returns to Party Mode.
              await host.waitForTimeout(1500);
              await Promise.all(pages.map(page=>pressKeyboard(page,'z',{delay:120}))); // Confirm human characters.
              await host.waitForTimeout(2000);
              // The normal character-confirm input can leave a follow-up Party-mode
              // prompt on screen. Try the real Start action first, then bounded A
              // confirms, checking live state after each; never send turn input
              // speculatively or walk through settings without observing them.
              let setupVisible=false;
              // The stock setup requires choosing two CPU characters, dismissing
              // each character card, then accepting each default EASY difficulty.
              // Peach and Yoshi are the first two unassigned roster entries.
              const transitionKeys=['Enter','z','Enter','z','z','d','z','Enter','z','z','Enter','z','Enter','z'];
              for(const [attempt,key] of transitionKeys.entries()){
                const before=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState())));
                if(before.every(state=>Number.isInteger(state.progress?.partySetupRow)&&state.progress.partySetupRow>=0)){
                  setupVisible=true;break;
                }
                await pressKeyboard(pages[owner],key,{delay:120});
                await host.waitForTimeout(800);
                const after=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState())));
                console.log('Party setup transition probe '+(attempt+1)+' ('+key+'): '+JSON.stringify(after.map(state=>({overlay:state.progress?.overlay,partySetupRow:state.progress?.partySetupRow,partySetupTurns:state.progress?.partySetupTurns,partySetupPort:state.progress?.partySetupPort}))));
                await pages[owner].locator('#canvas').screenshot({path:path.join(outputDirectory,'browser-setup-transition-'+(attempt+1)+'-peer'+owner+'.png')});
                if(after.every(state=>Number.isInteger(state.progress?.partySetupRow)&&state.progress.partySetupRow>=0)){
                  setupVisible=true;break;
                }
              }
              // The board picker transitions to turn settings with an in-game
              // animation; allow its telemetry to appear after the final choice.
              if(!setupVisible){
                const settleDeadline=Date.now()+10000;
                while(Date.now()<settleDeadline){
                  await host.waitForTimeout(250);
                  const settled=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState())));
                  if(settled.every(state=>Number.isInteger(state.progress?.partySetupRow)&&state.progress.partySetupRow>=0)){
                    setupVisible=true;break;
                  }
                }
                await pages[owner].locator('#canvas').screenshot({path:path.join(outputDirectory,'browser-party-setup-settled-peer'+owner+'.png')});
              }
              if(!setupVisible){
                const settled=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState())));
                assert.ok(settled.every(state=>state.progress?.overlay===70),
                  'Party setup must remain in mentDll after the board transition before using the known default sequence.');
                console.log('Turn-selector observer is absent from this cached browser runtime; settled settings screenshot confirms its default 20-turn menu.');
                // This runtime predates partySetup telemetry. The immediately
                // preceding screenshot shows the real settings menu at 20 turns,
                // with its initial "Please adjust the settings" prompt still open.
                // Dismiss that prompt before applying the source-verified sequence;
                // later require the running board's authoritative maxTurn to match.
                await pressKeyboard(pages[owner],'z',{delay:120});
                await host.waitForTimeout(500);
                await pages[owner].locator('#canvas').screenshot({path:path.join(outputDirectory,'browser-party-setup-open-peer'+owner+'.png')});
                const setupKeys=partySetupKeys(20,requestedFullMatchTurns);
                for(let index=0;index<setupKeys.length-1;index++){
                  const key=setupKeys[index];
                  await pressKeyboard(pages[owner],key,{delay:120});
                  await host.waitForTimeout(500);
                  if(index===0||index===3)
                    await pages[owner].locator('#canvas').screenshot({path:path.join(outputDirectory,'browser-party-setup-turns-row-step'+index+'-peer'+owner+'.png')});
                }
                await pages[owner].locator('#canvas').screenshot({path:path.join(outputDirectory,'browser-party-setup-requested-turns-peer'+owner+'.png')});
                await pressKeyboard(pages[owner],setupKeys.at(-1),{delay:120});
                await host.waitForTimeout(500);
                await pressKeyboard(pages[owner],'z',{delay:120}); // Confirm Yes on "Are these settings OK?"
                // Do not trust a sent key sequence as proof of the chosen
                // length: the running game must expose the configured value
                // on every peer before this test proceeds into the match.
                await Promise.all(pages.map(page=>page.waitForFunction(turns=>{
                  const progress=partyBoardApp.lobbyGameState()?.progress;
                  return progress?.turn>=1&&progress.maxTurn===turns;
                },requestedFullMatchTurns,{timeout:10000})));
                turnLengthSelected=true;
                boardSelectionConfirmed=true;
                await host.waitForTimeout(1000);
                continue;
              }
              assert.ok(setupVisible||turnLengthSelected,'Party setup must reach the live turn settings selector.');
              boardSelectionConfirmed=true;continue;
            }
            if(boardSelectionConfirmed&&!turnLengthSelected){
              const setupStates=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState())));
              const owner=setupMenuOwnerIndex(setupStates,setupPort);
              await pages[owner].locator('#canvas').screenshot({path:path.join(outputDirectory,'browser-party-setup-before-turn-selection-peer'+owner+'.png')});
              const waitForPostInputWindow=states=>Promise.all(states.map((state,index)=>{
                const targetFrame=Number(state.frame)+30;
                assert.ok(Number.isInteger(targetFrame),'Party setup input requires a live simulation-frame target.');
                return pages[index].waitForFunction(frame=>{
                  const current=partyBoardApp.lobbyGameState()?.frame;
                  return Number.isInteger(current)&&current>=frame;
                },targetFrame,{timeout:10000});
              }));
              const setupActionHistory=[];
              const recordSetupAction=entry=>{
                setupActionHistory.push({at:new Date().toISOString(),...entry});
                fs.writeFileSync(path.join(outputDirectory,'browser-party-setup-actions.json'),JSON.stringify(setupActionHistory,null,2)+'\n');
                console.log('Party setup action attempt: '+JSON.stringify(entry));
              };
              // Drive one observed cursor/turn change at a time, so menu transitions
              // or delayed synchronized input cannot consume a blind key sequence.
              for(let action=0;action<24;action++){
                const states=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState())));
                const decision=turnSetupAction(states,requestedFullMatchTurns);
                if(!decision)throw Error('Party turn settings telemetry disappeared during selection.');
                if(decision.selected){
                  if(requestedFullMatchTurns===5)
                    await pages[decision.owner].locator('#canvas').screenshot({path:path.join(outputDirectory,'browser-party-setup-5-turn-peer'+decision.owner+'.png')});
                  // Message updates may reset HuWin's key queue. Wait for live
                  // simulation frames after the last value change before moving
                  // to the next row, especially on slower four-peer runtimes.
                  await waitForPostInputWindow(states);
                  let row=decision.row,turns=decision.turns;
                  while(row!==4){
                    const key=cursorKey(row,4);
                    const updated=await retryObservedSetupAction({owner:decision.owner,row,turns,port:decision.port,key,kind:'cursor'}, {
                      press:(owner,action)=>pressKeyboard(pages[owner],action,{delay:120}),
                      waitForChange:async expected=>{
                        const outcomes=await Promise.allSettled(pages.map(page=>page.waitForFunction(value=>{
                          const progress=partyBoardApp.lobbyGameState()?.progress;
                          return progress?.partySetupPort===value.port&&
                            (progress.partySetupRow!==value.row||progress.partySetupTurns!==value.turns);
                        },expected,{timeout:5000})));
                        return outcomes.every(outcome=>outcome.status==='fulfilled');
                      },
                      readStates:()=>Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState()))),
                      waitFrames:waitForPostInputWindow,onAttempt:recordSetupAction,maxAttempts:3,
                    });
                    const progress=updated.map(state=>state.progress);
                    assert.ok(progress.every(value=>value.partySetupPort===decision.port),'Party setup controller ownership must remain synchronized.');
                    row=progress[0].partySetupRow;turns=progress[0].partySetupTurns;
                  }
                  await pressKeyboard(pages[decision.owner],'z',{delay:120});
                  let promptStates=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState())));
                  await waitForPostInputWindow(promptStates);
                  await pages[decision.owner].locator('#canvas').screenshot({path:path.join(outputDirectory,'browser-party-settings-confirm-prompt-peer'+decision.owner+'.png')});
                  console.log('Party settings confirmation prompt state: '+JSON.stringify(await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState()?.progress)))));
                  await pressKeyboard(pages[decision.owner],'z',{delay:120}); // Confirm Yes on "Are these settings OK?"
                  promptStates=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState())));
                  await waitForPostInputWindow(promptStates);
                  await pages[decision.owner].locator('#canvas').screenshot({path:path.join(outputDirectory,'browser-party-settings-after-confirm-peer'+decision.owner+'.png')});
                  console.log('Party settings post-confirm state: '+JSON.stringify(await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState()?.progress)))));
                  turnLengthSelected=true;break;
                }
                await pressKeyboard(pages[decision.owner],decision.key,{delay:120});
                // Advance past the game's post-message key-reset window before
                // observing the next setup row/value. This is the same frame
                // barrier used for value changes and avoids relying on a
                // nonexistent helper or sending a second key speculatively.
                await waitForPostInputWindow(states);
              }
              assert.ok(turnLengthSelected,'Actual Party setup must expose the requested '+requestedFullMatchTurns+'-turn value.');
              continue;
            }
            await host.waitForTimeout(250);
          }
          assert.ok(turnLengthSelected,'Actual Party setup must confirm the requested '+requestedFullMatchTurns+'-turn length before match start.');
          for(let index=0;index<pages.length;index++){
            const page=pages[index],frame=await page.evaluate(()=>partyBoardApp.lobbyGameState().input.frame);
            await page.waitForFunction(frame=>BigInt(partyBoardApp.lobbyGameState().input.frame)>=BigInt(frame)+60n,frame);
            await page.locator('#canvas').screenshot({path:path.join(outputDirectory,'browser-party-board'+selectedOverlay+'-peer'+index+'.png')});
          }
          const boardRecords=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyInputObservations())));
          const boardCommon=boardRecords[0].filter(record=>boardRecords.every(records=>records.some(other=>other.frame===record.frame)));
          assert.ok(boardCommon.length>=30,'Actual board input-state comparison required.');
          for(const record of boardCommon)for(const records of boardRecords)assert.deepEqual(records.find(other=>other.frame===record.frame),record);
          console.log('Actual production Party setup reached board'+selectedOverlay+' on every peer; matching board polls: '+boardCommon.length);
          const boardProgress=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState()?.progress)));
          for(const progress of boardProgress){
            assert.ok(progress&&Number.isInteger(progress.turn)&&progress.turn>=1,
              'Actual board progression getter must expose the current turn.');
            assert.equal(progress.maxTurn,requestedFullMatchTurns,'Actual board must use the requested configured turn count.');
            assert.ok(Number.isInteger(progress.maxTurn)&&progress.maxTurn>=5&&progress.turn<=progress.maxTurn,
              'Validate an ordinary configured board length.');
            assert.equal(progress.overlay,selectedOverlay,'Board progression must identify the actual board overlay.');
            assert.equal(progress.board,selectedBoard,'Ordinary menu must select the requested board.');
          }
          assert.ok(boardProgress.every(progress=>progress.maxTurn===boardProgress[0].maxTurn&&progress.board===boardProgress[0].board),
            'Every peer must use the same board and configured match length.');
          console.log('Actual board progression: '+JSON.stringify(boardProgress));
          if(process.env.PARTYBOARD_TEST_GAME_FULL_MATCH==='1'){
            const inventory=JSON.parse(fs.readFileSync(require('node:path').resolve(__dirname,'../../../outputs/validation/browser-usa-scene-inventory.json'),'utf8'));
            // Enforce the declared match lifetime outside browser operations:
            // a stuck input/CDP promise must still enter failure cleanup.
            let matchTimer;
            try{
              await Promise.race([
                require('../tools/drive-production-full-match.cjs').driveProductionFullMatch({pages,inventory,outputDirectory,maxTurn:requestedFullMatchTurns,timeoutMs:requestedFullMatchTimeoutMs}),
                new Promise((_,reject)=>{matchTimer=setTimeout(()=>reject(Error('Actual full-match test exceeded its configured '+Math.ceil(fullMatchHarnessTimeoutMs/60000)+'-minute harness lifetime.')),fullMatchHarnessTimeoutMs);}),
              ]);
            }finally{clearTimeout(matchTimer);}
            completedFullMatchProgress=true;
          }
          if(process.env.PARTYBOARD_TEST_GAME_PARTY_MINIGAME==='1'&&process.env.PARTYBOARD_TEST_GAME_FULL_MATCH!=='1'){
            const inventory=JSON.parse(fs.readFileSync(require('node:path').resolve(__dirname,'../../../outputs/validation/browser-usa-scene-inventory.json'),'utf8'));
            const minigames=new Set(inventory.overlays.filter(scene=>/^m\d+DLL$/i.test(scene.name)).map(scene=>scene.overlay));
            assert.ok(minigames.size>0,'Authoritative USA minigame overlays required.');
            const instructions=inventory.overlays.find(scene=>scene.name.toLowerCase()==='instdll');
            assert.ok(instructions,'Authoritative instructions overlay required.');
            const inInstructions=markers=>markers.map(text=>/Start New OVL (\d+) /.exec(text)).filter(Boolean).at(-1)?.[1]===String(instructions.overlay);
            const visited=markers=>markers.map(text=>Number(/Start New OVL (\d+) /.exec(text)?.[1])).filter(overlay=>minigames.has(overlay));
            const deadline=Date.now()+480000;let direction=0;
            while(!gameMarkers.every(markers=>visited(markers).length)){
              assert.ok(Date.now()<deadline,'Normal Party turns must reach a minigame on every peer.');
              if(gameMarkers.every(inInstructions)){
                // Instruction screens require PAD Start, not A or a route choice.
                await pressKeyboard(host,'Enter',{delay:250});await host.waitForTimeout(1000);continue;
              }
              await Promise.all(pages.map(page=>pressKeyboard(page,'z',{delay:120})));
              // Real arrow-key events map to the left analog stick. Board junctions
              // inspect HuPadStkX/Y and cannot be navigated by A alone.
              await Promise.all(pages.map(page=>pressKeyboard(page,['w','d','s','a'][direction%4],{delay:250})));direction++;
              await host.waitForTimeout(1000);
            }
            const selections=gameMarkers.map(markers=>visited(markers).at(-1));
            assert.ok(selections.every(overlay=>overlay===selections[0]),'Every peer must select the same actual minigame.');
            const entryFrames=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState().input.frame)));
            await Promise.all(pages.map((page,index)=>page.waitForFunction(frame=>{const state=partyBoardApp.lobbyGameState();return state?.input.state==='Running'&&BigInt(state.input.frame)>=BigInt(frame)+60n;},entryFrames[index],{timeout:30000})));
            for(let index=0;index<pages.length;index++)await pages[index].locator('#canvas').screenshot({path:path.join(outputDirectory,'browser-party-minigame-peer'+index+'.png')});
            const records=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyInputObservations())));
            const common=records[0].filter(record=>records.every(peer=>peer.some(other=>other.frame===record.frame)));
            assert.ok(common.length>=30,'Thirty matching polls at natural Party minigame entry required.');
            for(const record of common)for(const peer of records)assert.deepEqual(peer.find(other=>other.frame===record.frame),record);
            console.log('Natural production Party turns reached minigame overlay '+selections[0]+'; matching polls: '+common.length+'. Minigame completion and full match remain unverified.');
            if(process.env.PARTYBOARD_TEST_GAME_PARTY_CYCLE==='1'){
              const result=inventory.overlays.find(scene=>scene.name.toLowerCase()==='resultdll');
              assert.ok(result,'Authoritative result overlay required.');
              const offsets=gameMarkers.map(markers=>markers.length),cycleDeadline=Date.now()+480000;
              const finished=(markers,index)=>{
                const overlays=markers.slice(offsets[index]).map(text=>Number(/Start New OVL (\d+) /.exec(text)?.[1]));
                const resultIndex=overlays.indexOf(result.overlay);
                return resultIndex>=0&&overlays.slice(resultIndex+1).includes(selectedOverlay);
              };
              while(!gameMarkers.every(finished)){
                assert.ok(Date.now()<cycleDeadline,'Actual Party minigame must reach results and return to the board on every peer.');
                await Promise.all(pages.map(page=>pressKeyboard(page,'z',{delay:120})));
                await Promise.all(pages.map(page=>pressKeyboard(page,['w','d','s','a'][direction%4],{delay:250})));direction++;
                await host.waitForTimeout(1000);
              }
              // A board-load marker can precede its wipe and first rendered
              // frame. Require new completed polls after that transition;
              // records from the preceding minigame cannot satisfy this gate.
              const returnStates=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState())));
              const returnFrame=returnStates.reduce((latest,state)=>BigInt(state.input.frame)>latest?BigInt(state.input.frame):latest,0n);
              for(const page of pages)await page.waitForFunction(frame=>{
                const state=partyBoardApp.lobbyGameState();
                return state?.input.state==='Running'&&BigInt(state.input.frame)>=BigInt(frame)+180n;
              },String(returnFrame),{timeout:60000});
              for(let index=0;index<pages.length;index++)await pages[index].locator('#canvas').screenshot({path:path.join(outputDirectory,'browser-party-return-peer'+index+'.png')});
              const cycleRecords=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyInputObservations())));
              const cycleCommon=cycleRecords[0].filter(record=>BigInt(record.frame)>=returnFrame&&cycleRecords.every(peer=>peer.some(other=>other.frame===record.frame)));
              assert.ok(cycleCommon.length>=30,'Actual results-to-board matching state required.');
              for(const record of cycleCommon)for(const peer of cycleRecords)assert.deepEqual(peer.find(other=>other.frame===record.frame),record);
              completedNaturalCycle=true;
              console.log('Actual natural Party minigame/results/board return passed; matching polls: '+cycleCommon.length+'. Full match remains unverified.');
            }
          }
        }
      }
      if(process.env.PARTYBOARD_TEST_GAME_KEYBOARD==='1'){
        // Production Z maps to PAD A (0x100); each peer's local lane zero
        // must reach only its assigned global slot through the real C hook.
        for(let index=0;index<pages.length;index++){
          const page=pages[index],start=await page.evaluate(()=>partyBoardApp.lobbyGameState().input.frame);
          await page.locator('#canvas').click();await page.keyboard.down('z');
          try{await page.waitForFunction(frame=>BigInt(partyBoardApp.lobbyGameState().input.frame)>=BigInt(frame)+12n,start);}
          finally{await page.keyboard.up('z');}
          const records=await Promise.all(pages.map(peer=>peer.evaluate(()=>partyBoardApp.lobbyInputObservations())));
          const active=records[index].filter(record=>BigInt(record.frame)>=BigInt(start)&&(record.pads[index*11+1]&1));
          assert.ok(active.length>=3,'Actual keyboard A must reach assigned game slot.');
          for(const record of active)for(const peerRecords of records){
            assert.deepEqual(peerRecords.find(other=>other.frame===record.frame),record);
            for(let slot=0;slot<4;slot++)if(slot!==index)assert.equal(record.pads[slot*11+1]&1,0,'Keyboard must not control another player.');
          }
        }
        console.log('Actual keyboard A propagated through each assigned global player slot.');
      }
      if(process.env.PARTYBOARD_TEST_GAME_RECONNECT==='1'){
        const originalConfigurations=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbySessionConfiguration())));
        const originalBuilds=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyRuntimeState().buildId)));
        const guest=pages[count-1],disconnect=count===2?host:guest;
        if(process.env.PARTYBOARD_TEST_ONE_POLL_RECOVERY==='1'){
          // Drop one real outgoing guest input, then keep that route silent until
          // disconnection. Do not alter game counters, pads, RNG or checkpoints.
          await guest.evaluate(()=>{
            const original=RTCDataChannel.prototype.send;
            globalThis.testDroppedPoll={frame:null,dropped:0};
            globalThis.testRestoreInputSend=()=>{RTCDataChannel.prototype.send=original;};
            RTCDataChannel.prototype.send=function(data){
              const bytes=data instanceof ArrayBuffer?new Uint8Array(data):ArrayBuffer.isView(data)?new Uint8Array(data.buffer,data.byteOffset,data.byteLength):null;
              if(this.label==='partyboard-input-v1'&&bytes?.length===48&&bytes[0]===80&&bytes[1]===66&&bytes[2]===73&&bytes[3]===80){
                if(testDroppedPoll.frame===null)testDroppedPoll.frame=new DataView(bytes.buffer,bytes.byteOffset,bytes.byteLength).getBigUint64(24,true).toString();
                testDroppedPoll.dropped++;return;
              }
              return original.call(this,data);
            };
          });
          await guest.waitForFunction(()=>{const dropped=globalThis.testDroppedPoll,state=partyBoardApp.lobbyGameState();
            return dropped?.frame!==null&&state?.input.frame===(BigInt(dropped.frame)+1n).toString()&&state.pending!==null;
          },{},{timeout:10000});
          console.log('Injected actual outgoing input loss: '+JSON.stringify(await guest.evaluate(()=>testDroppedPoll)));
          await guest.evaluate(()=>{const connection=testPeerConnections.find(peer=>peer.connectionState==='connected');if(!connection)throw Error('No actual connected transport.');connection.close();testRestoreInputSend();});
        }else await disconnect.evaluate(()=>{const connection=testPeerConnections.find(peer=>peer.connectionState==='connected');if(!connection)throw Error('No actual connected transport.');connection.close();});
        for(const page of pages)await page.waitForFunction(()=>partyBoardApp.lobbyGameState()?.input.state==='Paused');
        const paused=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState())));console.log('Actual disconnected paused states: '+JSON.stringify(paused));
        if(process.env.PARTYBOARD_TEST_ONE_POLL_RECOVERY==='1'){
          assert.equal(BigInt(paused[count-1].input.frame),BigInt(paused[0].input.frame)+1n,'Injected loss must produce one actual pending poll of skew.');
          for(const state of paused.slice(0,-1))assert.equal(state.input.frame,paused[0].input.frame,'Healthy routes must retain the lagging actual pending poll.');
          assert.ok(paused.every(state=>state.pending!==null));
        }
        for(const page of pages)await page.locator('#recovery-panel').waitFor({state:'visible'});
        const reconnectDelay=Number(process.env.PARTYBOARD_TEST_RECONNECT_DELAY_MS||0);
        assert.ok(Number.isSafeInteger(reconnectDelay)&&reconnectDelay>=0&&reconnectDelay<=55000,
          'Reconnect test delay must be an integer from 0 through 55000 milliseconds.');
        if(reconnectDelay){
          await host.waitForTimeout(reconnectDelay);
          const countdowns=await Promise.all(pages.map(page=>page.locator('#recovery-countdown').textContent()));
          const seconds=countdowns.map(text=>Number(text.match(/^(\d+) seconds remaining\.$/)?.[1]));
          const expectedSeconds=Math.ceil((60000-reconnectDelay)/1000);
          assert.ok(seconds.every(value=>Number.isInteger(value)&&value>0&&value<=expectedSeconds+1)&&seconds.some(value=>value<=expectedSeconds),
            'Every peer must still display positive recovery time; allow at most one second of timer-display lag: '+JSON.stringify(countdowns));
          console.log('Near-deadline reconnect countdown verified after '+reconnectDelay+' ms: '+JSON.stringify(countdowns));
        }
        await host.locator('#recovery-host').click();await host.waitForFunction(()=>document.querySelector('#recovery-local').value.length>0);
        await guest.locator('#recovery-remote').fill(await host.locator('#recovery-local').inputValue());await guest.locator('#recovery-join').click();
        await guest.waitForFunction(()=>document.querySelector('#recovery-local').value.length>0);
        await host.locator('#recovery-remote').fill(await guest.locator('#recovery-local').inputValue());await host.locator('#recovery-answer').click();
        // Healthy peers keep their original routes; only the disconnected pair
        // exchanges replacement credentials and paused-state receipts.
        for(const page of [host,guest])await page.waitForFunction(()=>document.querySelector('#recovery-status').textContent.startsWith('Connection and paused state verified.'),{},{timeout:30000});
        await host.locator('#recovery-resume').click();
        for(let index=0;index<pages.length;index++)await pages[index].waitForFunction(frame=>{const state=partyBoardApp.lobbyGameState();return state?.input.state==='Running'&&BigInt(state.input.frame)>=BigInt(frame)+60n;},paused[index].input.frame,{timeout:30000});
        const recovered=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyInputObservations())));
        const common=recovered[0].filter(record=>BigInt(record.frame)>=BigInt(paused[0].input.frame)&&recovered.every(records=>records.some(other=>other.frame===record.frame)));
        assert.ok(common.length>=30,'Thirty matching completed polls after reconnect required.');
        for(const record of common)for(const records of recovered)assert.deepEqual(records.find(other=>other.frame===record.frame),record);
        for(let index=0;index<pages.length;index++){
          assert.deepEqual(await pages[index].evaluate(()=>partyBoardApp.lobbySessionConfiguration()),originalConfigurations[index],
            'Replacement must preserve the original immutable configuration.');
          assert.equal(await pages[index].evaluate(()=>partyBoardApp.lobbyRuntimeState().buildId),originalBuilds[index],
            'Replacement must retain the originally measured runtime package.');
        }
        console.log('Actual signed UI game reconnect resumed original pending poll; matching recovered polls: '+common.length);
      }
    }
  }
  console.log(JSON.stringify({reports}));await host.locator('#stop').click();await host.waitForFunction(()=>/^(Session stopped\.|Online session ended\.)/.test(document.querySelector('#status').textContent));
  const runningSession=process.env.PARTYBOARD_TEST_COMMITTED_GAME==='1';
  await Promise.all(pages.slice(1).map(guest=>guest.waitForFunction(running=>document.querySelector('#status').textContent.startsWith(running?'Online session ended.':'Connection closed.'),runningSession,{timeout:runningSession?90000:30000})));
  for(const page of pages)assert.deepEqual(await page.evaluate(async()=>(await import('/app.mjs')).lobbyPreparedReports()),{});
  if(process.env.PARTYBOARD_LOBBY_PIPELINE_CAPTURE_OUTPUT){
    for(let index=0;index<pages.length;index++){
      const captured=await pages[index].evaluate(async()=>{
        const seed=partyBoardApp.lastCapturedLobbyPipelineSeed();
        if(!seed)throw Error('Lobby shutdown did not capture its closed pipeline database.');
        return {sha256:seed.sha256,scope:seed.scope,bytes:Array.from(new Uint8Array(await seed.blob.arrayBuffer()))};
      });
      assert.equal(captured.scope,'observed-scenes-only');
      const bytes=Buffer.from(captured.bytes);
      assert.equal(require('node:crypto').createHash('sha256').update(bytes).digest('hex'),captured.sha256);
      const target=process.env.PARTYBOARD_LOBBY_PIPELINE_CAPTURE_OUTPUT+'-peer'+index+'.db';
      fs.writeFileSync(target,bytes,{flag:'wx'});
      fs.writeFileSync(target+'.json',JSON.stringify({scope:captured.scope,fullCoverage:false,
        sha256:captured.sha256,bytes:bytes.length,preparedReport:reports[index===0?'host':'guest'+index],
        observedOverlayMarkers:gameMarkers[index].filter(text=>text.includes('Start New OVL ')),
        completedNaturalCycle,completedFullMatchProgress,renderedRankingsVerified:false},null,2)+'\n',{flag:'wx'});
      console.log('Captured closed observed lobby shader definitions: '+target);
    }
  }
  const fullMatchPassed=process.env.PARTYBOARD_TEST_GAME_FULL_MATCH==='1'&&completedFullMatchProgress;
  assert.deepEqual(errors,[]);console.log('PASS '+count+' actual production lobby runtimes: private asset verification and matching prepared reports'+(runningSession?'; committed start, at least 120 input polls, matching completed poll state and original disconnect deadline cleanup.':' and cleanup.')+(process.env.PARTYBOARD_TEST_GAME_RECONNECT==='1'?' Signed UI reconnect and matching resumed polls passed.':' Successful game reconnect not exercised.')+(fullMatchPassed?' Configured full-match progression reached awards/rankings; inspect retained ranking captures.':' Full-match progression not exercised.')+' Full shader coverage remains unverified.');
 }catch(error){
  failed=true;
  console.error('Actual peer validation failed: '+String(error));
  fs.writeFileSync(path.join(outputDirectory,'browser-production-peer-page-errors.json'),JSON.stringify(pageErrors,null,2)+'\n');
  fs.writeFileSync(path.join(outputDirectory,'browser-production-peer-console-errors.json'),JSON.stringify(consoleErrors,null,2)+'\n');
  fs.writeFileSync(path.join(outputDirectory,'browser-production-peer-runtime-warnings.json'),JSON.stringify(runtimeWarnings,null,2)+'\n');
  fs.writeFileSync(path.join(outputDirectory,'browser-production-peer-overlay-markers.json'),JSON.stringify(gameMarkers,null,2)+'\n');
  const heapFailures=await Promise.allSettled(pages.map(page=>observe(page,()=>globalThis.partyBoardApp?.lobbyHeapFailure())));
  fs.writeFileSync(path.join(outputDirectory,'browser-production-peer-heap-failures.json'),JSON.stringify(serializeSettledObservations(heapFailures),null,2)+'\n');
  const rewinds=await Promise.allSettled(pages.map(page=>observe(page,()=>globalThis.partyBoardApp?.lobbyRewindDiagnostics())));
  fs.writeFileSync(path.join(outputDirectory,'browser-production-peer-rewinds.json'),JSON.stringify(serializeSettledObservations(rewinds),null,2)+'\n');
  const stacks=await Promise.all(pages.map(page=>require('../tools/capture-browser-stacks.cjs').captureBrowserStack(page)));
  fs.writeFileSync(path.join(outputDirectory,'browser-production-peer-failure-stacks.json'),JSON.stringify(stacks,null,2)+'\n');
  fs.writeFileSync(path.join(outputDirectory,'browser-production-peer-runtime-warnings.json'),JSON.stringify(runtimeWarnings,null,2)+'\n');
  const diagnostics=await Promise.allSettled(pages.map(page=>observe(page,()=>({status:document.querySelector('#status')?.textContent,
    recoveryStatus:document.querySelector('#recovery-status')?.textContent,countdown:document.querySelector('#recovery-countdown')?.textContent,
    pageLifecycle:globalThis.testPeerLifecycleEvents??[],
    game:globalThis.partyBoardApp?.lobbyGameState(),transport:globalThis.partyBoardApp?.lobbyTransportDiagnostics(),
    inputTiming:globalThis.partyBoardApp?.lobbyInputTiming?.(),
    activeFrameStage:globalThis.partyBoardActiveFrameStage??null,rendererBeginStage:globalThis.partyBoardRendererBeginStage??null,stagingWait:globalThis.partyBoardStagingWait??null,
    framePacingTiming:globalThis.partyBoardFramePacingTiming??null,
    frameStageTiming:globalThis.partyBoardFrameStageTiming??null,
    rendererStageTiming:globalThis.partyBoardRendererStageTiming??null,
    rendererSubmissionTiming:globalThis.partyBoardRendererSubmissionTiming??null,
    rendererStagingTiming:globalThis.partyBoardRendererStagingTiming??null,
    audio:(()=>{const audio=document.querySelector('#lobby-audio');return audio?{src:audio.currentSrc,time:audio.currentTime,duration:audio.duration,
      paused:audio.paused,ended:audio.ended,seeking:audio.seeking,readyState:audio.readyState,networkState:audio.networkState,
      error:audio.error?{code:audio.error.code,message:audio.error.message}:null}:null;})(),
    rtcEvents:globalThis.testPeerConnectionEvents??[],connections:(globalThis.testPeerConnections||[]).map(peer=>({connection:peer.connectionState,ice:peer.iceConnectionState,gathering:peer.iceGatheringState,candidates:peer.testIceCandidates??[],localCandidateCount:(peer.localDescription?.sdp.match(/^a=candidate:/gm)||[]).length,remoteCandidateCount:(peer.remoteDescription?.sdp.match(/^a=candidate:/gm)||[]).length}))}))));
  fs.writeFileSync(path.join(outputDirectory,'browser-production-peer-failure-diagnostics.json'),JSON.stringify(serializeSettledObservations(diagnostics),null,2)+'\n');
  console.error('Actual peer failure diagnostics: '+JSON.stringify(serializeSettledObservations(diagnostics)));
  console.error('Actual peer failure overlay markers: '+JSON.stringify(gameMarkers));
  const captures=await Promise.allSettled(pages.map((page,index)=>page.screenshot({path:path.join(outputDirectory,'browser-production-peer-failure-'+index+'.png'),timeout:10000})));
  console.error('Actual peer failure screenshots: '+JSON.stringify(captures.map((capture,index)=>({peer:index,status:capture.status,error:capture.status==='rejected'?String(capture.reason):undefined}))));
  if(process.env.PARTYBOARD_LOBBY_PIPELINE_CAPTURE_OUTPUT){
    try{await require('../tools/retain-failed-lobby-pipelines.cjs').retainFailedLobbyPipelines(pages,outputDirectory);}
    catch(captureError){console.error('Failure pipeline retention unavailable: '+String(captureError));}
  }
  throw error;
 }finally{
  clearInterval(timer);
  const cleanupTask=(async()=>{if(failed)await browserServer?.kill();else{await browser?.close();await browserServer?.close();}})();
  const cleanupCompleted=await Promise.race([
    cleanupTask.then(()=>true,error=>{console.error('Browser cleanup failed: '+String(error));return true;}),
    new Promise(resolve=>setTimeout(()=>resolve(false),30000))
  ]);
  if(!cleanupCompleted){
    console.error('Browser cleanup exceeded 30 seconds; stopping the owned browser process.');
    try{browserServer?.process()?.kill('SIGKILL');}catch(error){console.error('Owned browser process stop failed: '+String(error));}
  }
  server.kill();releaseInspection();
  if(!cleanupCompleted)process.exit(failed?1:0);
 }
})().catch(e=>{console.error(e);process.exitCode=1;});



