// Explicit local-file diagnostic: actual preparation, no simulation resume.
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const http=require('node:http'),fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
if(!process.argv[2]||!process.env.PARTYBOARD_LOCAL_CISO)throw Error('Runtime directory and explicit local game file required');
const files=new Map(['pipeline-seed.mjs','browser-runtime-package.mjs','browser-platform.mjs','session-runtime.mjs','browser-input-exchange.mjs','runtime-mount.mjs','runtime-asset.mjs','verified-asset-file.mjs','transfer.mjs','async-blob-fs.mjs','engine-lifecycle.mjs','pipeline-drain.mjs','runtime-build.mjs']
  .map(name=>['/'+name,path.join(__dirname,name)]));
for(const name of ['contract.mjs','contract.wasm'])files.set('/'+name,path.resolve(process.argv[2],name));
for(const name of ['start-client.mjs','start-barrier.mjs','start-host.mjs','start-channels.mjs','start-commit-channel.mjs','start-preparation-channel.mjs'])files.set('/'+name,path.join(__dirname,name));
files.set('/online-input-gate.mjs',path.join(__dirname,'online-input-gate.mjs'));
for(const name of ['session-input.mjs','session-input-exchange.mjs','lockstep.mjs','reconnect-policy.mjs','input-packet.mjs'])files.set('/'+name,path.join(__dirname,name));
for(const name of ['realtime-input-recovery.mjs','suspended-input-recovery.mjs'])files.set('/'+name,path.join(__dirname,name));
// Serve current local module dependencies by exact map lookup; manually frozen
// route lists otherwise fail whenever a production module adds an import.
for(const name of fs.readdirSync(__dirname))if(name.endsWith('.mjs')&&!name.endsWith('.test.mjs'))files.set('/'+name,path.join(__dirname,name));
const server=http.createServer((req,res)=>{
  if(req.method==='GET'&&req.url==='/'){res.end('<!doctype html><input type="file"><canvas id="canvas" width="640" height="480"></canvas>');return;}
  const file=req.method==='GET'&&files.get(req.url);if(!file){console.error('Missing diagnostic route: '+req.url);res.writeHead(404);res.end();return;}
  res.setHeader('Content-Type',file.endsWith('.wasm')?'application/wasm':'text/javascript');
  fs.createReadStream(file).on('error',error=>{console.error('Diagnostic file read failed: '+file+' '+error.message);res.statusCode=500;res.end();}).pipe(res);
});
(async()=>{
  let browser;
  try {
    await new Promise((resolve,reject)=>{server.once('error',reject);server.listen(0,'127.0.0.1',resolve);});
    // Explicit same-machine diagnostic only; production browser launch is unchanged.
    const localIceDiagnostic=process.env.PARTYBOARD_TEST_LOCAL_ICE_NO_MDNS==='1';
    browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio',...(localIceDiagnostic?['--disable-features=WebRtcHideLocalIpsWithMdns']:[])]});const page=await browser.newPage(),errors=[];
    page.on('pageerror',cause=>errors.push(cause.message));
    let lastDiagnosticStage='browser-ready';
    await page.exposeFunction('partyBoardDiagnosticStage',stage=>{lastDiagnosticStage=stage;console.log('Session diagnostic stage: '+stage);});
    if(process.env.PARTYBOARD_TRACE_STATE_DIAGNOSTICS==='1'){
      await page.addInitScript(()=>{globalThis.testTraceState=true;});
      page.on('console',message=>{const value=message.text();if(value.includes('State snapshot:')||value.includes('Global state snapshot:')||value.includes('Prepared engine RNG:'))console.log(value);});
    }
    await page.goto(`http://127.0.0.1:${server.address().port}/`);
    await page.locator('input').setInputFiles(process.env.PARTYBOARD_LOCAL_CISO);
    const mapping=process.env.PARTYBOARD_TEST_LOCAL_PAD_MAPPING==='1';
    const customKeyboard=process.env.PARTYBOARD_TEST_CUSTOM_KEYBOARD==='1';
    if(customKeyboard&&!mapping)throw Error('Custom keyboard fixture requires owned keyboard mapping');
    const extendedKeyboard=process.env.PARTYBOARD_TEST_EXTENDED_KEYBOARD==='1';
    if(extendedKeyboard&&!mapping)throw Error('Extended keyboard fixture requires owned keyboard mapping');
    await page.exposeFunction('partyBoardInjectLocalKey',async()=>{
      for(const key of customKeyboard?['Space']:extendedKeyboard?['z','q','e','l','i','ArrowUp']:['z'])await page.keyboard.down(key);
    });
    const platform=process.env.PARTYBOARD_TEST_SESSION_PLATFORM==='1';
    const glyphLifetime=process.env.PARTYBOARD_TEST_GLYPH_LIFETIME==='1';
    const standalone=process.env.PARTYBOARD_TEST_STANDALONE_RUNTIME==='1';
    if(standalone&&!platform)throw Error('Standalone runtime fixture requires reusable platform');
    const rtcCommit=process.env.PARTYBOARD_TEST_SESSION_RTC_COMMIT==='1';
    const ownership=process.env.PARTYBOARD_TEST_SESSION_OWNERSHIP==='1';
    const recoveryDeadline=process.env.PARTYBOARD_TEST_SESSION_RECOVERY==='1';
    const reconnect=process.env.PARTYBOARD_TEST_SESSION_RECONNECT==='1';
    if(reconnect&&!recoveryDeadline)throw Error('Reconnect fixture requires recovery mode');
    if(recoveryDeadline&&!ownership)throw Error('Recovery fixture requires owned input mode');
    const inputExchange=ownership||process.env.PARTYBOARD_TEST_SESSION_INPUT==='1';
    let expectedFrames=inputExchange?4:5;
    if(mapping&&(!platform||!ownership))throw Error('Keyboard mapping fixture requires reusable platform and owned input');
    if(ownership&&!rtcCommit)throw Error('Ownership fixture requires RTC commit mode');
    const runCommit=rtcCommit||process.env.PARTYBOARD_TEST_SESSION_COMMIT==='1';
    const neutralProbe=process.env.PARTYBOARD_TEST_NEUTRAL_FRAME_PROBE==='1';
    const probeButtons=process.env.PARTYBOARD_TEST_PROBE_BUTTONS==='1';
    if(probeButtons&&!neutralProbe)throw Error('Button probe requires bounded logical-port probe');
    if(neutralProbe&&(!platform||!standalone||!runCommit||!inputExchange||ownership))throw Error('Neutral frame probe requires standalone platform bounded echo execution');
    if(process.env.PARTYBOARD_TEST_PROBE_FRAMES){
      expectedFrames=Number(process.env.PARTYBOARD_TEST_PROBE_FRAMES);
      if(!neutralProbe||!Number.isInteger(expectedFrames)||expectedFrames<1||expectedFrames>1800)throw Error('Probe count requires neutral probe and 1..1800 frames');
    }
    const cancelRenderer=process.env.PARTYBOARD_TEST_SESSION_ABORT==='1';
    const cancelInitialize=process.env.PARTYBOARD_TEST_SESSION_ABORT_INITIALIZE==='1';
    const cancellation=cancelRenderer||cancelInitialize;
    if(platform&&runCommit&&!inputExchange)throw Error('Reusable platform bounded execution requires owned or echo input');
    if((cancellation&&runCommit)||(cancelRenderer&&cancelInitialize))throw Error('Choose one cancellation or committed execution mode');
    const resolution=process.env.PARTYBOARD_TEST_GUEST_RESOLUTION?JSON.parse(process.env.PARTYBOARD_TEST_GUEST_RESOLUTION):undefined;
    const execution=page.evaluate(async({resolution,mapping,customKeyboard,extendedKeyboard,glyphLifetime,standalone,platform,runCommit,rtcCommit,cancelRenderer,cancelInitialize,cancellation,inputExchange,ownership,recoveryDeadline,reconnect,neutralProbe,expectedFrames,probeButtons})=>{
      const {prepare,browserStore,CHUNK}=await import('/transfer.mjs');
      const {browserPlatformCallbacks}=await import('/browser-platform.mjs');
      const platformCallbacks=browserPlatformCallbacks({resolution});
      const {prepareFingerprintedSessionRuntime}=await import('/session-runtime.mjs');
      const {drainIdleEnginePipelines}=await import('/pipeline-drain.mjs');
      const {prepareEngineLifecycle}=await import('/engine-lifecycle.mjs');
      const {SessionInput}=await import('/session-input.mjs');
      const {createSessionInputExchange}=await import('/session-input-exchange.mjs');
      const {encodeInput,decodeInput}=await import('/input-packet.mjs');
      const {createRealtimeInputRecovery}=await import('/realtime-input-recovery.mjs');
      let inputChannel,ownedExchange,inputQueue,inputSent=0,inputReceived=0,mappedKeyboardPackets=0;
      let recoveryClock=0,recoveryResult;
      const inputNow=()=>recoveryDeadline?recoveryClock:performance.now();
      const waitExecution=async execution=>{
        let timer;
        try {await Promise.race([execution,new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error('Bounded game execution timed out')),neutralProbe?180000:10000);})]);}
        finally {clearTimeout(timer);}
      };
      const inputSession=new Uint8Array(16).fill(1);
      if(ownership){
        inputQueue=new SessionInput(inputSession,[1,2,1,2]);
        ownedExchange=createSessionInputExchange(inputQueue,{session:inputSession,owners:[1,2,1,2],peer:2,localPadSlots:mapping?[null,0,null,1]:undefined,
          checkpoint:options=>options.checkpoint,suspendOnPause:recoveryDeadline,now:inputNow,
          send:packet=>{inputSent++;inputChannel.send(packet);}});
      }
      const {loadBrowserRuntimePackage}=await import('/browser-runtime-package.mjs');
      const artifacts=await loadBrowserRuntimePackage({baseURL:location.origin+'/'});
      const file=document.querySelector('input').files[0],asset=await prepare(file),store=await browserStore();
      await globalThis.partyBoardDiagnosticStage('local-assets-hashed');
      let runtime,module,buildId,inputSamples=0,terminalInput,terminalUnchanged=false;
      const frameCount=()=>platform?module._PartyBoard_GameFrameCount():module._PartyBoardTestExternalFrames();
      const verifyTerminal=()=>{
        if(!terminalInput)return;
        const actual=[0,1,2,3].map(field=>module._PartyBoard_GamePausedState(field));
        const expected=[terminalInput.frame,terminalInput.checkpoint,terminalInput.frand,terminalInput.rand8];
        if(JSON.stringify(actual)!==JSON.stringify(expected))throw Error('Rejected online poll advanced simulation state: '+JSON.stringify({actual,expected}));
        terminalUnchanged=true;
      };const events=[],output=[],probeStates=[],callerAbort=new AbortController();
      try {
        for(let i=0;i<asset.manifest.count;i++)await store.put(asset.manifest.id,i,
          {bytes:await file.slice(i*CHUNK,(i+1)*CHUNK).arrayBuffer(),hash:asset.hashes.slice(i*32,(i+1)*32)});
        await store.complete(asset.manifest);
        await globalThis.partyBoardDiagnosticStage('local-assets-stored');
        if(!platform){globalThis.testExternalEngine=true;globalThis.testSampleLimit=8;}
        runtime=await prepareFingerprintedSessionRuntime(store,asset.manifest.id,artifacts,{
            runtimeOptions:{canvas:document.getElementById('canvas'),print:line=>{output.push(line);if(globalThis.testTraceState&&(line.includes('State snapshot:')||line.includes('Global state snapshot:')||line.includes('Prepared engine RNG:')))console.log(line);},printErr:line=>output.push(line)},
            sessionSeed:1,signal:callerAbort.signal,
            inputExchange:inputExchange?async(input,options)=>{
              const frame=frameCount();
              if(neutralProbe)probeStates.push({frame,checkpoint:options.checkpoint,frand:options.frand,rand8:options.rand8});
              if(platform&&runCommit&&inputSamples===expectedFrames){terminalInput={frame,...options};return null;}
              inputSamples++;
              if(ownership)return ownedExchange.exchange(input,options);
              if(!neutralProbe)return input;
              const logical=new Uint8Array(44);
              // Canonical little-endian PAD button: A(0x0100)|Start(0x1000).
              if(probeButtons&&frame>=60&&frame%60<2)logical[1]=0x11;
              return logical;
            }:undefined,
            setup:async m=>{module=m;events.push('setup');
              await globalThis.partyBoardDiagnosticStage('platform-setup');
              if(platform){if(standalone&&['PartyBoardTestPrepareMountedPlatform','PartyBoardTestFinishExternalEngine','PartyBoardTestExternalFrames'].some(name=>typeof m['_'+name]==='function'))throw Error('Standalone runtime unexpectedly exports diagnostic bootstrap');
                await platformCallbacks.setup(m,{signal:callerAbort.signal});
                if(m._PartyBoardBrowserPreparePlatform()!==-1)throw Error('Duplicate platform preparation accepted');
              }else if(await m.ccall('PartyBoardTestPrepareMountedPlatform','number',[],[],{async:true})!==0)throw Error('Platform preparation failed');},
            prepareEngine:async(m,options)=>{
              await globalThis.partyBoardDiagnosticStage('engine-initialize');
              if(!cancelInitialize)return prepareEngineLifecycle(m,options);
              const original=m.ccall;
              m.ccall=function(name,...args){
                const result=original.call(m,name,...args);
                if(name==='PartyBoard_GameInitialize'){events.push('cancelInitialize');callerAbort.abort();}
                return result;
              };
              try{return await prepareEngineLifecycle(m,options);}finally{m.ccall=original;}
            },
            prepareRenderer:async(m,{engine,signal})=>{
              events.push('renderer');
              await globalThis.partyBoardDiagnosticStage('renderer-prepare');
              if(platform){const receipt=await platformCallbacks.prepareRenderer(m,{engine,signal});if(cancelRenderer)callerAbort.abort();return receipt;}
              if(await m.ccall('PartyBoardTestAdvancePreparedRenderer','number',[],[],{async:true})!==0)throw Error('Renderer-only step failed');
              if(cancelRenderer)callerAbort.abort();
              return drainIdleEnginePipelines(engine,{signal,
                read:()=>({queued:m._PartyBoardTestPipelineCount(0),created:m._PartyBoardTestPipelineCount(1)}),
                advance:async()=>{if(await m.ccall('PartyBoardTestAdvancePreparedRenderer','number',[],[],{async:true})!==0)throw Error('Renderer drain step failed');}});
            },
            shutdown:async m=>{events.push('shutdown');m._PartyBoard_GameRequestStop();if(platform){await platformCallbacks.shutdown(m);if(m._PartyBoardBrowserShutdownPlatform()!==0)throw Error('Repeated platform shutdown rejected');}else m._PartyBoardTestFinishExternalEngine();events.push('shutdownCompleted');},
          }).catch(cause=>{
            if(cancellation&&cause.name==='AbortError')return undefined;
            throw cause;
          });
        if(cancellation) {
          if(runtime)throw Error('Cancelled renderer preparation returned runtime ownership');
          return {platform,cancelled:true,frame:frameCount(),events,
            released:!module.FS.analyzePath('/assets').exists,cleanup:events.includes('shutdownCompleted')};
        }
        if(customKeyboard){const {defaultControlBindings}=await import('/control-bindings.mjs');const {applyRuntimeKeyboardBindings}=await import('/runtime-control-bindings.mjs');const profile=defaultControlBindings();profile.keyboard.a='Space';applyRuntimeKeyboardBindings(module,profile);if(frameCount()!==0)throw Error('Rebinding advanced prepared engine');await globalThis.partyBoardDiagnosticStage('custom-keyboard-applied-frame-zero');}
        if(glyphLifetime){
          if(standalone||!platform||typeof module._PartyBoardTestControlGlyphLifetime!=='function')throw Error('Glyph lifetime fixture requires diagnostic platform runtime');
          const {prepareRuntimeGlyphs}=await import('/runtime-glyphs.mjs');
          const glyphs=await prepareRuntimeGlyphs();module.partyBoardGlyphTextures=glyphs.initial;
          for(let slot=0;slot<4;slot++){
            module.partyBoardGlyphLocalPadSlots=Object.freeze(Array.from({length:4},(_,lane)=>lane===slot?0:null));
            const result=module._PartyBoardTestControlGlyphLifetime(slot);
            if(result!==0)throw Error('Actual glyph lifetime failed for slot '+slot+': '+result);
            for(let remote=0;remote<4;remote++)if(remote!==slot&&module._PartyBoardTestControlGlyphLifetime(remote)!==-10)
              throw Error('Local glyph labels leaked into remote slot '+remote);
          }
          if(module._PartyBoardTestControlGlyphLifetime(-1)!==-1||module._PartyBoardTestControlGlyphLifetime(4)!==-1)
            throw Error('Invalid glyph port accepted');
          delete module.partyBoardGlyphLocalPadSlots;
          if(frameCount()!==0)throw Error('Glyph lifetime diagnostic advanced the game');
        }
        buildId=runtime.buildId;
        const prepared={phase:runtime.engine.phase,frame:frameCount(),checkpoint:runtime.checkpoint,
          currentCheckpoint:runtime.engine.preparedCheckpoint(),assetId:runtime.assetId,renderer:runtime.rendererPreparation};
        if(platform&&typeof module._PartyBoard_GameCheckpointComponent==='function')prepared.components=[0,1,2,3].map(field=>module._PartyBoard_GameCheckpointComponent(field));
        if(platform)prepared.pausedState=runtime.engine.pausedState();
        if(mapping){const canvas=document.getElementById('canvas');canvas.tabIndex=0;canvas.focus();await globalThis.partyBoardInjectLocalKey();}
        let committedFrames;
        if(runCommit) {
          const {StartBarrier}=await import('/start-barrier.mjs');
          const {prepareStartAcknowledgment,resumeCommittedClientRuntime}=await import('/start-client.mjs');
          // Diagnostic session/build/readiness identity, not production evidence.
          const snapshot={revision:1,membership:1,total:asset.manifest.size,assetId:asset.manifest.id,players:
            ['host','guest'].map((peer,slot)=>({peer,slot,bytes:asset.manifest.size,verified:true,shaders:true,ready:true}))};
          const barrier=new StartBarrier(snapshot,{session:new Uint8Array(16).fill(1),seed:1,buildId,checkpoint:runtime.checkpoint});
          const options={peer:'guest',buildId,snapshot:()=>snapshot,requireOnlineInput:inputExchange};
          if(rtcCommit) {
            const {StartChannels}=await import('/start-channels.mjs');
            const {bindClientStartPreparation}=await import('/start-preparation-channel.mjs');
            const hostPc=new RTCPeerConnection({iceServers:[]}),guestPc=new RTCPeerConnection({iceServers:[]});
            let receiver,recovery,createInputRemote;const failures=[];
            const transport=new StartChannels(barrier,{snapshot:()=>snapshot,error:(_peer,cause)=>failures.push(cause.message)});
            try {
              const waitTransport=async(promise,stage)=>{
                await globalThis.partyBoardDiagnosticStage(stage);
                let timer;
                try{return await Promise.race([promise,new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error('WebRTC acceptance deadline: '+stage+' '+JSON.stringify({host:{connection:hostPc.connectionState,ice:hostPc.iceConnectionState,gathering:hostPc.iceGatheringState,signaling:hostPc.signalingState},guest:{connection:guestPc.connectionState,ice:guestPc.iceConnectionState,gathering:guestPc.iceGatheringState,signaling:guestPc.signalingState}}))),15000);})]);}
                finally{clearTimeout(timer);}
              };
              let receivedInput;
              const inputReady=ownership?new Promise(resolve=>receivedInput=resolve):Promise.resolve();
              const received=new Promise(resolve=>guestPc.ondatachannel=event=>{
                if(event.channel.label==='actual-session-input'){
                  inputChannel=event.channel;inputChannel.binaryType='arraybuffer';
                  inputChannel.onmessage=message=>{
                    const status=inputQueue.receive(1,new Uint8Array(message.data),inputNow());
                    if(!['Accepted','Duplicate'].includes(status)){failures.push('Remote owned input rejected: '+status);ownedExchange.notify();return;}
                    inputReceived++;ownedExchange.notify();
                  };
                  if(recoveryDeadline)inputChannel.onclose=()=>{inputQueue.disconnected(1,inputNow());ownedExchange.notify();};
                  receivedInput();return;
                }resolve(event.channel);
              });
              const hostChannel=hostPc.createDataChannel('actual-session-start',{ordered:true});
              if(ownership){
                const answered=new Set();let dropped=false;
                createInputRemote=()=>{
                const remote=hostPc.createDataChannel('actual-session-input',{ordered:true});
                remote.binaryType='arraybuffer';remote.onmessage=message=>{
                  // Already queued messages may arrive after the drop. They
                  // must not mark a frame answered on the closed old channel.
                  if(remote.readyState!=='open')return;
                  const packet=decodeInput(new Uint8Array(message.data));
                  if(!packet||![1,3].includes(packet.slot)){failures.push('Invalid local ownership packet');return;}
                  if(mapping&&packet.slot===1){if(!(packet.pad[1]&1)){failures.push('Guest keyboard A did not reach owned slot 1');return;}mappedKeyboardPackets++;}
                  if(extendedKeyboard&&packet.slot===1){
                    if((packet.pad[0]&0x68)!==0x68||packet.pad[4]===0||packet.pad[4]>127||packet.pad[5]===0||packet.pad[5]>127||packet.pad[6]<140||packet.pad[7]<140){
                      failures.push('Guest keyboard D-pad, right stick or analog shoulders did not reach owned slot 1: '+Array.from(packet.pad));return;
                    }
                  }
                  if(mapping&&packet.slot===3&&(packet.pad[1]&1)){failures.push('Keyboard leaked into second owned slot');return;}
                  if(extendedKeyboard&&packet.slot===3&&packet.pad.slice(0,9).some(value=>value!==0)){failures.push('Extended keyboard controls leaked into second owned slot');return;}
                  if(recoveryDeadline&&!dropped&&packet.frame===1n){dropped=true;remote.close();return;}
                  const key=String(packet.frame);if(answered.has(key))return;answered.add(key);
                  for(const slot of [0,2])remote.send(encodeInput({session:inputSession,frame:packet.frame,
                    checkpoint:packet.checkpoint,slot,pad:new Uint8Array(11)}));
                };
                return remote;
                };createInputRemote();
              }
              const gather=async pc=>{
                if(pc.iceGatheringState!=='complete')await new Promise(resolve=>{
                  const changed=()=>{if(pc.iceGatheringState==='complete'){pc.removeEventListener('icegatheringstatechange',changed);resolve();}};
                  pc.addEventListener('icegatheringstatechange',changed);changed();
                });return pc.localDescription;
              };
              await hostPc.setLocalDescription(await hostPc.createOffer());await guestPc.setRemoteDescription(await waitTransport(gather(hostPc),'host-ice-gathering'));
              await guestPc.setLocalDescription(await guestPc.createAnswer());await hostPc.setRemoteDescription(await waitTransport(gather(guestPc),'guest-ice-gathering'));
              const guestChannel=await waitTransport(received,'start-channel-received');
              await waitTransport(inputReady,'input-channel-received');
              receiver=bindClientStartPreparation(guestChannel,{...options,prepare:async()=>runtime},{error:cause=>failures.push(cause.message)});
              transport.acknowledgeLocal('host',{...barrier.proposal,type:'session-start-ack'});
              transport.attach('guest',hostChannel);
              const deadline=performance.now()+10000;
              while(ownership&&inputChannel.readyState!=='open'&&performance.now()<deadline)await new Promise(resolve=>setTimeout(resolve,5));
              if(ownership&&inputChannel.readyState!=='open')throw Error('Owned input channel unavailable');
              while(barrier.state!=='Agreed'&&!failures.length&&performance.now()<deadline)await new Promise(resolve=>setTimeout(resolve,5));
              if(barrier.state!=='Agreed'||failures.length||runtime.engine.phase!=='Idle'||frameCount()!==0)
                throw Error('Actual RTC preparation agreement failed: '+JSON.stringify(failures));
              const commit=transport.prepareCommit('host');hostChannel.send(JSON.stringify(commit));
              while(!receiver.committed&&!failures.length&&performance.now()<deadline)await new Promise(resolve=>setTimeout(resolve,5));
              if(!receiver.committed||failures.length)throw Error('Actual RTC commit failed: '+JSON.stringify(failures));
              if(recoveryDeadline){
                while(inputQueue.status(inputNow()).state!=='Paused'&&!failures.length&&performance.now()<deadline)await new Promise(resolve=>setTimeout(resolve,5));
                const pending=ownedExchange.pendingState(),frame=frameCount();let menus=0;
                if(inputQueue.status(inputNow()).state!=='Paused'||!pending||pending.frame!==1n||runtime.engine.phase!=='Running')throw Error('Actual input suspension unavailable');
                recovery=createRealtimeInputRecovery(inputQueue,ownedExchange,{now:inputNow,
                  schedule:()=>0,cancel:()=>{},channels:{close:()=>inputChannel.close()},dispose:()=>runtime.dispose(),
                  transportReady:()=>reconnect&&inputChannel.readyState==='open',returnToMenu:()=>{menus++;}});
                recoveryClock=59999;await recovery.tick();
                if(menus!==0||runtime.module!==module||runtime.engine.phase!=='Running'||frameCount()!==frame||ownedExchange.pendingState().checkpoint!==pending.checkpoint)
                  throw Error('Actual recovery released or advanced retained session early');
                if(reconnect){
                  const previous=inputChannel;createInputRemote();
                  while((inputChannel===previous||inputChannel.readyState!=='open')&&performance.now()<deadline)await new Promise(resolve=>setTimeout(resolve,5));
                  if(inputChannel===previous||inputChannel.readyState!=='open'||inputQueue.status(inputNow()).state!=='Paused'||frameCount()!==frame)
                    throw Error('Replacement channel unavailable or resumed without state agreement');
                  const token=recovery.attempt(1);let verified=0;
                  if(!token||await recovery.restore({...token,checkpoint:token.checkpoint^1},async()=>{verified++;return true;})||verified!==0)
                    throw Error('Changed recovery state accepted');
                  const restored=await recovery.restore(token,async actual=>{
                    verified++;return actual.frame===pending.frame&&actual.checkpoint===pending.checkpoint&&actual.frand===pending.frand&&actual.rand8===pending.rand8;
                  });
                  if(!restored||verified!==1||menus!==0)throw Error('Retained poll recovery failed');
                  await waitExecution(receiver.execution);
                  if(runtime.engine.phase!=='Idle'||frameCount()!==expectedFrames||inputQueue.status(inputNow()).frame!==4n)
                    throw Error('Recovered actual execution did not continue');
                  recoveryResult={retainedUntilMs:59999,recovered:true,menusBeforeCleanup:menus,frame,pendingFrame:String(pending.frame)};
                }else{
                recoveryClock=60000;await recovery.tick();await waitExecution(receiver.execution);
                if(menus!==1||runtime.module!==undefined||module.partyBoardInputExchange!==undefined||module.FS.analyzePath('/assets').exists)
                  throw Error('Actual recovery deadline did not release session');
                recoveryResult={retainedUntilMs:59999,expiredAtMs:60000,menus,frame,pendingFrame:String(pending.frame)};
                }
              }
              await waitExecution(receiver.execution);verifyTerminal();committedFrames=frameCount();
              if(!recoveryDeadline&&(runtime.engine.phase!=='Idle'||committedFrames!==expectedFrames))throw Error('Actual RTC execution frame count differs');
              await receiver.close();
            } finally {if(recovery)await recovery.close();if(receiver)await receiver.close();transport.close();hostPc.close();guestPc.close();}
          } else {
          const accepted=await prepareStartAcknowledgment(barrier.proposal,{...options,prepare:async()=>runtime});
          let rejected=false;
          try{resumeCommittedClientRuntime({...barrier.proposal,type:'session-start-commit',seed:2},accepted,options);}catch{rejected=true;}
          if(!rejected||runtime.engine.phase!=='Idle'||frameCount()!==0)throw Error('Changed commit advanced shared session');
          await waitExecution(resumeCommittedClientRuntime({...barrier.proposal,type:'session-start-commit'},accepted,options));verifyTerminal();
          committedFrames=frameCount();
          if(runtime.engine.phase!=='Idle'||committedFrames!==expectedFrames)throw Error('Shared session bounded commit execution failed');
          }
        }
        const terminalComponents=neutralProbe?[0,1,2,3].map(field=>module._PartyBoard_GameCheckpointComponent(field)):undefined;
        const canvas=document.querySelector('canvas'),renderSize={width:canvas.width,height:canvas.height};
        await Promise.all([runtime.dispose(),runtime.dispose()]);
        return {glyphLifetime,renderSize,mappedKeyboardPackets,terminalUnchanged,standalone,platform,prepared,buildId,committedFrames,inputSamples,inputSent,inputReceived,recoveryResult,inputFrame:inputQueue?String(inputQueue.status(inputNow()).frame):undefined,inputReleased:module.partyBoardInputExchange===undefined,events,released:runtime.module===undefined&&!module.FS.analyzePath('/assets').exists,
          size:asset.manifest.size,probeStates,terminalComponents,cleanup:events.includes('shutdownCompleted')};
      } finally {if(runtime)await runtime.dispose();store.close();}
    },{resolution,mapping,customKeyboard,extendedKeyboard,glyphLifetime,standalone,platform,runCommit,rtcCommit,cancelRenderer,cancelInitialize,cancellation,inputExchange,ownership,recoveryDeadline,reconnect,neutralProbe,expectedFrames,probeButtons});
    let diagnosticTimer;
    const result=await Promise.race([execution,new Promise((_,reject)=>{
      diagnosticTimer=setTimeout(()=>reject(Error('Session diagnostic exceeded 10-minute acceptance deadline at '+lastDiagnosticStage)),600000);
    })]).finally(()=>clearTimeout(diagnosticTimer));
    console.log('Session diagnostic result: '+JSON.stringify(result));
    if(resolution&&!cancellation)assert.deepEqual(result.renderSize,resolution,'Selected guest framebuffer size differs');
    if(cancellation) {
      assert.deepEqual(errors,[]);assert.equal(result.cancelled,true);assert.equal(result.frame,0);
      assert.deepEqual(result.events,['setup',cancelInitialize?'cancelInitialize':'renderer','shutdown','shutdownCompleted']);
      assert.equal(result.released,true);assert.equal(result.cleanup,true);
      console.log('PASS: actual Wasm '+(cancelInitialize?'initialization-call':'renderer-stage')+' cancellation returned no runtime, completed shutdown and released mounted game assets at frame zero.');return;
    }
    assert.deepEqual(errors,[]);assert.equal(result.prepared.phase,'Idle');assert.equal(result.prepared.frame,0);
    assert.equal(result.prepared.checkpoint,result.prepared.currentCheckpoint);assert.equal(result.prepared.renderer.checkpoint,result.prepared.checkpoint);
    if(process.env.PARTYBOARD_EXPECT_PREPARED_STATE){
      const expected=JSON.parse(process.env.PARTYBOARD_EXPECT_PREPARED_STATE);
      assert.deepEqual(result.prepared.pausedState,expected,'Actual browser prepared state differs from independently captured native state');
      console.log('PASS: actual browser prepared frame/checkpoint/frand/rand8 match supplied native capture');
    }
    assert.equal(result.prepared.renderer.scope,'discovered-pipelines-only');assert.equal(result.prepared.renderer.queued,0);
    assert.deepEqual(result.events,['setup','renderer','shutdown','shutdownCompleted']);assert.equal(result.released,true);assert.equal(result.cleanup,true);
    if(runCommit&&!recoveryDeadline)assert.equal(result.committedFrames,expectedFrames);
    if(platform&&runCommit&&(!recoveryDeadline||reconnect))assert.equal(result.terminalUnchanged,true);
    // Online failures stop before frame work; legacy local trace counts remain unchanged.
    if(inputExchange&&runCommit){if(!recoveryDeadline)assert.equal(result.inputSamples,expectedFrames);assert.equal(result.inputReleased,true);}
    if(mapping&&!recoveryDeadline)assert.equal(result.mappedKeyboardPackets,4);
    if(ownership&&!recoveryDeadline){assert.equal(result.inputSent,8);assert.equal(result.inputReceived,8);assert.equal(result.inputFrame,'4');}
    if(recoveryDeadline&&!reconnect){assert.equal(result.recoveryResult.menus,1);assert.equal(result.recoveryResult.pendingFrame,'1');assert.equal(result.recoveryResult.expiredAtMs,60000);}
    if(reconnect){assert.equal(result.recoveryResult.recovered,true);assert.equal(result.recoveryResult.menusBeforeCleanup,0);assert.equal(result.committedFrames,expectedFrames);assert.equal(result.inputSamples,4);assert.equal(result.inputSent,10);assert.equal(result.inputReceived,8);assert.equal(result.inputFrame,'4');}
    console.log('PASS: actual local-game shared session preparation, renderer callback, idle checkpoint and ordered cleanup/unmount: '+JSON.stringify(result));
    console.log(reconnect?'PASS: actual suspended Wasm continued its same execution after replacement input channel, rejected changed recovery token, explicit diagnostic state verification and retained packet resend; synthetic remote owner, no independent remote game state proof.':recoveryDeadline?'PASS: actual committed Wasm retained a suspended owned-input poll through injected 59999ms, then shut down/unmounted and returned to diagnostic menu callback once at 60000ms.':rtcCommit?'PASS: actual prepared runtime acknowledged over local WebRTC and executed bounded startup frames only after matching host commit. One browser context, fixed identity/synthetic host; no production netplay claim.':runCommit?'PASS: changed commit held actual shared runtime at frame zero; matching commit ran bounded startup frames. Fixed diagnostic identity; no production netplay or shader-coverage claim.':'No simulation resumed. No complete shader discovery, transferred-game start or multiplayer claim.');
  } finally {await browser?.close();await new Promise(resolve=>server.close(resolve));}
})().catch(cause=>{console.error(cause);process.exitCode=1;});



