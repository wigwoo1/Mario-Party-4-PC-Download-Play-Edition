'use strict';
const assert=require('node:assert/strict'),path=require('node:path'),fs=require('node:fs');
const {spawn}=require('node:child_process'),{createInterface}=require('node:readline');
const {pathToFileURL}=require('node:url');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
(async()=>{
 const executable=process.env.PARTYBOARD_NATIVE_RTC_PROBE;
 assert.ok(executable&&fs.existsSync(executable),'Compiled native probe required');
 const authenticatedMode=process.env.PARTYBOARD_NATIVE_RTC_AUTHENTICATED==='1';
 const lockstepMode=authenticatedMode||process.env.PARTYBOARD_NATIVE_RTC_LOCKSTEP==='1';
 const native=spawn(executable,authenticatedMode?['--authenticated-lockstep']:lockstepMode?['--lockstep']:[],{stdio:['pipe','pipe','pipe']});
 let authServer,credential;
 const events=[],waiters=[];let failure,stderr='',browser;
 native.stderr.on('data',bytes=>stderr+=bytes.toString());
 const rejectAll=error=>{failure=error;for(const waiter of waiters.splice(0)){clearTimeout(waiter.timer);waiter.reject(error);}};
 native.on('error',rejectAll);
 const exited=new Promise(resolve=>native.on('exit',(code,signal)=>{rejectAll(Error('Native probe exited: '+code+'/'+signal));resolve({code,signal});}));
 createInterface({input:native.stdout}).on('line',line=>{
  try{
   const event=JSON.parse(line);events.push(event);
   if(event.type==='error'){rejectAll(Error(event.message));return;}
   const index=waiters.findIndex(waiter=>waiter.type===event.type);
   if(index>=0){const [waiter]=waiters.splice(index,1);clearTimeout(waiter.timer);waiter.resolve(event);}
  }catch(error){rejectAll(error);}
 });
 const wait=type=>new Promise((resolve,reject)=>{
  if(failure)return reject(failure);
  const existing=events.find(event=>event.type===type);if(existing)return resolve(existing);
  const waiter={type,resolve,reject};waiter.timer=setTimeout(()=>{const index=waiters.indexOf(waiter);if(index>=0)waiters.splice(index,1);reject(Error('Native '+type+' timeout'));},45000);waiters.push(waiter);
 });
 try{
  if(authenticatedMode){
   const {createRecoveryCredential}=await import(pathToFileURL(path.resolve(__dirname,'../../web/recovery-identity.mjs')));
   credential=createRecoveryCredential({session:'1'.repeat(32),assetId:'a'.repeat(64)},'guest1',require('node:crypto').webcrypto);
   native.stdin.write(JSON.stringify({type:'original-credential',credential})+'\n');
   authServer=spawn(process.execPath,[path.resolve(__dirname,'../../web/serve.cjs')],{env:{...process.env,PARTYBOARD_TEST_PORT:'8818'},stdio:['ignore','pipe','pipe']});
   for(let i=0;i<100;i++){try{if((await fetch('http://127.0.0.1:8818/recovery-identity.mjs')).ok)break;}catch{}if(i===99)throw Error('Recovery server unavailable');await new Promise(resolve=>setTimeout(resolve,50));}
  }
  const offer=await wait('offer');
  if(authenticatedMode)assert.match(offer.packet.nonce,/^[a-f0-9]{32}$/,'Native must generate the offer nonce');
  browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});const page=await browser.newPage();
  let offerSDP=offer.sdp;
  if(authenticatedMode){
   await page.goto('http://127.0.0.1:8818');
   offerSDP=await page.evaluate(async({packet,credential})=>{
    globalThis.recoveryCodec=await import('/recovery-identity.mjs');globalThis.recoveryCredential=credential;
    const verified=await recoveryCodec.verifyRecoveryDescription(packet,credential,{type:'offer',usedNonces:new Set()});
    if(!verified)throw Error('Native offer signature rejected');return verified.description.sdp;
   },{packet:offer.packet,credential});
  }
  const answer=await page.evaluate(async sdp=>{
   globalThis.probeReceived=[];globalThis.probeConnection=new RTCPeerConnection({iceServers:[]});
   probeConnection.ondatachannel=event=>{globalThis.probeChannel=event.channel;probeChannel.binaryType='arraybuffer';probeChannel.onmessage=message=>probeReceived.push(Array.from(new Uint8Array(message.data)));};
   await probeConnection.setRemoteDescription({type:'offer',sdp});await probeConnection.setLocalDescription(await probeConnection.createAnswer());
   if(probeConnection.iceGatheringState!=='complete')await new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Browser ICE gathering timeout')),30000);probeConnection.addEventListener('icegatheringstatechange',()=>{if(probeConnection.iceGatheringState==='complete'){clearTimeout(timer);resolve();}});});
   return probeConnection.localDescription.sdp;
  },offerSDP);
  let signedAnswer;
  if(authenticatedMode){
   signedAnswer=await page.evaluate(async({sdp,nonce})=>recoveryCodec.signRecoveryDescription(recoveryCredential,{type:'answer',sdp},{nonce}),{sdp:answer,nonce:offer.packet.nonce});
   native.stdin.write(JSON.stringify({type:'answer',packet:{...signedAnswer,description:{...signedAnswer.description,sdp:answer+'tampered'}}})+'\n');
   await wait('rejected');assert.equal(events.some(event=>event.type==='open'),false);
   native.stdin.write(JSON.stringify({type:'answer',packet:signedAnswer})+'\n');
  }else native.stdin.write(JSON.stringify({type:'answer',sdp:answer})+'\n');
  await wait('open');
  await page.waitForFunction(()=>globalThis.probeChannel?.readyState==='open',{},{timeout:45000});
  assert.deepEqual(await page.evaluate(()=>({label:probeChannel.label,ordered:probeChannel.ordered,retransmits:probeChannel.maxRetransmits,lifetime:probeChannel.maxPacketLifeTime})),{label:'partyboard-input-v1',ordered:true,retransmits:null,lifetime:null});
  const {encodeInput}=await import(pathToFileURL(path.resolve(__dirname,'../../web/input-packet.mjs')));
  const packets=Array.from({length:4},(_,slot)=>Array.from(encodeInput({session:Uint8Array.from({length:16},(_,i)=>i+1),frame:lockstepMode?0n:slot===3?0xffffffffffffffffn:BigInt(slot),checkpoint:lockstepMode?0xdeadbeef:0xffffffff-slot,slot,pad:Uint8Array.from({length:11},(_,i)=>(slot*71+i*23)&255)})));
  if(!lockstepMode)packets.push(Array.from({length:16384},(_,i)=>(i*37)&255));
  if(lockstepMode){
   await page.waitForFunction(()=>probeReceived.length===1,{},{timeout:5000});
   await page.evaluate(bytes=>probeChannel.send(Uint8Array.from(bytes)),packets[1]);
   await wait('waiting');
   assert.equal(events.some(event=>event.type==='committed'),false,'Partial frame must not commit');
   await page.evaluate(values=>{for(const bytes of values)probeChannel.send(Uint8Array.from(bytes));},packets.slice(2));
   const committed=await wait('committed');
   assert.deepEqual(committed,{type:'committed',frame:'0',checkpoint:0xdeadbeef,
    pads:Array.from({length:4},(_,slot)=>Array.from({length:11},(_,i)=>(slot*71+i*23)&255)),nextFrame:'1'});
   assert.deepEqual(events.filter(event=>event.type==='waiting').map(({missing,nextFrame})=>({missing,nextFrame})),[{missing:12,nextFrame:'0'},{missing:8,nextFrame:'0'}]);
  }else await page.evaluate(values=>{for(const bytes of values)probeChannel.send(Uint8Array.from(bytes));},packets);
  await page.waitForFunction(count=>probeReceived.length===count,packets.length,{timeout:45000});
  assert.deepEqual(await page.evaluate(()=>probeReceived),packets);
  if(authenticatedMode){
   native.stdin.write(JSON.stringify({type:'answer',packet:signedAnswer})+'\n');
   const replayDeadline=Date.now()+5000;
   while(events.filter(event=>event.type==='rejected').length<2&&Date.now()<replayDeadline)await new Promise(resolve=>setTimeout(resolve,10));
   assert.equal(events.filter(event=>event.type==='rejected').length,2,'Replayed real answer must be rejected');
   assert.equal(await page.evaluate(()=>probeConnection.connectionState),'connected');
  }
  // Echo delivery follows native decoding on the same callback, but stdout
  // events can arrive after the browser observes the returned bytes.
  const decodedDeadline=Date.now()+5000;
  while(events.filter(event=>event.type==='decoded').length<(lockstepMode?3:4)&&Date.now()<decodedDeadline)
   await new Promise(resolve=>setTimeout(resolve,10));
  assert.deepEqual(events.filter(event=>event.type==='decoded'),Array.from({length:lockstepMode?3:4},(_,index)=>{const slot=index+(lockstepMode?1:0);return ({
   type:'decoded',slot,frame:(lockstepMode?0n:slot===3?0xffffffffffffffffn:BigInt(slot)).toString(),
   checkpoint:lockstepMode?0xdeadbeef:0xffffffff-slot,session:Array.from({length:16},(_,i)=>i+1),
   pad:Array.from({length:11},(_,i)=>(slot*71+i*23)&255)});}));
  await page.evaluate(()=>{probeChannel.close();probeConnection.close();});
  native.stdin.end(JSON.stringify({type:'stop'})+'\n');
  const result=await Promise.race([exited,new Promise((_,reject)=>setTimeout(()=>reject(Error('Native cleanup timeout')),10000))]);assert.equal(result.code,0);
  const evidence={scope:'Actual native libdatachannel to Edge ordered reliable binary roundtrip',packets:packets.map(packet=>packet.length),nativeInputCodecVerified:true,nativeLockstepFrameVerified:lockstepMode,nativeSignedRTCDescriptionsVerified:authenticatedMode,productionLobbyAuthenticationVerified:false,nativeExitCode:0,nativeGameIntegrationVerified:false,playableNetplayVerified:false};
  if(process.env.PARTYBOARD_NATIVE_RTC_REPORT)fs.writeFileSync(process.env.PARTYBOARD_NATIVE_RTC_REPORT,JSON.stringify(evidence,null,2)+'\n');
  console.log('PASS native-to-Edge binary transport; game integration remains unverified.');
 }catch(error){console.error(stderr);throw error;}finally{if(native.exitCode===null)native.kill();authServer?.kill();await browser?.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
