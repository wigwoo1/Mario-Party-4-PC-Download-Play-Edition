'use strict';
// User preference: test audio must never reach physical playback devices.
process.env.SDL_AUDIO_DRIVER='dummy';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {spawn}=require('node:child_process'),{createInterface}=require('node:readline');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const http=require('node:http');
(async()=>{
 const frames=Number(process.env.PARTYBOARD_RTC_ENGINE_FRAMES??4);
 if(!Number.isInteger(frames)||frames<1||frames>1800)throw Error('Actual RTC diagnostic requires 1..1800 frames');
 const closeMode=!!process.env.PARTYBOARD_NATIVE_ENGINE_CLOSE_TEST;
 const browserGame=!!process.env.PARTYBOARD_BROWSER_GAME_RUNTIME;
 const port=Number(process.env.PARTYBOARD_TEST_PORT??8819);
 if(!Number.isInteger(port)||port<1024||port>65535)throw Error('Valid local test port required');
 const origin='http://127.0.0.1:'+port;
 if(browserGame&&closeMode)throw Error('Choose browser game comparison or disconnect scenario');
 const diagnosticButtons=!!process.env.PARTYBOARD_RTC_ENGINE_BUTTONS;
 const native=spawn(process.env.PARTYBOARD_NATIVE_GAME,['--disc',process.env.PARTYBOARD_LOCAL_CISO,
  '--prepare-online-engine','--simulation-seed','1','--probe-rtc-engine',String(frames),...(diagnosticButtons?['--probe-online-buttons']:[])],{stdio:['pipe','pipe','pipe']});
 let browser,server,failure,stderr='';const events=[],pending=[];
 const fail=error=>{failure=error;for(const w of pending.splice(0)){clearTimeout(w.timer);w.reject(error);}};
 native.on('error',fail);native.stderr.on('data',bytes=>stderr+=bytes);
 const exited=new Promise(resolve=>native.on('exit',(code,signal)=>{resolve({code,signal});fail(Error('Native exit '+code));}));
 createInterface({input:native.stdout}).on('line',line=>{
  if(!line.startsWith('Native RTC diagnostic: '))return;
  try{const event=JSON.parse(line.slice(23));events.push(event);
   for(let i=pending.length-1;i>=0;i--)if(pending[i].test(event)){const w=pending.splice(i,1)[0];clearTimeout(w.timer);w.resolve(event);}
  }catch(error){fail(error);}
 });
 const wait=(type,slot)=>new Promise((resolve,reject)=>{
  if(failure)return reject(failure);const test=e=>e.type===type&&(slot===undefined||e.slot===slot);
  const found=events.find(test);if(found)return resolve(found);
  const w={test,resolve,reject};w.timer=setTimeout(()=>{pending.splice(pending.indexOf(w),1);reject(Error('Native event timeout '+type));},90000);pending.push(w);
 });
 try{
  if(browserGame){
   const web=path.resolve(__dirname,'../../web'),runtime=path.resolve(process.env.PARTYBOARD_BROWSER_GAME_RUNTIME);
   server=http.createServer((req,res)=>{
    if(req.url==='/'){res.end('<!doctype html><input type="file"><canvas id="canvas" width="640" height="480"></canvas>');return;}
    const name=req.url?.slice(1);if(!name||name.includes('/')||name.endsWith('.test.mjs')){res.writeHead(404);res.end();return;}
    const file=path.join(name==='contract.mjs'||name==='contract.wasm'?runtime:web,name);
    if(!fs.existsSync(file)){res.writeHead(404);res.end();return;}
    res.setHeader('Content-Type',name.endsWith('.wasm')?'application/wasm':'text/javascript');fs.createReadStream(file).pipe(res);
   });
   await new Promise((resolve,reject)=>{server.once('error',reject);server.listen(port,'127.0.0.1',resolve);});
  }else server=spawn(process.execPath,[path.resolve(__dirname,'../../web/serve.cjs')],{env:{...process.env,PARTYBOARD_TEST_PORT:String(port)},stdio:['ignore','pipe','pipe']});
  for(let i=0;i<100;i++){try{if((await fetch(origin+'/lockstep.mjs')).ok)break;}catch{}if(i===99)throw Error('Server unavailable');await new Promise(r=>setTimeout(r,50));}
  browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});const pages=[];
  for(let slot=1;slot<4;slot++){
   const credential=(await wait('original-credential',slot)).credential,offer=(await wait('offer',slot)).packet;
   const page=await(await browser.newContext()).newPage();pages.push(page);await page.goto(origin);
   if(browserGame)await page.locator('input').setInputFiles(process.env.PARTYBOARD_LOCAL_CISO);
   page.on('console',message=>{if(message.text().startsWith('RTC preparation: '))console.log('guest'+slot+' '+message.text());});
   const answer=await page.evaluate(async({credential,offer,slot,closeMode,browserGame,frames})=>{
    const recovery=await import('/recovery-identity.mjs');const {Lockstep}=await import('/lockstep.mjs');
    const {decodeInput,encodeInput}=await import('/input-packet.mjs');
    const identity=recovery.receiveRecoveryCredential(credential,credential);
    const verified=await recovery.verifyRecoveryDescription(offer,identity,{type:'offer',usedNonces:new Set()});if(!verified)throw Error('Offer rejected');
    const session=new Uint8Array(16).fill(0x11),owners=[1,2,3,4];
    const {SessionInput}=await import('/session-input.mjs');
    const {createSessionInputExchange}=await import('/session-input-exchange.mjs');
    const {createLobbyInputRouter}=await import('/lobby-input-router.mjs');
    const {lobbyInputOwnership}=await import('/lobby-input-ownership.mjs');
    let inputRouter;
    const queue=browserGame?new SessionInput(session,owners):new Lockstep(session,owners);
    const exchange=browserGame?createSessionInputExchange(queue,{session,owners,peer:slot+1,checkpoint:options=>options.checkpoint,send:bytes=>inputRouter.send(bytes),localPadSlots:owners.map((owner,index)=>index===slot?0:null)}):null;
    globalThis.committed=[];globalThis.inputError=null;
    globalThis.engineStates=[];
    if(browserGame){
     const {prepare,browserStore,CHUNK}=await import('/transfer.mjs');
     const {browserPlatformCallbacks}=await import('/browser-platform.mjs');
     const {loadBrowserRuntimePackage}=await import('/browser-runtime-package.mjs');
     const {prepareFingerprintedSessionRuntime}=await import('/session-runtime.mjs');
     console.log('RTC preparation: hashing private assets');
     const file=document.querySelector('input').files[0],asset=await prepare(file),store=await browserStore();
     for(let i=0;i<asset.manifest.count;i++)await store.put(asset.manifest.id,i,{bytes:await file.slice(i*CHUNK,(i+1)*CHUNK).arrayBuffer(),hash:asset.hashes.slice(i*32,(i+1)*32)});
     await store.complete(asset.manifest);console.log('RTC preparation: verified private asset store');
     const platform=browserPlatformCallbacks(),artifacts=await loadBrowserRuntimePackage({baseURL:location.origin+'/'});
     globalThis.runtime=await prepareFingerprintedSessionRuntime(store,asset.manifest.id,artifacts,{
      sessionSeed:1,runtimeOptions:{canvas:document.querySelector('canvas'),print:()=>{},printErr:line=>console.error(line)},
      setup:platform.setup,prepareRenderer:platform.prepareRenderer,shutdown:platform.shutdown,
      inputExchange:async(input,options)=>{
       const frame=runtime.module._PartyBoard_GameFrameCount();
       engineStates.push({frame,checkpoint:options.checkpoint,frand:options.frand,rand8:options.rand8});
       if(frame===frames)return null;
       while(!globalThis.channel||channel.readyState!=='open')await new Promise(r=>setTimeout(r,1));
       const local=new Uint8Array(44);local[0]=slot;
       const merged=await exchange.exchange(local,options);
       if(inputError)throw Error(inputError);
       if(!merged)throw Error('Production exchange stopped before bounded completion');
       const pads=Array.from({length:4},(_,index)=>Array.from(merged.slice(index*11,(index+1)*11)));
       committed.push({frame,pads});return merged;
      }});
     if(runtime.checkpoint!==3522050)throw Error('Actual prepared checkpoint differs from native seed1');
     console.log('RTC preparation: actual engine and renderer ready');
    }
    globalThis.connection=new RTCPeerConnection({iceServers:[]});
    connection.ondatachannel=event=>{globalThis.channel=event.channel;channel.binaryType='arraybuffer';
     if(browserGame){
      const plan=lobbyInputOwnership({membership:1,players:owners.map((_,index)=>({peer:index===0?'host':'guest'+index,slot:index}))},'guest'+slot);
      globalThis.inputTransportOwner=inputRouter=createLobbyInputRouter({plan,session,channels:new Map([['host',channel]]),receive:(owner,bytes)=>{
       const result=queue.receive(owner,bytes,performance.now());if(!['Accepted','Duplicate','Stale'].includes(result))throw Error('Production route input rejected: '+result);exchange.notify();
      },disconnected:lost=>{for(const owner of lost)queue.disconnected(owner,performance.now());exchange.notify();},error:(_,error)=>{inputError=String(error);queue.cancel();exchange.notify();}});
      return;
     }
     channel.onmessage=message=>{try{
      const bytes=new Uint8Array(message.data),packet=decodeInput(bytes);if(!packet||packet.slot===slot)throw Error('Bad relay');
      if(packet.slot===0 && closeMode && slot===1){globalThis.closedAt=Date.now();channel.close();return;}
      if(packet.slot===0&&!browserGame){const pad=new Uint8Array(11);pad[0]=slot;
       const local=encodeInput({session,frame:packet.frame,checkpoint:packet.checkpoint,slot,pad});
       if(queue.receive(slot+1,local)!=='Accepted')throw Error('Local input rejected');channel.send(local);
      }
      if(queue.receive(packet.slot+1,bytes,performance.now())!=='Accepted')throw Error('Remote input rejected');
      if(browserGame)exchange.notify();
      if(!browserGame){const frame=queue.advance(packet.checkpoint);if(frame)committed.push({frame:Number(frame.index),pads:frame.pads.map(p=>Array.from(p))});}
     }catch(error){inputError=String(error);}};
    };
    await connection.setRemoteDescription(verified.description);await connection.setLocalDescription(await connection.createAnswer());
    if(connection.iceGatheringState!=='complete')await new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('ICE timeout')),30000);connection.addEventListener('icegatheringstatechange',()=>{if(connection.iceGatheringState==='complete'){clearTimeout(timer);resolve();}});});
    const answer=await recovery.signRecoveryDescription(identity,{type:'answer',sdp:connection.localDescription.sdp},{nonce:offer.nonce});
    if(browserGame)globalThis.execution=runtime.engine.resume().catch(error=>{inputError=String(error);throw error;});
    return answer;
   },{credential,offer,slot,closeMode,browserGame,frames});
   native.stdin.write(JSON.stringify({type:'answer',slot,packet:answer})+'\n');await wait('open',slot);
  }
  if(closeMode){
   const stopped=await wait('stopped'),initial=events.find(e=>e.type==='state');
   const elapsed=Date.now()-await pages[0].evaluate(()=>closedAt);
   assert.ok(elapsed>=59000&&elapsed<70000,'Actual native engine original reconnect deadline');
   assert.equal(stopped.accepted,0);assert.equal(stopped.phase,2);assert.equal(stopped.detached,true);
   for(const field of ['frame','checkpoint','frand','rand8'])assert.equal(stopped[field],initial[field],field+' stayed frozen');
   assert.equal(events.some(e=>e.type==='committed'),false);for(const page of pages)assert.deepEqual(await page.evaluate(()=>committed),[]);
   assert.equal((await exited).code,2); // Intentional bounded probe did not complete its requested frames.
   fs.writeFileSync(process.env.PARTYBOARD_NATIVE_GAME_REPORT,JSON.stringify({scope:'Actual native engine waits on three RTC guests; guest disconnect expires without simulation',
    elapsedMs:elapsed,initial,stopped,actualGameStateFrozen:true,noFrameCommitted:true,stoppedOwnedInputDetached:true,
    nativeExitCode:2,expectedIncompleteDiagnostic:true,browserGameExecutionVerified:false,authenticatedReconnectVerified:false,playableNetplayVerified:false},null,2)+'\n');
   console.log('PASS actual native engine RTC disconnect: frozen state, 60-second expiry and owned cleanup.');return;
  }
  const complete=await wait('complete');assert.equal(complete.frames,frames);assert.equal(complete.phase,2);assert.equal(complete.detached,true);
  const nativeFrames=events.filter(e=>e.type==='committed').map(({frame,pads})=>({frame,pads}));
  assert.equal(nativeFrames.length,frames);
  if(diagnosticButtons)for(const frame of nativeFrames){const buttons=frame.frame>=60&&frame.frame%60<2?0x1100:0;assert.deepEqual(frame.pads[0],[buttons&255,buttons>>8,0,0,0,0,0,0,0,0,0]);}assert.deepEqual(nativeFrames.map(f=>f.frame),Array.from({length:frames},(_,index)=>index));
  for(const page of pages){await page.waitForFunction(limit=>inputError||committed.length===limit,frames,{timeout:10000});assert.equal(await page.evaluate(()=>inputError),null);assert.deepEqual(await page.evaluate(()=>committed),nativeFrames);}
  assert.deepEqual(events.filter(e=>e.type==='state').map(e=>e.frame),Array.from({length:frames+1},(_,index)=>index));
  const guestStates=[],guestProgress=[],screenshots=[];
  if(browserGame)for(const page of pages){
   const result=await page.evaluate(async()=>{await execution;const state=runtime.engine.pausedState();const states=engineStates;return {state,states,progress:Array.from({length:6},(_,field)=>runtime.module._PartyBoard_GameProgress(field))};});
   const screenshot=process.env.PARTYBOARD_NATIVE_GAME_REPORT.replace(/\.json$/,'')+'-guest'+(guestStates.length+1)+'.png';
   await page.locator('canvas').screenshot({path:screenshot});screenshots.push(screenshot);guestProgress.push(result.progress);
   assert.equal(await page.evaluate(async()=>{inputTransportOwner.close();await runtime.dispose();return runtime.module===undefined;}),true);assert.equal(result.state.frame,frames);
   assert.deepEqual(result.states,events.filter(e=>e.type==='state').map(({frame,checkpoint,frand,rand8})=>({frame,checkpoint,frand,rand8})));
   guestStates.push(result.states);
  }
  native.stdin.write(JSON.stringify({type:'stop'})+'\n');assert.equal((await exited).code,0);
  fs.writeFileSync(process.env.PARTYBOARD_NATIVE_GAME_REPORT,JSON.stringify({scope:browserGame?'Actual native and three browser game engines: '+frames+' startup frames over RTC; no production lobby/start or full-match acceptance':complete.scope,actualNativeEngineFrames:frames,nativeFrames,
   states:events.filter(e=>e.type==='state'),threeActualRtcGuests:true,allFourMergedInputsIdentical:true,stoppedOwnedInputDetached:true,
   nativeExitCode:0,diagnosticButtons,browserGameExecutionVerified:browserGame,guestStates,guestProgress,screenshots,productionLobbyAndStartVerified:false,playableNetplayVerified:false},null,2)+'\n');
  console.log(browserGame?'PASS actual native and three browser game engines: '+frames+' RTC startup frames with identical inputs, checkpoints and RNG.':'PASS actual native engine / three Edge RTC input guests: '+frames+' frames and identical merged inputs.');
 }catch(error){if(stderr)console.error(stderr);throw error;}
 finally{for(const w of pending)clearTimeout(w.timer);if(native.exitCode===null)native.kill();if(browserGame)server?.close();else server?.kill();await browser?.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
