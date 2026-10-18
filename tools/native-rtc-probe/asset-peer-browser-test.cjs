'use strict';
// User preference: test audio must never reach physical playback devices.
process.env.SDL_AUDIO_DRIVER='dummy';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {spawn}=require('node:child_process'),{createInterface}=require('node:readline');
const http=require('node:http');
const {errorEvidence}=require('./error-evidence.cjs');
const {createNativeMatchObserver}=require('./native-match-observer.cjs');
const {parseLocalAudioLayouts,localAudioChannels}=require('./local-audio-layouts.cjs');
const {recoveryPlan}=require('./recovery-plan.cjs');
const {readPreparedBrowserProfile}=require('./prepared-browser-profile.cjs');
const localAudioLayouts=parseLocalAudioLayouts(process.env.PARTYBOARD_NATIVE_LOCAL_AUDIO_LAYOUTS);
const readPackage=()=>{
 const filename=path.resolve(process.env.PARTYBOARD_NATIVE_FRONTEND_PACKAGE);
 const descriptor=JSON.parse(fs.readFileSync(filename,'utf8'));
 for(const artifact of descriptor.files){
  if(typeof artifact.path!=='string'||!artifact.path)throw Error('Local runtime artifact path required');
  artifact.path=path.resolve(path.dirname(filename),artifact.path);
 }
 return descriptor;
};
const {chromium}=require(process.env.PLAYWRIGHT_MODULE);
(async()=>{
 const port=process.env.PARTYBOARD_NATIVE_ASSET_TEST_PORT||'8819',origin='http://127.0.0.1:'+port;
 const initial=process.env.PARTYBOARD_NATIVE_ASSET_INITIAL_INVITATION==='1';
 const lobbyMode=process.env.PARTYBOARD_NATIVE_ASSET_LOBBY==='1';
 const sessionMode=process.env.PARTYBOARD_NATIVE_ASSET_SESSION==='1';
 const frontend=process.env.PARTYBOARD_NATIVE_ASSET_FRONTEND==='1';
 const fullMatch=process.env.PARTYBOARD_NATIVE_FULL_MATCH==='1';
 const focusedPlay=fullMatch&&process.env.PARTYBOARD_NATIVE_FOCUSED_MINIGAME==='443';
 if(process.env.PARTYBOARD_NATIVE_FOCUSED_MINIGAME)assert(focusedPlay,'Focused minigame requires the owned full-match diagnostic transport and target 443');
 const fullMatchTurns=Number(process.env.PARTYBOARD_NATIVE_FULL_MATCH_TURNS||20);
 assert.ok(Number.isInteger(fullMatchTurns)&&fullMatchTurns>=5&&fullMatchTurns<=50&&fullMatchTurns%5===0,'Full-match turn count must be 5 through 50 in five-turn steps');
 const captureNativeRankings=process.env.PARTYBOARD_NATIVE_CAPTURE_RANKINGS==='1';
 assert.ok(!captureNativeRankings||fullMatch,'Native rankings capture requires full match');
 const captureDirectory=captureNativeRankings?path.resolve(process.env.PARTYBOARD_NATIVE_ASSET_REPORT+'.full-match'):null;
 if(captureDirectory)fs.mkdirSync(captureDirectory,{recursive:true});
 const nativeCaptureEnv=owner=>captureNativeRankings?{PARTYBOARD_FRAME_CAPTURE_REQUEST:path.join(captureDirectory,'native-rankings-'+owner+'.request')}:{PARTYBOARD_FRAME_CAPTURE_REQUEST:''};
 const nativeLaunchEnv=extra=>{
  const environment={...process.env,...extra};
  if(process.env.PARTYBOARD_NATIVE_BUNDLED_PIPELINES==='1'){
   for(const name of ['PARTYBOARD_NATIVE_PIPELINE_SEED','PARTYBOARD_NATIVE_PIPELINE_SEED_SHA256','PARTYBOARD_NATIVE_PIPELINE_SEED_COUNT'])delete environment[name];
   console.log('Launching native child with bundled shaders and no seed environment settings');
  }
  return environment;
 };
 const prepareFrontend=process.env.PARTYBOARD_NATIVE_FRONTEND_PREPARE==='1';
 const startFrontend=process.env.PARTYBOARD_NATIVE_FRONTEND_START==='1';
 const {recoverFrontend,nativeSelfRecovery,recoveryRounds,recoveryTurns}=recoveryPlan(process.env,{focusedPlay,fullMatch,fullMatchTurns});
 const externalJoin=process.env.PARTYBOARD_NATIVE_EXTERNAL_JOIN==='1';
 const menuJoin=process.env.PARTYBOARD_NATIVE_MENU_JOIN==='1';assert.ok(!menuJoin||externalJoin);
 assert.ok(!fullMatch||(frontend&&startFrontend&&menuJoin),'Mixed full match requires native host gameplay and native menu Join.');
 const verifyLobbyMusic=process.env.PARTYBOARD_NATIVE_LOBBY_MUSIC==='1';
 assert.ok(!verifyLobbyMusic||(menuJoin&&startFrontend),'Lobby music acceptance requires actual menu Join and committed gameplay start.');
 assert.ok(!nativeSelfRecovery||(externalJoin&&recoverFrontend));
 assert.ok(!externalJoin||prepareFrontend);
 assert.ok(!recoverFrontend||startFrontend);
 assert.ok(!startFrontend||prepareFrontend);
 assert.ok(!prepareFrontend||frontend);
 const count=Number(process.env.PARTYBOARD_NATIVE_ASSET_GUESTS||1);assert.ok(count===1||(initial&&count===3));
 assert.ok(!lobbyMode||(initial&&count===3));
 assert.ok(!sessionMode||(lobbyMode&&initial&&count===3));
 let observedPipelineSeed;
 if(process.env.PARTYBOARD_NATIVE_PIPELINE_SEED){
  assert.ok(prepareFrontend,'Pipeline seed requires engine preparation');
  const bytes=fs.readFileSync(process.env.PARTYBOARD_NATIVE_PIPELINE_SEED);
  assert.ok(bytes.length>=100&&bytes.length<=32*1024**2&&bytes.subarray(0,16).equals(Buffer.from('SQLite format 3\0')),'Invalid observed pipeline database');
  const sha256=require('node:crypto').createHash('sha256').update(bytes).digest('hex');
  if(process.env.PARTYBOARD_NATIVE_PIPELINE_SEED_SHA256)assert.equal(sha256,process.env.PARTYBOARD_NATIVE_PIPELINE_SEED_SHA256,'Observed pipeline seed changed');
  observedPipelineSeed={bytes,sha256};
 }
 const expectedSeedPipelines=Number(process.env.PARTYBOARD_NATIVE_PIPELINE_SEED_COUNT||0);
 assert.ok(Number.isSafeInteger(expectedSeedPipelines)&&expectedSeedPipelines>=0&&(!expectedSeedPipelines||observedPipelineSeed),'Invalid expected pipeline seed count');
 const native=spawn(process.env.PARTYBOARD_NATIVE_ASSET_PEER,frontend?[startFrontend?'--probe-host-gameplay':'--probe-host-connected','--disc',process.env.PARTYBOARD_NATIVE_ASSET_PATH,'--native-session-package',process.env.PARTYBOARD_NATIVE_FRONTEND_PACKAGE,'--user-dir',process.env.PARTYBOARD_NATIVE_FRONTEND_PROFILE]:[...(initial?[sessionMode?'--session-three-asset-lobby':lobbyMode?'--initial-three-asset-lobby':count===3?'--initial-three-invitations':'--initial-invitation']:[]),process.env.PARTYBOARD_NATIVE_ASSET_PATH],{env:nativeLaunchEnv({...nativeCaptureEnv('host'),...(process.env.PARTYBOARD_NATIVE_HOST_CRASH_DUMP?{PARTYBOARD_NATIVE_CRASH_DUMP:process.env.PARTYBOARD_NATIVE_HOST_CRASH_DUMP}:{}),...(startFrontend?{PARTYBOARD_NATIVE_DIAGNOSTIC_FINISH_ACK:'1'}:{})}),cwd:frontend?path.dirname(process.env.PARTYBOARD_NATIVE_ASSET_PEER):undefined,stdio:['pipe','pipe','pipe']});
 const hostMatchObserver=fullMatch?createNativeMatchObserver({timeoutMs:120000,send:packet=>new Promise((resolve,reject)=>native.stdin.write(JSON.stringify(packet)+'\n',error=>error?reject(error):resolve()))}):null;
 let joinMatchObserver;
 const events=[],pending=[],peers=[];let browser,browserServer,server,artifactServer,artifactOrigin,failure,failed=false,stderr='';
 const retainEvent=(list,event)=>{
  list.push(event);
  if(event.type==='diagnostic-transfer-state'&&process.env.PARTYBOARD_NATIVE_ASSET_REPORT)
   fs.appendFileSync(process.env.PARTYBOARD_NATIVE_ASSET_REPORT+'.transfer-state.jsonl',JSON.stringify(event)+'\n');
  // Rolling observations contain 256 full PAD records each. Keep a bounded
  // diagnostic history so failure reporting remains possible in long matches.
  if(event.type==='full-match-observation'){
   let remaining=16;
   for(let index=list.length-1;index>=0;index--)if(list[index].type===event.type&&--remaining<0)list.splice(index,1);
  }
 };
 const diagnostics=[];let stage='launch';
 const randomTraceTail=require('./random-trace-tail.cjs').createRandomTraceTail();
 let randomTraceCount=0;
 const randomTrace=(peer,message)=>{
  if(!/^RNG (call|hook) /.test(String(message)))return false;
  randomTraceTail.add({peer,message:String(message),stage});
  if(randomTraceCount>=4096)return true;
  const entry={peer,message:String(message),stage};randomTraceCount++;
  if(process.env.PARTYBOARD_NATIVE_ASSET_REPORT)
   fs.appendFileSync(process.env.PARTYBOARD_NATIVE_ASSET_REPORT+'.rng-calls.jsonl',JSON.stringify(entry)+'\n');
  return true;
 };
 createInterface({input:native.stderr}).on('line',line=>randomTrace('native-host',line));
 const diagnostic=(type,message,error)=>{const entry={stage,type,message:String(message).slice(0,1000),
  ...(error?{errorDetails:errorEvidence(error)}:{})};
  const critical=Boolean(error)||['page-crash','browser-disconnected'].includes(type)||/device lost|device loss|fatal|aborted/i.test(entry.message);
  if(critical&&process.env.PARTYBOARD_NATIVE_ASSET_REPORT)
   fs.appendFileSync(process.env.PARTYBOARD_NATIVE_ASSET_REPORT+'.critical-errors.jsonl',JSON.stringify(entry)+'\n');
  if(diagnostics.length>=128&&critical){
   const discard=diagnostics.findIndex(item=>!item.errorDetails);
   diagnostics.splice(discard<0?0:discard,1);
  }
  if(diagnostics.length<128)diagnostics.push(entry);
  console.error(JSON.stringify(entry));};
 // Full logs stay on disk; only a bounded tail is retained for failure receipts.
 // A multi-hour validation must not retain every native warning in JS memory.
 const retainStderr=(peer,tail,data)=>{
  if(process.env.PARTYBOARD_NATIVE_ASSET_REPORT)
   fs.appendFileSync(process.env.PARTYBOARD_NATIVE_ASSET_REPORT+'.'+peer+'.stderr.log',data);
  return (tail+data.toString()).slice(-1024*1024);
 };
 native.stderr.on('data',data=>{stderr=retainStderr('native-host',stderr,data);});
 const fail=e=>{failure=e;hostMatchObserver?.close(e);for(const w of pending.splice(0)){clearTimeout(w.timer);w.reject(e);}};
 native.stdin.on('error',fail);
 native.on('error',fail);const exited=new Promise(resolve=>native.on('exit',(code,signal)=>{fail(Error('Native exited '+code));resolve({code,signal});}));
 createInterface({input:native.stdout}).on('line',line=>{try{if(frontend){const prefix=['Native frontend invitation: ','Native frontend event: ','Native owned lobby prepared receipt: '].find(p=>line.startsWith(p));if(!prefix)return;line=line.slice(prefix.length);}const e=JSON.parse(line);retainEvent(events,e);if(e.type==='diagnostic-stop-reason')diagnostic('native-stop-reason',e.reason);hostMatchObserver?.receive(e);if(e.type==='error')return fail(Error(e.message));for(let i=pending.length-1;i>=0;i--)if(pending[i].type===e.type){const w=pending.splice(i,1)[0];clearTimeout(w.timer);w.resolve(e);}}catch(e){fail(e);}});
 const consumed=new Set();
 const wait=type=>new Promise((resolve,reject)=>{const old=events.find(e=>e.type===type&&!consumed.has(e));if(old){consumed.add(old);return resolve(old);}if(failure)return reject(failure);const w={type,resolve:e=>{consumed.add(e);resolve(e);},reject};w.timer=setTimeout(()=>{pending.splice(pending.indexOf(w),1);reject(Error('Native timeout '+type));},600000);pending.push(w);});
 let joinChild,joinExited,joinStderr='',joinBootstrap,joinFailure;
 const joinEvents=[],joinPending=[],joinConsumed=new Set();
 const failJoin=error=>{joinFailure=error;joinMatchObserver?.close(error);for(const waiter of joinPending.splice(0)){clearTimeout(waiter.timer);waiter.reject(error);}};
 const waitJoin=type=>new Promise((resolve,reject)=>{
  const old=joinEvents.find(event=>event.type===type&&!joinConsumed.has(event));
  if(old){joinConsumed.add(old);return resolve(old);}
  if(joinFailure)return reject(joinFailure);
  const waiter={type,resolve:event=>{joinConsumed.add(event);resolve(event);},reject};
  waiter.timer=setTimeout(()=>{joinPending.splice(joinPending.indexOf(waiter),1);reject(Error('Native Join timeout '+type));},600000);joinPending.push(waiter);
 });
 try{
  if(externalJoin){
   joinChild=spawn(process.env.PARTYBOARD_NATIVE_ASSET_PEER,[menuJoin?'--probe-join-menu':'--probe-join-external','--native-session-package',process.env.PARTYBOARD_NATIVE_FRONTEND_PACKAGE,'--user-dir',process.env.PARTYBOARD_NATIVE_EXTERNAL_JOIN_PROFILE],{env:nativeLaunchEnv({...nativeCaptureEnv('guest'),...(process.env.PARTYBOARD_NATIVE_GUEST_CRASH_DUMP?{PARTYBOARD_NATIVE_CRASH_DUMP:process.env.PARTYBOARD_NATIVE_GUEST_CRASH_DUMP}:{})}),cwd:path.dirname(process.env.PARTYBOARD_NATIVE_ASSET_PEER),stdio:['pipe','pipe','pipe']});
   if(fullMatch)joinMatchObserver=createNativeMatchObserver({timeoutMs:120000,send:packet=>new Promise((resolve,reject)=>joinChild.stdin.write(JSON.stringify(packet)+'\n',error=>error?reject(error):resolve()))});
   joinChild.stderr.on('data',data=>{joinStderr=retainStderr('native-guest1',joinStderr,data);});
   createInterface({input:joinChild.stderr}).on('line',line=>randomTrace('native-guest1',line));
   joinChild.stdin.on('error',error=>{failJoin(error);fail(error);});
   joinChild.on('error',error=>{fail(error);failJoin(error);});
   joinExited=new Promise(resolve=>joinChild.once('exit',(code,signal)=>{
    failJoin(Error('Native Join exited '+code));resolve({code,signal});
   }));
   createInterface({input:joinChild.stdout}).on('line',line=>{try{
    const prefix='Native external Join event: ';if(!line.startsWith(prefix))return;
    const event=JSON.parse(line.slice(prefix.length));retainEvent(joinEvents,event);
    joinMatchObserver?.receive(event);
    for(let i=joinPending.length-1;i>=0;i--)if(joinPending[i].type===event.type){const waiter=joinPending.splice(i,1)[0];clearTimeout(waiter.timer);waiter.resolve(event);}
   }catch(error){fail(error);}});
   await waitJoin('join-awaiting-invitation');
  }
  if(prepareFrontend){
   const artifacts=new Map(readPackage().files.map(file=>['/'+file.name,file.path]));
   artifactServer=http.createServer((request,response)=>{
    if(observedPipelineSeed&&new URL(request.url,'http://127.0.0.1').pathname==='/observed-pipeline-seed.db'&&['GET','HEAD'].includes(request.method)){
     response.writeHead(200,{'Access-Control-Allow-Origin':origin,'Cache-Control':'no-store','Content-Type':'application/octet-stream','Content-Length':observedPipelineSeed.bytes.length});
     response.end(request.method==='HEAD'?undefined:observedPipelineSeed.bytes);return;
    }
    const filename=artifacts.get(new URL(request.url,'http://127.0.0.1').pathname);
    if(!filename||!['GET','HEAD'].includes(request.method)){response.writeHead(404);response.end();return;}
    response.setHeader('Access-Control-Allow-Origin',origin);response.setHeader('Cache-Control','no-store');
    response.setHeader('Content-Type',filename.endsWith('.wasm')?'application/wasm':filename.endsWith('.mjs')?'text/javascript':'application/octet-stream');
    fs.stat(filename,(error,stat)=>{if(error||!stat.isFile()){response.writeHead(500);response.end();return;}
     response.setHeader('Content-Length',stat.size);if(request.method==='HEAD'){response.end();return;}
     fs.createReadStream(filename).on('error',()=>response.destroy()).pipe(response);
    });
   });
   await new Promise((resolve,reject)=>{artifactServer.once('error',reject);artifactServer.listen(0,'127.0.0.1',resolve);});
   artifactOrigin='http://127.0.0.1:'+artifactServer.address().port+'/';
  }
  const browserDirectory=prepareFrontend?path.dirname(readPackage().files.find(file=>file.name==='contract.mjs').path):undefined;
  server=spawn(process.execPath,[path.resolve(__dirname,'../../web/serve.cjs')],{env:{...process.env,PARTYBOARD_TEST_PORT:port,...(browserDirectory?{PARTYBOARD_RUNTIME_DIRECTORY:browserDirectory}:{})},stdio:['ignore','pipe','pipe']});
  for(let i=0;i<100;i++){try{if((await fetch(origin+'/transfer.mjs')).ok)break;}catch{}if(i===99)throw Error('Server unavailable');await new Promise(r=>setTimeout(r,50));}
  const browserChannel=process.env.PARTYBOARD_TEST_BROWSER_CHANNEL||'msedge';
  browserServer=await chromium.launchServer({channel:browserChannel,headless:true,args:['--mute-audio']});browser=await chromium.connect(browserServer.wsEndpoint());browser.on('disconnected',()=>diagnostic('browser-disconnected',browserChannel+' disconnected'));
  const initialInvitations=[];
  if(initial)for(let index=0;index<count;index++)initialInvitations.push(await wait('initial-invitation'));
  const orderedInvitations=initial?require('./initial-invitation-order.cjs').orderInitialInvitations(initialInvitations,count):null;
  for(let slot=1;slot<=count;slot++){
  const bootstrap=initial?orderedInvitations[slot-1]:await wait('original-credential'),offer=initial?null:(await wait('offer')).packet;
  if(externalJoin&&slot===1){
   joinBootstrap=bootstrap;joinChild.stdin.write(JSON.stringify({type:'join-invitation',code:bootstrap.code})+'\n');
   const answer=await waitJoin('join-reply');assert.equal(answer.peer,'guest1');native.stdin.write(answer.code+'\n');continue;
  }
  const page=await(await browser.newContext()).newPage();
  if(process.env.PARTYBOARD_BROWSER_DEVICE_LOSS_PROBE==='1')await page.addInitScript(()=>{
   const requestDevice=GPUAdapter.prototype.requestDevice;
   globalThis.partyBoardProbeGpuDevices=[];
   GPUAdapter.prototype.requestDevice=async function(...args){
    const device=await requestDevice.apply(this,args);
    partyBoardProbeGpuDevices.push(device);return device;
   };
  });
  page.on('crash',()=>{const error=Error('Browser guest'+slot+' crashed');diagnostic('page-crash',error.message,error);if(stage!=='cleanup')fail(error);});page.on('close',()=>diagnostic('page-close','guest'+slot));
  page.on('pageerror',error=>{diagnostic('page-error',error.message,error);if(stage!=='cleanup')fail(error);});
  page.on('response',response=>{if(response.status()>=400)diagnostic('http-error',response.status()+' '+response.url());});
  page.on('console',message=>{if(randomTrace('browser-guest'+slot,message.text()))return;
   if(['warning','error'].includes(message.type())&&(diagnostics.length<128||/device lost|device loss|fatal|aborted/i.test(message.text())))diagnostic('console-'+message.type(),message.text());});
  await page.goto(origin);if(prepareFrontend){
   await page.evaluate(value=>{globalThis.nativeCustomKeyboardProbe=value;},process.env.PARTYBOARD_NATIVE_CUSTOM_KEYBOARD==='1');
   await page.evaluate(value=>{globalThis.nativeRuntimeArtifactOrigin=value;},artifactOrigin);
   await page.evaluate(value=>{globalThis.nativeRandomTraceRange=value;},{
    from:process.env.PARTYBOARD_RNG_TRACE_FROM ? Number(process.env.PARTYBOARD_RNG_TRACE_FROM) : undefined,
    to:process.env.PARTYBOARD_RNG_TRACE_TO ? Number(process.env.PARTYBOARD_RNG_TRACE_TO) : undefined});
   if(observedPipelineSeed){
    await page.evaluate(value=>{globalThis.nativePipelineSeedHash=value;},observedPipelineSeed.sha256);
    await page.evaluate(value=>{globalThis.nativeExpectedSeedPipelines=value;},expectedSeedPipelines);
   }
  }peers.push({page,bootstrap});
  const answer=await page.evaluate(async({credential,offer,initial,code,peer,lobbyMode,manifest})=>{
   const recovery=await import('/recovery-identity.mjs'),transfer=await import('/transfer.mjs');
   const original=initial?null:recovery.receiveRecoveryCredential(credential,credential);
   const invitation=initial?JSON.parse(atob(code)):null;
   if(initial&&(invitation.version!==1||invitation.playerId!==peer||invitation.description.type!=='offer'))throw Error('Invalid assigned invitation');
   const verified=initial?invitation:await recovery.verifyRecoveryDescription(offer,original,{type:'offer',usedNonces:new Set()});if(!verified)throw Error('Invalid signed offer');
   globalThis.assetStore=await transfer.browserStore();globalThis.assetProgress=0;globalThis.assetFailure=null;globalThis.assetComplete=null;globalThis.assetConnection=new RTCPeerConnection({iceServers:[]});
   globalThis.transportEvents=[];
   const recordTransport=(type,channel)=>{if(transportEvents.length>=128)transportEvents.shift();transportEvents.push({at:performance.now(),observedAt:new Date().toISOString(),type,connection:assetConnection.connectionState,ice:assetConnection.iceConnectionState,label:channel?.label,state:channel?.readyState});};
   for(const type of ['connectionstatechange','iceconnectionstatechange'])assetConnection.addEventListener(type,()=>recordTransport(type));
   globalThis.controlChannels=[];
   globalThis.inputChannel=null;
   globalThis.receivedConfiguration=null;globalThis.configurationMessages=0;
   globalThis.originalRecoveryCredential=null;globalThis.recoveryCredentialMessages=0;
   const {receiveSessionConfiguration}=await import('/session-config.mjs');
   const {LobbyClient}=await import('/lobby-client.mjs');const client=new LobbyClient(manifest.id,manifest.size);
   let lobbyChannel=null,sequence=0,reportedBytes=-1048576,verifiedAsset=false;globalThis.assetLobbySnapshot=null;
   const reportProgress=()=>{if(!lobbyMode||lobbyChannel?.readyState!=='open'||(!verifiedAsset&&assetProgress-reportedBytes<1048576))return;reportedBytes=assetProgress;lobbyChannel.send(JSON.stringify({type:'lobby-report',version:1,report:{assetId:manifest.id,sequence:++sequence,bytes:assetProgress,verified:verifiedAsset,shaders:false,ready:false}}));};
   globalThis.assetReportReady=()=>{
    if(!verifiedAsset||!nativeGuestRuntime||lobbyChannel?.readyState!=='open'||!assetLobbySnapshot)throw Error('Actual prepared assigned guest required');
    lobbyChannel.send(JSON.stringify({type:'lobby-report',version:1,report:{assetId:manifest.id,sequence:++sequence,bytes:manifest.size,verified:true,shaders:true,ready:true,membership:assetLobbySnapshot.membership}}));
   };
   assetConnection.ondatachannel=event=>{
    for(const type of ['open','close','error'])event.channel.addEventListener(type,()=>recordTransport(type,event.channel));
    if(event.channel.label==='partyboard-input-v1'){inputChannel=event.channel;return;}
    if(event.channel.label!=='partyboard-assets-v1'){
     controlChannels.push(event.channel);
     if(event.channel.label==='partyboard-preparation-v1')event.channel.onmessage=event=>{
      try{
       const packet=JSON.parse(event.data);
       if(packet.type==='recovery-credential'){
        if(!receivedConfiguration)throw Error('Original configuration required before recovery credential');
        originalRecoveryCredential=recovery.receiveRecoveryCredential(packet,{session:receivedConfiguration.session,assetId:manifest.id,peer,current:originalRecoveryCredential});
        recoveryCredentialMessages++;return;
       }
       receivedConfiguration=receiveSessionConfiguration(event.data,{assetId:manifest.id,current:receivedConfiguration});configurationMessages++;
      }
      catch(error){assetFailure=String(error);}
     };
     if(lobbyMode&&event.channel.label==='partyboard-lobby-v1'){
      lobbyChannel=event.channel;lobbyChannel.onopen=reportProgress;
      lobbyChannel.onmessage=event=>{try{const snapshot=client.receive(event.data);if(snapshot)assetLobbySnapshot=snapshot;}catch(error){assetFailure=String(error);}};
      reportProgress();
     }return;
    }
    globalThis.assetChannel=event.channel;let receiver;
    receiver=transfer.guest(assetChannel,assetStore,{offer:()=>receiver.accept().catch(e=>assetFailure=String(e)),error:e=>assetFailure=String(e),progress:bytes=>{assetProgress=bytes;reportProgress();},complete:m=>{assetComplete=m;assetProgress=m.size;verifiedAsset=true;reportProgress();}});
   };
   await assetConnection.setRemoteDescription(verified.description);await assetConnection.setLocalDescription(await assetConnection.createAnswer());
   if(assetConnection.iceGatheringState!=='complete')await new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('ICE timeout')),30000);assetConnection.addEventListener('icegatheringstatechange',()=>{if(assetConnection.iceGatheringState==='complete'){clearTimeout(timer);resolve();}});});
   const description={type:'answer',sdp:assetConnection.localDescription.sdp};
   return initial?btoa(JSON.stringify({version:1,playerId:invitation.playerId,description})):recovery.signRecoveryDescription(original,description,{nonce:offer.nonce});
  },{credential:bootstrap.credential,offer,initial,code:bootstrap.code,peer:'guest'+slot,lobbyMode,manifest:bootstrap.manifest});
  native.stdin.write((initial?answer:JSON.stringify({type:'answer',packet:answer}))+'\n');
  }
  stage='asset-transfer';await wait('open');const completed=await wait('complete');const observations=[];
  for(const {page,bootstrap} of peers){
  await page.waitForFunction(()=>assetComplete||assetFailure,{},{timeout:600000});assert.equal(await page.evaluate(()=>assetFailure),null);
  const observed=await page.evaluate(async()=>{const m=assetComplete;assetStore.close();assetStore=await (await import('/transfer.mjs')).browserStore();let size=0;for(let i=0;i<m.count;i++){const c=await assetStore.get(m.id,i);if(!c)throw Error('Missing persisted chunk');size+=c.bytes.byteLength;}return {manifest:m,persisted:await assetStore.manifest(m.id),size,connection:assetConnection.connectionState};});
  assert.deepEqual(observed.manifest,bootstrap.manifest);assert.deepEqual(observed.persisted,bootstrap.manifest);assert.equal(observed.size,bootstrap.manifest.size);assert.equal(observed.connection,'connected');
  if(initial){
   const labels=['partyboard-lobby-v1','partyboard-preparation-v1','partyboard-recovery-v1','partyboard-start-v1'];
   if(frontend&&observedPipelineSeed)labels.push('partyboard-pipelines-v1');
   labels.sort((a,b)=>a.localeCompare(b));
   assert.deepEqual(await page.evaluate(()=>controlChannels.map(c=>({label:c.label,state:c.readyState})).sort((a,b)=>a.label.localeCompare(b.label))),labels.map(label=>({label,state:'open'})));
  }
  if(initial)assert.equal(await page.evaluate(()=>inputChannel?.readyState),'open');
  if(sessionMode){
   await page.waitForFunction(()=>receivedConfiguration,{},{timeout:30000});
   const configuration=await page.evaluate(()=>receivedConfiguration);
   assert.equal(configuration.assetId,bootstrap.manifest.id);
   if(frontend){assert.match(configuration.session,/^[a-f0-9]{32}$/);assert.ok(Number.isInteger(configuration.seed));assert.equal(configuration.buildId,readPackage().configuration.buildId);}
   else{assert.equal(configuration.session,'1'.repeat(32));assert.equal(configuration.seed,123);assert.equal(configuration.buildId,'2'.repeat(64));}
   assert.equal(await page.evaluate(()=>configurationMessages),1);
   if(frontend){
    await page.waitForFunction(()=>originalRecoveryCredential||assetFailure,{},{timeout:30000});
    assert.equal(await page.evaluate(()=>assetFailure),null);
    assert.equal(await page.evaluate(()=>recoveryCredentialMessages),1);
    assert.equal(await page.evaluate(()=>originalRecoveryCredential.session),configuration.session);
   }
  }
  if(lobbyMode){await page.waitForFunction(()=>assetLobbySnapshot?.players.length===4&&assetLobbySnapshot.players.every(p=>p.verified),{},{timeout:30000});assert.deepEqual(await page.evaluate(()=>assetLobbySnapshot),completed.lobbySnapshot);assert.equal(completed.lobbySnapshot.canStart,false);assert.ok(completed.lobbySnapshot.players.every(p=>!p.ready&&(p.shaders===(frontend&&p.peer==='host'))));}
  observations.push(observed);
  }
  const preparedReceipts=[];
  if(frontend){
   const secrets=await Promise.all(peers.map(({page})=>page.evaluate(()=>originalRecoveryCredential.secret)));
   assert.equal(new Set(secrets).size,peers.length);
  }
  let nativeAcceptedPreparedReceipts=false;
  let gameplayEvidence;
  let mixedFullMatchEvidence=null;
  let nativeLobbyMusicEvidence=null;
  const browserShaderPreparation=[];
  const browserAudioPreparation=[];
  const browserProfilePreparation=[];
  if(prepareFrontend){
   const hostReceipt=await wait('runtime-prepared');
   if(process.env.PARTYBOARD_PROBE_DESCRIPTOR_SEED==='1')assert.equal(hostReceipt.seed,readPackage().configuration.seed,'Diagnostic RNG seed must match the measured descriptor');
   if(process.env.PARTYBOARD_NATIVE_ASSET_REPORT)
    fs.writeFileSync(process.env.PARTYBOARD_NATIVE_ASSET_REPORT+'.preparation-receipt.json',JSON.stringify(hostReceipt,null,2)+'\n',{flag:'wx'});
   if(externalJoin){const prepared=await waitJoin('join-engine-prepared');assert.equal(prepared.peer,'guest1');assert.deepEqual(prepared.receipt,hostReceipt);
    if(verifyLobbyMusic){assert.equal(prepared.music.active,true);assert.equal(prepared.music.error,0);assert.ok(prepared.music.decodedFrames.reduce((sum,count)=>sum+count,0)>0);assert.ok([2,6].includes(prepared.music.sourceChannels));assert.ok(prepared.music.queuedBytes>=0&&prepared.music.queuedBytes<=48000*prepared.music.sourceChannels*2*12/100);
     nativeLobbyMusicEvidence={prepared:prepared.music,committed:null,outputLifecycleVerified:false,audibilityVerified:false};}
    preparedReceipts.push(prepared.receipt);}
   for(const {page,bootstrap} of peers){
    stage='browser-engine-preparation-'+(preparedReceipts.length+1);
    console.log('Preparing actual connected Edge guest '+(preparedReceipts.length+1));
    const receipt=await page.evaluate(async({audioChannels})=>{
     const retained=receivedConfiguration;if(!retained||!assetComplete)throw Error('Retained configuration and verified assets required');
     const {loadBrowserRuntimePackage}=await import('/browser-runtime-package.mjs');
     const files=await loadBrowserRuntimePackage({baseURL:nativeRuntimeArtifactOrigin,supplementalArtifacts:retained.runtimeArtifacts});
     const {browserPlatformCallbacks}=await import('/browser-platform.mjs');
     const {prepareFingerprintedSessionRuntime}=await import('/session-runtime.mjs');
     let pipelineSeed;
     if(globalThis.nativePipelineSeedHash){
      const response=await fetch(new URL('observed-pipeline-seed.db',nativeRuntimeArtifactOrigin));
      if(!response.ok)throw Error('Observed pipeline seed fetch failed');
      pipelineSeed={blob:await response.blob(),sha256:nativePipelineSeedHash,scope:'observed-scenes-only'};
     }
     const platform=browserPlatformCallbacks({maxSteps:pipelineSeed?10000:60,audioChannels});
     globalThis.nativeGuestRuntime=await prepareFingerprintedSessionRuntime(assetStore,retained.assetId,files,{
      runtimeBuild:{expectedBuildId:retained.buildId},sessionSeed:retained.seed,pipelineSeed,
      runtimeOptions:{canvas:document.querySelector('canvas'),partyBoardRandomTraceFrom:globalThis.nativeRandomTraceRange?.from,
       partyBoardRandomTraceTo:globalThis.nativeRandomTraceRange?.to,print:()=>{},printErr:message=>console.error(String(message))},setup:platform.setup,prepareRenderer:platform.prepareRenderer,shutdown:platform.shutdown});
     const {preparedRuntimeReport}=await import('/prepared-runtime-report.mjs');
     if(globalThis.nativeCustomKeyboardProbe){
      const {defaultControlBindings}=await import('/control-bindings.mjs');
      const {applyRuntimeKeyboardBindings}=await import('/runtime-control-bindings.mjs');
      const profile=defaultControlBindings();profile.keyboard.a='Space';
      applyRuntimeKeyboardBindings(nativeGuestRuntime.module,profile);
      globalThis.nativeCustomKeyboardProfile=profile;
     }
     if(pipelineSeed){
      if(nativeGuestRuntime.pipelineSeed?.sha256!==nativePipelineSeedHash||nativeGuestRuntime.rendererPreparation?.queued!==0||
       !Number.isSafeInteger(nativeGuestRuntime.rendererPreparation?.created)||
       nativeGuestRuntime.rendererPreparation?.created<nativeExpectedSeedPipelines)throw Error('Observed seed pipelines not prepared before readiness');
     }
     const receipt=preparedRuntimeReport(nativeGuestRuntime,retained);
     const channel=controlChannels.find(c=>c.label==='partyboard-preparation-v1');
     if(channel?.readyState!=='open')throw Error('Assigned preparation channel unavailable');
     channel.send(JSON.stringify(receipt));return receipt;
    },{audioChannels:localAudioChannels(localAudioLayouts,bootstrap.peer)});
    assert.deepEqual(receipt,hostReceipt);preparedReceipts.push(receipt);
    if(process.env.PARTYBOARD_NATIVE_PROFILE_LAYOUT){
     const layout=JSON.parse(fs.readFileSync(process.env.PARTYBOARD_NATIVE_PROFILE_LAYOUT,'utf8'));
     const observation=await page.evaluate('('+readPreparedBrowserProfile.toString()+')({runtime:globalThis.nativeGuestRuntime,layout:'+JSON.stringify(layout)+'})');
     assert.equal(observation.checkpoint,receipt.checkpoint);
     browserProfilePreparation.push({peer:bootstrap.peer,...observation});
     if(process.env.PARTYBOARD_NATIVE_ASSET_REPORT)fs.writeFileSync(process.env.PARTYBOARD_NATIVE_ASSET_REPORT+'.profile-preparation.json',
      JSON.stringify({scope:'Read-only actual normal-platform prepared browser profile; no reset or save/restore calls; boot screens unverified',guests:browserProfilePreparation},null,2)+'\n');
    }
    const audio=await page.evaluate(()=>({sourceChannels:nativeGuestRuntime.module._PartyBoardBrowserAudioSourceChannels?.()??0,
     deviceChannels:nativeGuestRuntime.module._PartyBoardBrowserAudioDeviceChannels?.()??0,surroundRouting:nativeGuestRuntime.module._PartyBoardBrowserAudioSynthMetric?.(13)??0,
     browserDestination:nativeGuestRuntime.module.SDL3?.audioContext?.destination?.channelCount}));
    const requested=localAudioChannels(localAudioLayouts,bootstrap.peer);
    if(requested!==undefined)assert.equal(audio.sourceChannels,requested);
    browserAudioPreparation.push({peer:bootstrap.peer,requested,...audio});
    if(process.env.PARTYBOARD_NATIVE_ASSET_REPORT)fs.writeFileSync(process.env.PARTYBOARD_NATIVE_ASSET_REPORT+'.audio-preparation.json',JSON.stringify({scope:'Local browser source/device settings before readiness; physical speakers unverified',guests:browserAudioPreparation},null,2)+'\n');
    browserShaderPreparation.push(await page.evaluate(()=>({seed:nativeGuestRuntime.pipelineSeed??null,renderer:nativeGuestRuntime.rendererPreparation,fullCoverage:false})));
    if(process.env.PARTYBOARD_NATIVE_ASSET_REPORT)fs.writeFileSync(process.env.PARTYBOARD_NATIVE_ASSET_REPORT+'.shader-preparation.json',JSON.stringify({scope:'browser guests only; observed pipelines, not full game coverage',guests:browserShaderPreparation},null,2)+'\n');
    console.log('Actual connected Edge guest prepared at frame zero; receipt matches native host.');
   }
   const accepted=await wait('prepared-guests');
   assert.equal(accepted.frame,0);assert.equal(accepted.phase,2);
   assert.deepEqual(Object.keys(accepted.receipts).sort(),['guest1','guest2','guest3','host']);
   for(const receipt of Object.values(accepted.receipts))assert.deepEqual(receipt,hostReceipt);
   nativeAcceptedPreparedReceipts=true;
   if(startFrontend){
    for(const {page,bootstrap} of peers)await page.evaluate(async({peer,recoveryTest})=>{
     const runtime=nativeGuestRuntime,configuration=receivedConfiguration;
     const {lobbyInputOwnership}=await import('/lobby-input-ownership.mjs');
     const {createLobbyInputRouter}=await import('/lobby-input-router.mjs');
     const {prepareLobbyGameInput}=await import('/lobby-game-input.mjs');
     const {prepareStartAcknowledgment,resumeCommittedClientRuntime}=await import('/start-client.mjs');
     const plan=lobbyInputOwnership(assetLobbySnapshot,peer),session=Uint8Array.from(configuration.session.match(/../g),byte=>parseInt(byte,16));
     let router;
     globalThis.nativeGuestInput=prepareLobbyGameInput(runtime,{plan,session,bind:(receive,disconnected)=>(router=createLobbyInputRouter({plan,session,channels:new Map([['host',inputChannel]]),receive,disconnected,error:(_,error)=>{assetFailure=String(error);}}))});
     runtime.inputExchangeInstalled=true;
     let prepared;globalThis.nativeGuestExecution=null;globalThis.nativeGuestCommit=null;
     if(recoveryTest){const {nativeGameplayRecoveryTestSupport}=await import('/native-gameplay-recovery-test-support.mjs');
      globalThis.nativeRecoverySupport=nativeGameplayRecoveryTestSupport({runtime,input:nativeGuestInput,router,plan,configuration,credential:originalRecoveryCredential,
       control:controlChannels.find(channel=>channel.label==='partyboard-recovery-v1'),inputChannel,identity:()=>nativeGuestCommit,error:error=>{assetFailure=String(error);}});}
     const channel=controlChannels.find(c=>c.label==='partyboard-start-v1');
     if(channel?.readyState!=='open')throw Error('Assigned start channel unavailable');
     channel.addEventListener('message',async event=>{
      try{
       const packet=JSON.parse(event.data);
       if(packet.type==='session-start'){
        prepared=await prepareStartAcknowledgment(packet,{peer,buildId:runtime.buildId,snapshot:()=>assetLobbySnapshot,prepare:async()=>runtime,requireOnlineInput:true});
        channel.send(JSON.stringify(prepared.acknowledgment));
       }else if(packet.type==='session-start-commit'){
        if(nativeGuestCommit)throw Error('Duplicate native commit');
        nativeGuestCommit=packet;
        nativeGuestExecution=resumeCommittedClientRuntime(packet,prepared,{peer,buildId:runtime.buildId,snapshot:()=>assetLobbySnapshot,requireOnlineInput:true});
        nativeGuestExecution.catch(error=>{
         assetFailure=String(error);
         globalThis.nativeGuestExecutionFailure={name:String(error?.name||'Error'),message:String(error?.message||error),stack:String(error?.stack||'')};
        });
       }else throw Error('Unexpected native start packet');
      }catch(error){assetFailure=String(error);}
     });
     assetReportReady();
    },{peer:bootstrap.peer,recoveryTest:recoverFrontend});
    if(externalJoin)joinChild.stdin.write('ready\n');
    native.stdin.write('ready\n');
    for(const {page} of peers)await page.waitForFunction(()=>assetFailure||assetLobbySnapshot?.canStart,{},{timeout:30000});
    for(const {page} of peers)assert.equal(await page.evaluate(()=>assetFailure),null);
    native.stdin.write('start\n');
    for(const {page} of peers)await page.waitForFunction(()=>assetFailure||nativeGuestCommit,{},{timeout:30000});
    for(const {page} of peers)assert.equal(await page.evaluate(()=>assetFailure),null);
    if(externalJoin){
     const installed=await waitJoin('join-input-installed');assert.equal(installed.phase,2);assert.equal(installed.frame,0);
     if(verifyLobbyMusic){assert.equal(installed.music.active,false);assert.equal(installed.music.queuedBytes,0);assert.equal(installed.music.error,0);
      nativeLobbyMusicEvidence.committed=installed.music;nativeLobbyMusicEvidence.outputLifecycleVerified=true;}
     assert.deepEqual(installed.commit,await peers[0].page.evaluate(()=>nativeGuestCommit));
     joinChild.stdin.write('run\n');
    }
    let completedRecoveryRounds=0;
    const recoverAtTurn=async()=>{
     stage='native-gameplay-recovery';const guest=peers[0];
     const recoveryRound=completedRecoveryRounds;
     await guest.page.waitForFunction(threshold=>assetFailure||nativeGuestInput.observations().length>=threshold,5+20*recoveryRound,{timeout:30000});
     assert.equal(await guest.page.evaluate(()=>assetFailure),null);
     if(nativeSelfRecovery){joinChild.stdin.write('disconnect\n');await waitJoin('join-disconnected');}
     else await guest.page.evaluate(()=>nativeRecoverySupport.disconnect());
     const paused=await wait('recovery-status');assert.equal(paused.status.paused,true);assert.ok(paused.status.remainingMs>0&&paused.status.remainingMs<=60000);
     for(const {page} of peers)await page.waitForFunction(()=>assetFailure||nativeRecoverySupport.status().state==='Paused',{},{timeout:5000});
     for(const {page} of peers)assert.equal(await page.evaluate(()=>assetFailure),null);
     const recoveringPeer=nativeSelfRecovery?joinBootstrap.peer:guest.bootstrap.peer;
     native.stdin.write(JSON.stringify({type:'recovery-begin',peer:recoveringPeer})+'\n');
     const offered=await wait('recovery-offer');assert.equal(offered.peer,recoveringPeer);
     let answer;
     if(nativeSelfRecovery){
      joinChild.stdin.write(JSON.stringify({type:'join-recovery-offer',offer:offered.offer})+'\n');
      answer=(await waitJoin('join-recovery-answer')).answer;
     }else answer=await guest.page.evaluate(offer=>nativeRecoverySupport.acceptOffer(offer),offered.offer);
     native.stdin.write(JSON.stringify({type:'recovery-answer',peer:recoveringPeer,answer})+'\n');
     const verified=await wait('recovery-verified');assert.equal(verified.peer,recoveringPeer);assert.equal(verified.status.paused,true);
     if(!nativeSelfRecovery){const browserVerified=await guest.page.evaluate(()=>nativeRecoverySupport.verified());assert.equal(browserVerified.paused,true);}
     native.stdin.write(JSON.stringify({type:'recovery-resume',peer:recoveringPeer,owner:paused.status.owner})+'\n');
     try{
      for(const {page} of peers)await page.waitForFunction(expected=>assetFailure||nativeRecoverySupport.commits()===expected,recoveryRound+1,{timeout:30000});
     }catch(error){
      const states=await Promise.allSettled(peers.map(({page})=>page.evaluate(()=>({
       failure:assetFailure,commits:nativeRecoverySupport.commits(),status:nativeRecoverySupport.status()
      }))));
      diagnostic('recovery-commit-timeout',JSON.stringify({round:recoveryRound+1,peers:states.map(result=>result.status==='fulfilled'?result.value:{error:String(result.reason)})}));
      throw error;
     }
     for(const {page} of peers){assert.equal(await page.evaluate(()=>assetFailure),null);assert.equal(await page.evaluate(()=>nativeRecoverySupport.status().configurationRetained),true);}
     const resumed=await wait('recovery-status');assert.equal(resumed.status.paused,false);
     completedRecoveryRounds++;
    };
    if(recoverFrontend&&!recoveryTurns.length)for(let round=0;round<recoveryRounds;round++)await recoverAtTurn();
    if(process.env.PARTYBOARD_BROWSER_DEVICE_LOSS_PROBE==='1'){
     stage='browser-device-loss-validation';
     const result=await peers[0].page.evaluate(async()=>{
      const device=partyBoardProbeGpuDevices.at(-1);
      if(!device)throw Error('Actual browser WebGPU device was not captured');
      const outcome=nativeGuestExecution.then(()=>({resolved:true}),error=>({error:String(error)}));
      device.destroy();
      const lost=await device.lost;
      const execution=await Promise.race([outcome,new Promise((_,reject)=>setTimeout(()=>reject(Error('Abort did not settle execution')),10000))]);
      const phase=nativeGuestRuntime.engine.phase;
      let cleanupError;
      try {await Promise.race([nativeGuestRuntime.dispose(),new Promise((_,reject)=>setTimeout(()=>reject(Error('Aborted runtime cleanup timed out')),10000))]);}
      catch(error){cleanupError=String(error);}
      return {deviceReason:lost.reason,deviceMessage:lost.message,execution,phase,
       disposed:nativeGuestRuntime.module===undefined,cleanupError};
     });
     fs.writeFileSync(process.env.PARTYBOARD_NATIVE_ASSET_REPORT+'.device-loss-state.json',JSON.stringify(result,null,2)+'\n');
     assert.equal(result.phase,'Failed');assert.match(result.execution.error,/Engine aborted|Renderer stopped/);assert.equal(result.disposed,true);
     fs.writeFileSync(process.env.PARTYBOARD_NATIVE_ASSET_REPORT,JSON.stringify({scope:'Actual prepared browser runtime WebGPU device destruction and abort cleanup',result,gpuRecoveryVerified:false,fullMatchVerified:false},null,2)+'\n');
     return;
    }
    if(fullMatch){
     stage='mixed-full-match';
     const {createNativeMatchKeyboard}=require('./native-match-keyboard.cjs');
     const {driveMixedFullMatch,matchingRecords}=require('./drive-mixed-full-match.cjs');
     const send=child=>packet=>new Promise((resolve,reject)=>child.stdin.write(JSON.stringify(packet)+'\n',error=>error?reject(error):resolve()));
     const slotFor=peer=>{
      const player=completed.lobbySnapshot.players.find(player=>player.peer===peer);
      assert.ok(player&&Number.isInteger(player.slot)&&player.slot>=0&&player.slot<4,'Assigned lobby slot required for match driver');
      return player.slot;
     };
     const hostKeyboard=createNativeMatchKeyboard(send(native)),joinKeyboard=createNativeMatchKeyboard(send(joinChild));
     const participants=[{slot:slotFor('host'),observe:()=>hostMatchObserver.observe(),keyboard:hostKeyboard,releaseAll:()=>hostKeyboard.releaseAll()},
      {slot:slotFor(joinBootstrap.peer),observe:()=>joinMatchObserver.observe(),keyboard:joinKeyboard,releaseAll:()=>joinKeyboard.releaseAll()}];
     for(const {page} of peers){
      // This harness owns the prepared runtime directly instead of the app's
      // local-play UI owner; reveal its existing game view before input/capture.
      await page.evaluate(()=>{document.querySelector('#game-view').hidden=false;});
      await page.locator('canvas').click();
      const {createBoundMatchKeyboard,browserMatchMapping}=require('./bound-match-keyboard.cjs');
      const mapping=browserMatchMapping((await import('../../web/control-bindings.mjs')).defaultControlBindings());
      if(process.env.PARTYBOARD_NATIVE_CUSTOM_KEYBOARD==='1')mapping.z='Space';
      const keyboard=createBoundMatchKeyboard(page.keyboard,mapping);
      participants.push({slot:slotFor(peers.find(peer=>peer.page===page).bootstrap.peer),keyboard,capture:filename=>page.locator('canvas').screenshot({path:filename,timeout:10000}),releaseAll:async()=>{for(const key of new Set(Object.values(mapping)))await page.keyboard.up(key);},
       observeFrame:()=>page.evaluate(()=>nativeGuestRuntime.module._PartyBoard_GameFrameCount()),
       observeLottery:()=>page.evaluate(()=>{
        const module=nativeGuestRuntime.module;
        return {frame:module._PartyBoard_GameFrameCount(),fields:module._PartyBoard_GameLotteryField?
         Array.from({length:10},(_,field)=>module._PartyBoard_GameLotteryField(field)):null};
       }),
       observe:()=>page.evaluate(()=>{
        const module=nativeGuestRuntime.module;
        return {frame:module._PartyBoard_GameFrameCount(),progress:{turn:module._PartyBoard_GameProgress(0),maxTurn:module._PartyBoard_GameProgress(1),
         board:module._PartyBoard_GameProgress(2),player:module._PartyBoard_GameProgress(3),overlay:module._PartyBoard_GameProgress(4),event:module._PartyBoard_GameProgress(5),
         menuBoard:module._PartyBoard_GameProgress(6),menuPort:module._PartyBoard_GameProgress(7),interactionStage:module._PartyBoard_GameProgress(8),
         partySetupTurns:module._PartyBoard_GameProgress(9),partySetupRow:module._PartyBoard_GameProgress(10),partySetupPort:module._PartyBoard_GameProgress(11),
         mapView:!!module._PartyBoardBrowserMapViewActive(),mapViewPort:module._PartyBoardBrowserMapViewPort()},
         observations:nativeGuestInput.observations(),
         checkpointComponents:[0,1,2,3].map(field=>module._PartyBoard_GameCheckpointComponent(field)),
         pollCheckpointComponents:nativeGuestInput.pollCheckpointComponents(),
         sequenceFields:module._PartyBoard_GameSequenceField?
          Array.from({length:module._PartyBoard_GameSequenceField(-1)},(_,field)=>module._PartyBoard_GameSequenceField(field)):null,
         sequenceTrace:module._PartyBoard_GameSequenceTrace?
          Array.from({length:module._PartyBoard_GameSequenceTrace(-1,0)},(_,row)=>
           Array.from({length:module._PartyBoard_GameSequenceTrace(0,-1)},(_,field)=>module._PartyBoard_GameSequenceTrace(row,field))):null,
         itemEventStage:module._PartyBoard_GameItemEventStage?.(),
         lotteryFields:module._PartyBoard_GameLotteryField?
          Array.from({length:10},(_,field)=>module._PartyBoard_GameLotteryField(field)):null,
         globalCheckpointFields:module._PartyBoard_GameGlobalCheckpointField?
          Array.from({length:module._PartyBoard_GameGlobalCheckpointField(-1)},(_,field)=>module._PartyBoard_GameGlobalCheckpointField(field)):null,
         transport:{events:transportEvents.slice(),connection:assetConnection.connectionState,ice:assetConnection.iceConnectionState,input:inputChannel?.readyState,inputBufferedBytes:inputChannel?.bufferedAmount,controls:controlChannels.map(channel=>({label:channel.label,state:channel.readyState,bufferedBytes:channel.bufferedAmount}))},
         inputDiagnostics:JSON.parse(JSON.stringify({rejectedPoll:nativeGuestInput.rejectedPoll(),pending:nativeGuestInput.bridge.pendingState(),
          queue:nativeGuestInput.queue.status(performance.now()),recovery:globalThis.nativeRecoverySupport?.status()??null,failure:assetFailure,
          executionFailure:globalThis.nativeGuestExecutionFailure??null},
          (_key,value)=>typeof value==='bigint'?value.toString():value))};
       })});
     }
     const inventory=JSON.parse(fs.readFileSync(path.resolve(__dirname,'../../../../outputs/validation/browser-usa-scene-inventory.json'),'utf8'));
     const outputDirectory=path.resolve(process.env.PARTYBOARD_NATIVE_ASSET_REPORT+'.full-match');
     try {
      mixedFullMatchEvidence=focusedPlay?await require('./drive-focused-freeplay.cjs').driveFocusedFreePlay({peers:participants,inventory,outputDirectory}):await driveMixedFullMatch({peers:participants,inventory,outputDirectory,board:Number(process.env.PARTYBOARD_NATIVE_FULL_MATCH_BOARD??0),maxTurn:fullMatchTurns,onVerifiedTurn:async turn=>{if(recoveryTurns.includes(turn)){await recoverAtTurn();stage="mixed-full-match";fs.writeFileSync(path.join(outputDirectory,"mixed-recovery-turn-"+turn+".json"),JSON.stringify({turn,completedRecoveryRounds},null,2)+"\n");}}});
     } catch (error) {
      const browserStates=await Promise.allSettled(participants.slice(2).map(peer=>peer.observe()));
      fs.writeFileSync(path.join(outputDirectory,'mixed-full-match-browser-failure-state.json'),
       JSON.stringify({error:String(error),errorDetails:errorEvidence(error),browserStates},null,2)+'\n');
      throw error;
     }
     for(let index=0;index<peers.length;index++)await peers[index].page.locator('canvas').screenshot({path:path.join(outputDirectory,(focusedPlay?'focused-results-browser-':'mixed-rankings-browser-')+index+'.png')});
     if(captureNativeRankings){
      const {requestNativeFrame}=require('./native-frame-capture.cjs');
      const captures=await Promise.all(['host','guest'].map(owner=>requestNativeFrame(path.join(captureDirectory,'native-rankings-'+owner+'.request'))));
      fs.writeFileSync(path.join(outputDirectory,'native-rankings-captures.json'),JSON.stringify({scope:'actual-native-resolved-game-images',captures,renderedPlacementsVerified:false},null,2)+'\n');
     }
     if(process.env.PARTYBOARD_NATIVE_RANKINGS_CAPTURE_WAIT==='1'){
      const releasePath=path.join(outputDirectory,'native-rankings-capture-release');
      const deadline=Date.now()+10*60*1000;
      console.log(JSON.stringify({stage:'native-rankings-capture-wait',releasePath}));
      while(!fs.existsSync(releasePath)){
       assert(Date.now()<deadline,'Native rankings visual capture release timed out');
       await new Promise(resolve=>setTimeout(resolve,250));
      }
     }
     // Let Join consume its owned stop before Host can close its transport.
     // Otherwise a successful focused test can look like an unacknowledged exit.
     joinChild.stdin.write('stop\n');
     const joined=await waitJoin('join-gameplay');
     native.stdin.write('stop\n');
     const gameplay=await wait('gameplay');
     const hostCompletion=await wait('full-match'),joinCompletion=await waitJoin('join-full-match');
     assert.equal(hostCompletion.complete,!focusedPlay);assert.equal(joinCompletion.complete,!focusedPlay);
     if(focusedPlay){assert.equal(hostCompletion.focusedMinigame,true);assert.equal(joinCompletion.focusedMinigame,true);}
     assert.equal(joined.execution,0);
     const common=matchingRecords([gameplay.observations,joined.observations]);assert(common.length>=30);
     gameplayEvidence={host:gameplay.observations,guests:[{engine:'native',observations:joined.observations}],matchingFinalNativePolls:common.length};
     await waitJoin('join-game-stopped');assert.equal((await joinExited).code,0);
     for(const {page} of peers)await page.evaluate(async()=>{nativeGuestInput.cancel();await nativeGuestExecution;nativeGuestInput.close();});
     native.stdin.write('finish\n');assert.equal((await wait('game-stopped')).detached,true);assert.equal((await exited).code,0);
    }else{
    const gameplay=await wait('gameplay');
    assert.equal(gameplay.accepted,60);assert.equal(gameplay.observations.length,60);assert.equal(gameplay.phase,2);
    gameplayEvidence={host:gameplay.observations,guests:[]};
    if(externalJoin){
     const joined=await waitJoin('join-gameplay');assert.equal(joined.execution,0);assert.equal(joined.accepted,60);
     assert.equal(joined.frame,60);assert.equal(joined.phase,2);assert.deepEqual(joined.observations,gameplay.observations);
     gameplayEvidence.guests.push({peer:joinBootstrap.peer,engine:'native',observations:joined.observations});
     const detached=await waitJoin('join-game-stopped');assert.equal(detached.detached,true);assert.equal(detached.frame,60);
     assert.equal((await joinExited).code,0);
    }
    for(const {page} of peers){
     await page.waitForFunction(()=>nativeGuestInput.observations().length>=60,{},{timeout:30000});
     const actual=await page.evaluate(()=>nativeGuestInput.observations().slice(0,60));
     assert.deepEqual(actual,gameplay.observations.map(sample=>({...sample,frame:String(sample.frame)})));
     gameplayEvidence.guests.push({peer:peers.find(peer=>peer.page===page).bootstrap.peer,engine:'browser',observations:actual});
     await page.evaluate(async()=>{nativeGuestInput.cancel();await nativeGuestExecution;nativeGuestInput.close();});
    }
    native.stdin.write('finish\n');
    const stopped=await wait('game-stopped');
    assert.equal(stopped.detached,true);assert.equal(stopped.phase,2);assert.equal(stopped.frame,60);
    assert.equal((await exited).code,0);
    }
   }
   for(const {page} of peers)await page.evaluate(async()=>{await nativeGuestRuntime.dispose();nativeGuestRuntime=undefined;});
  }
  if(!frontend)for(const {page} of peers)await page.evaluate(()=>{assetStore.close();assetChannel.close();assetConnection.close();});
  if(!startFrontend)native.stdin.write(initial?'stop\n':'{"type":"stop"}\n');assert.equal((await exited).code,0);
  if(externalJoin&&!startFrontend){joinChild.stdin.write('stop\n');assert.equal((await joinExited).code,0);}
  const observed=observations[0];
  // Keep diagnostic evidence on success too; ring retention is still incomplete.
  if(process.env.PARTYBOARD_NATIVE_ASSET_REPORT)fs.writeFileSync(process.env.PARTYBOARD_NATIVE_ASSET_REPORT+'.rng-tail.json',JSON.stringify({captureComplete:false,peers:randomTraceTail.snapshot(),targetTrace:{captureComplete:false,peers:randomTraceTail.targetSnapshot()}},null,2)+'\n');
  if(initial){if(frontend)assert.equal(completed.lobbySnapshot.players.length,count+1);else assert.equal(completed.guests,count);assert.deepEqual(completed.sentBytes,externalJoin?[joinBootstrap.manifest.size,...observations.map(o=>o.size)]:observations.map(o=>o.size));}
  fs.writeFileSync(process.env.PARTYBOARD_NATIVE_ASSET_REPORT,JSON.stringify({mixedFullMatchEvidence,browserAudioPreparation,scope:fullMatch?(focusedPlay?'Focused normal-menu m443 Free Play coin-award check; no completed Party match claimed':'Mixed native/browser configured match; rendered native placements still require separate verification'):externalJoin?(startFrontend?'Actual native host, external native Join and two Edge guests; full asset transfer, engine preparation, synchronized 60-poll gameplay'+(recoverFrontend?(nativeSelfRecovery?' and native guest authenticated self recovery':' and browser guest authenticated recovery'):''):'Actual native host, external native Join and two Edge guests; asset transfer and stopped engine preparation'):'Actual native RTC host to independent Edge contexts, actual transfer.mjs and IndexedDB',nativeMenuJoinVerified:menuJoin&&startFrontend,externalNativeJoinPreparationVerified:externalJoin,externalNativeJoinGameplayVerified:externalJoin&&startFrontend,nativeHealthyGuestRecoveryVerified:externalJoin&&recoverFrontend&&!nativeSelfRecovery,nativeGuestSelfRecoveryVerified:nativeSelfRecovery,nativeInputTiming:{host:events.findLast(event=>event.type==='gameplay')?.inputTiming,guest:joinEvents.findLast(event=>event.type==='join-gameplay')?.inputTiming},nativeGuestPacketDiagnostics:externalJoin?joinEvents.findLast(event=>event.type==='join-gameplay')?.inputDiagnostics:null,nativeGuestRecoveryEvents:externalJoin&&recoverFrontend?joinEvents.filter(event=>event.type==='join-recovery'):[],independentBrowserGuestCount:peers.length,actualNativeFrontend:frontend,connectedBrowserEnginePreparationVerified:prepareFrontend,nativeAcceptedPreparedReceipts,preparedReceipts,independentGuestCount:count,realAssetLobbyVerified:lobbyMode,lobbySnapshot:lobbyMode?completed.lobbySnapshot:null,initialInvitationOwnerVerified:initial,assetId:observed.manifest.id,size:observed.size,persistedChunks:observed.manifest.count,indexedDBReopenVerified:true,nativeExitCode:0,transferVerified:true,productionCredentialDeliveryVerified:frontend,nativeGameIntegrationVerified:startFrontend,nativeAuthenticatedRecoveryVerified:recoverFrontend,verifiedRecoveryRounds:recoveryRounds,matchingCommittedGameplayPolls:fullMatch?gameplayEvidence.matchingFinalNativePolls:startFrontend?60:0,nativeLobbyMusicEvidence,gameplayEvidence,playableNetplayVerified:false},null,2)+'\n');console.log('PASS actual native RTC asset to '+peers.length+' Edge persisted stores'+(externalJoin?' and one independent native Join guest':'')+': '+observed.size+' bytes each');
 }catch(e){
  let browserTransferStates=[];
  try{browserTransferStates=await Promise.all(peers.map(async({page,bootstrap})=>({peer:bootstrap.peer,expectedBytes:bootstrap.manifest.size,state:await page.evaluate(()=>({progress:globalThis.assetProgress??null,complete:Boolean(globalThis.assetComplete),failure:globalThis.assetFailure??null,connection:globalThis.assetConnection?.connectionState??null,ice:globalThis.assetConnection?.iceConnectionState??null}))})));}
  catch(progressError){browserTransferStates=[{error:String(progressError)}];}
  if(process.env.PARTYBOARD_NATIVE_ASSET_REPORT)fs.writeFileSync(process.env.PARTYBOARD_NATIVE_ASSET_REPORT+'.rng-tail.json',JSON.stringify({captureComplete:false,peers:randomTraceTail.snapshot(),targetTrace:{captureComplete:false,peers:randomTraceTail.targetSnapshot()}},null,2)+'\n');failed=true;if(stderr)console.error(stderr);if(joinStderr)console.error(joinStderr);if(process.env.PARTYBOARD_NATIVE_ASSET_REPORT)fs.writeFileSync(process.env.PARTYBOARD_NATIVE_ASSET_REPORT+'.failure.json',JSON.stringify({stage,error:String(e),errorDetails:errorEvidence(e),diagnostics,nativeEvents:events,nativeExitCode:native.exitCode,nativeJoinExitCode:joinChild?.exitCode,nativeJoinEvents:joinEvents,nativeJoinStderr:joinStderr,browserTransferStates},null,2)+'\n');throw e;
 }finally{stage='cleanup';for(const w of pending)clearTimeout(w.timer);for(const w of joinPending)clearTimeout(w.timer);if(joinChild&&joinChild.exitCode===null)joinChild.kill();if(native.exitCode===null)native.kill();server?.kill();if(failed)await browserServer?.kill();else{await browser?.close();await browserServer?.close();}if(artifactServer){artifactServer.closeAllConnections();await new Promise(resolve=>artifactServer.close(resolve));}}
})().catch(e=>{console.error(e);process.exitCode=1;});










