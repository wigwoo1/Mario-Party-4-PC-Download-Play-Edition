'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {spawn}=require('node:child_process'),{createInterface}=require('node:readline');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
(async()=>{
 const ownerMode=!!process.env.PARTYBOARD_NATIVE_RTC_OWNER_TEST;
 const native=spawn(process.env.PARTYBOARD_NATIVE_THREE_PEER,ownerMode?['--rtc-owner']:process.env.PARTYBOARD_NATIVE_POLL_TEST ? ['--poll'] : [],{stdio:['pipe','pipe','pipe']});
 const events=[],pending=[];let browser,server,failure,stderr='';
 native.stderr.on('data',data=>stderr+=data);
 const reject=error=>{failure=error;for(const waiter of pending.splice(0)){clearTimeout(waiter.timer);waiter.reject(error);}};
 native.on('error',reject);
 const exited=new Promise(resolve=>native.on('exit',(code,signal)=>{reject(Error('Native exited '+code));resolve({code,signal});}));
 createInterface({input:native.stdout}).on('line',line=>{try{const event=JSON.parse(line);events.push(event);if(event.type==='error' && !(process.env.PARTYBOARD_NATIVE_CLOSE_TEST && event.message==='Native poll callback stopped'))return reject(Error(event.message));for(let i=pending.length-1;i>=0;i--)if(pending[i].test(event)){const waiter=pending.splice(i,1)[0];clearTimeout(waiter.timer);waiter.resolve(event);}}catch(error){reject(error);}});
 const wait=(type,slot,after=0)=>new Promise((resolve,reject)=>{if(failure)return reject(failure);const test=e=>e.type===type&&(slot===undefined||e.slot===slot),found=events.slice(after).find(test);if(found)return resolve(found);const waiter={test,resolve,reject};waiter.timer=setTimeout(()=>{pending.splice(pending.indexOf(waiter),1);reject(Error('Native event timeout '+type));},type==='error'&&process.env.PARTYBOARD_NATIVE_CLOSE_TEST?70000:45000);pending.push(waiter);});
 try{
  server=spawn(process.execPath,[path.resolve(__dirname,'../../web/serve.cjs')],{env:{...process.env,PARTYBOARD_TEST_PORT:'8818'},stdio:['ignore','pipe','pipe']});
  for(let i=0;i<100;i++){try{if((await fetch('http://127.0.0.1:8818/lockstep.mjs')).ok)break;}catch{}if(i===99)throw Error('Server unavailable');await new Promise(resolve=>setTimeout(resolve,50));}
  browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});const pages=[];
  for(let slot=1;slot<4;slot++){
   const credential=(await wait('original-credential',slot)).credential,offer=(await wait('offer',slot)).packet;
   const page=await(await browser.newContext()).newPage();pages.push(page);await page.goto('http://127.0.0.1:8818');
   const answer=await page.evaluate(async({credential,offer,slot})=>{
    const recovery=await import('/recovery-identity.mjs');const {Lockstep}=await import('/lockstep.mjs');const {decodeInput,encodeInput}=await import('/input-packet.mjs');
    const original=recovery.receiveRecoveryCredential(credential,credential);
    const verified=await recovery.verifyRecoveryDescription(offer,original,{type:'offer',usedNonces:new Set()});if(!verified)throw Error('Invalid native offer');
    const session=new Uint8Array(16).fill(0x11);globalThis.queue=new Lockstep(session,[1,2,3,4]);globalThis.received=[];globalThis.committed=null;
    globalThis.localPacket=encodeInput({session,frame:0n,checkpoint:0xdeadbeef,slot,pad:Uint8Array.from({length:11},(_,i)=>(slot*71+i*23)&255)});
    if(queue.receive(slot+1,localPacket)!=='Accepted')throw Error('Local input rejected');
    globalThis.connection=new RTCPeerConnection({iceServers:[]});
    connection.ondatachannel=event=>{globalThis.channel=event.channel;channel.binaryType='arraybuffer';channel.onmessage=message=>{
     const bytes=new Uint8Array(message.data),packet=decodeInput(bytes);if(!packet||packet.slot===slot)throw Error('Bad relay');
     const result=queue.receive(packet.slot+1,bytes);if(result!=='Accepted')throw Error('Relay input rejected '+result);received.push(Array.from(bytes));
     const frame=queue.advance(0xdeadbeef);if(frame)committed={frame:frame.index.toString(),pads:frame.pads.map(p=>Array.from(p))};
    };};
    await connection.setRemoteDescription(verified.description);await connection.setLocalDescription(await connection.createAnswer());
    if(connection.iceGatheringState!=='complete')await new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('ICE timeout')),30000);connection.addEventListener('icegatheringstatechange',()=>{if(connection.iceGatheringState==='complete'){clearTimeout(timer);resolve();}});});
    return recovery.signRecoveryDescription(original,{type:'answer',sdp:connection.localDescription.sdp},{nonce:offer.nonce});
   },{credential,offer,slot});
   native.stdin.write(JSON.stringify({type:'answer',slot,packet:answer})+'\n');await wait('open',slot);
  }
  for(const page of pages){await page.waitForFunction(()=>received.length===1&&channel?.readyState==='open',{},{timeout:45000});assert.equal(await page.evaluate(()=>committed),null);}
  if(process.env.PARTYBOARD_NATIVE_CLOSE_TEST){
   const began=Date.now();await pages[0].evaluate(()=>channel.close());await wait('paused-disconnect',1);
   for(const page of pages)assert.equal(await page.evaluate(()=>committed),null);
   const stopped=await wait('error');assert.equal(stopped.message,'Native poll callback stopped');
   const elapsed=Date.now()-began;assert.ok(elapsed>=59000&&elapsed<70000,'Original 60-second reconnect deadline');
   assert.equal(events.some(e=>e.type==='committed'),false);native.stdin.write(JSON.stringify({type:'stop'})+'\n');assert.equal((await exited).code,0);
   fs.writeFileSync(process.env.PARTYBOARD_NATIVE_THREE_PEER_REPORT,JSON.stringify({scope:'Actual RTC closure during native retained callback wait; synthetic game capture',pausedDisconnectVerified:true,elapsedMs:elapsed,noFrameCommitted:true,nativeExitCode:0,nativeGameIntegrationVerified:false,playableNetplayVerified:false},null,2)+'\n');console.log('PASS native callback RTC disconnect / original 60-second expiry / no frame commit');return;
  }
  await pages[0].evaluate(async()=>{const {encodeInput}=await import('/input-packet.mjs');channel.send(encodeInput({session:new Uint8Array(16).fill(0x11),frame:0n,checkpoint:0xdeadbeef,slot:2,pad:new Uint8Array(11)}));});
  await wait('rejected',1);for(const page of pages)assert.equal(await page.evaluate(()=>received.length),1,'Unauthorized packet must not relay');
  for(let index=0;index<pages.length;index++){const after=events.length;await pages[index].evaluate(()=>channel.send(localPacket));if(index<2)await wait(ownerMode?'routed':'waiting-pad',ownerMode?index+1:undefined,after);}
  const expected=Array.from({length:4},(_,slot)=>Array.from({length:11},(_,i)=>(slot*71+i*23)&255));
  assert.deepEqual(await wait('committed'),{type:'committed',frame:'0',pads:expected,nextFrame:'1'});
  if(!ownerMode)assert.deepEqual(events.filter(e=>e.type==='waiting-pad'),[{type:'waiting-pad',frame:'0'},{type:'waiting-pad',frame:'0'}]);
  for(const page of pages){await page.waitForFunction(()=>committed!==null,{},{timeout:10000});assert.deepEqual(await page.evaluate(()=>committed),{frame:'0',pads:expected});assert.equal(await page.evaluate(()=>connection.connectionState),'connected');}
  assert.deepEqual(events.filter(e=>e.type==='routed').map(e=>({slot:e.slot,recipients:e.recipients})).sort((a,b)=>a.slot-b.slot),[{slot:1,recipients:12},{slot:2,recipients:10},{slot:3,recipients:6}]);
  native.stdin.write(JSON.stringify({type:'stop'})+'\n');const result=await exited;assert.equal(result.code,0);
  fs.writeFileSync(process.env.PARTYBOARD_NATIVE_THREE_PEER_REPORT,JSON.stringify({scope:'Actual native host to three Edge peers, signed RTC descriptions and four-slot test-input relay',nativePADStatusExchangeVerified:true,waitingPADStatusUnchanged:!ownerMode,nativeRtcInputOwnerVerified:ownerMode,nativeUIButtonsExcluded:true,nativeHostAndThreeBrowserPeersVerified:true,allFourLockstepFramesIdentical:true,unauthorizedSlotRejectedBeforeRelay:true,nativeExitCode:0,productionCredentialDeliveryVerified:false,assetTransferVerified:false,nativeGameIntegrationVerified:false,playableNetplayVerified:false},null,2)+'\n');
  console.log('PASS native host / three Edge peers: signed RTC, authorized relay, identical four-slot frame.');
 }catch(error){if(stderr)console.error(stderr);throw error;}finally{for(const waiter of pending)clearTimeout(waiter.timer);if(native.exitCode===null)native.kill();server?.kill();await browser?.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});


