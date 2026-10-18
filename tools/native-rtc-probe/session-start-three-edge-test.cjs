'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {spawn}=require('node:child_process'),{createInterface}=require('node:readline');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
(async()=>{
 const native=spawn(process.env.PARTYBOARD_NATIVE_THREE_PEER,['--start'],{stdio:['pipe','pipe','pipe']});
 const events=[],pending=[];let browser,server,failure,stderr='';
 native.stderr.on('data',data=>stderr+=data);
 const reject=error=>{failure=error;for(const waiter of pending.splice(0)){clearTimeout(waiter.timer);waiter.reject(error);}};
 native.on('error',reject);
 const exited=new Promise(resolve=>native.on('exit',(code,signal)=>{reject(Error('Native exited '+code));resolve({code,signal});}));
 createInterface({input:native.stdout}).on('line',line=>{try{const event=JSON.parse(line);events.push(event);if(event.type==='error')return reject(Error(event.message));for(let i=pending.length-1;i>=0;i--)if(pending[i].test(event)){const waiter=pending.splice(i,1)[0];clearTimeout(waiter.timer);waiter.resolve(event);}}catch(error){reject(error);}});
 const wait=(type,slot,after=0)=>new Promise((resolve,reject)=>{if(failure)return reject(failure);const test=e=>e.type===type&&(slot===undefined||e.slot===slot),found=events.slice(after).find(test);if(found)return resolve(found);const waiter={test,resolve,reject};waiter.timer=setTimeout(()=>{pending.splice(pending.indexOf(waiter),1);reject(Error('Native event timeout '+type));},45000);pending.push(waiter);});
 try{
  server=spawn(process.execPath,[path.resolve(__dirname,'../../web/serve.cjs')],{env:{...process.env,PARTYBOARD_TEST_PORT:'8818'},stdio:['ignore','pipe','pipe']});
  for(let i=0;i<100;i++){try{if((await fetch('http://127.0.0.1:8818/lockstep.mjs')).ok)break;}catch{}if(i===99)throw Error('Server unavailable');await new Promise(resolve=>setTimeout(resolve,50));}
  browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});const pages=[];
  for(let slot=1;slot<4;slot++){
   const credential=(await wait('original-credential',slot)).credential,offer=(await wait('offer',slot)).packet;
   const page=await(await browser.newContext()).newPage();pages.push(page);await page.goto('http://127.0.0.1:8818');
   const answer=await page.evaluate(async({credential,offer,slot})=>{
    const recovery=await import('/recovery-identity.mjs'),start=await import('/start-client.mjs');
    const original=recovery.receiveRecoveryCredential(credential,credential);const verified=await recovery.verifyRecoveryDescription(offer,original,{type:'offer',usedNonces:new Set()});if(!verified)throw Error('Signed offer invalid');
    const snapshot={assetId:'a'.repeat(64),total:50000,revision:5,membership:3,saveRequired:false,players:[{peer:'host',slot:0},{peer:'guest1',slot:1},{peer:'guest2',slot:2},{peer:'guest3',slot:3}].map(p=>({...p,ready:true,verified:true,shaders:true,bytes:50000}))};
    globalThis.startAck=null;globalThis.startCommit=null;globalThis.startError=null;globalThis.resumeCount=0;globalThis.startConnection=new RTCPeerConnection({iceServers:[]});
    startConnection.ondatachannel=event=>{globalThis.startChannel=event.channel;if(startChannel.label!=='partyboard-start-v1')throw Error('Unexpected channel');let chain=Promise.resolve();startChannel.onmessage=event=>{chain=chain.then(async()=>{const message=JSON.parse(event.data);if(message.type==='session-start'){globalThis.preparedStart=await start.prepareStartAcknowledgment(message,{peer:'guest'+slot,buildId:'b'.repeat(64),snapshot:()=>snapshot,prepare:async()=>({checkpoint:123,sessionSeed:7,engine:{phase:'Idle',preparedCheckpoint:()=>123,resume:()=>{resumeCount++;return true;}},dispose(){}})});startAck=preparedStart.acknowledgment;}else if(message.type==='session-start-commit'){start.resumeCommittedClientRuntime(message,preparedStart,{peer:'guest'+slot,buildId:'b'.repeat(64),snapshot:()=>snapshot});startCommit=message;}else throw Error('Unexpected start message');}).catch(e=>startError=String(e));};};
    await startConnection.setRemoteDescription(verified.description);await startConnection.setLocalDescription(await startConnection.createAnswer());
    if(startConnection.iceGatheringState!=='complete')await new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('ICE timeout')),30000);startConnection.addEventListener('icegatheringstatechange',()=>{if(startConnection.iceGatheringState==='complete'){clearTimeout(timer);resolve();}});});
    return recovery.signRecoveryDescription(original,{type:'answer',sdp:startConnection.localDescription.sdp},{nonce:offer.nonce});
   },{credential,offer,slot});native.stdin.write(JSON.stringify({type:'answer',slot,packet:answer})+'\n');await wait('open',slot);
  }
  for(const page of pages){await page.waitForFunction(()=>startAck||startError,{},{timeout:30000});assert.equal(await page.evaluate(()=>startError),null);assert.equal(await page.evaluate(()=>resumeCount),0);}
  for(let index=0;index<2;index++){await pages[index].evaluate(()=>startChannel.send(JSON.stringify(startAck)));assert.equal((await wait('start-ack',index+1)).agreed,false);}
  assert.equal(events.some(e=>e.type==='start-committed'),false);for(const page of pages)assert.equal(await page.evaluate(()=>resumeCount),0);
  await pages[2].evaluate(()=>startChannel.send(JSON.stringify(startAck)));assert.equal((await wait('start-ack',3)).agreed,true);
  const committed=await wait('start-committed');assert.equal(committed.inputFrame,'0');
  for(const page of pages){await page.waitForFunction(()=>startCommit||startError,{},{timeout:10000});assert.equal(await page.evaluate(()=>startError),null);assert.deepEqual(await page.evaluate(()=>startCommit),committed.commit);assert.equal(await page.evaluate(()=>resumeCount),1);assert.equal(await page.evaluate(()=>startConnection.connectionState),'connected');await page.evaluate(()=>{startChannel.close();startConnection.close();});}
  native.stdin.write('{"type":"stop"}\n');assert.equal((await exited).code,0);
  fs.writeFileSync(process.env.PARTYBOARD_NATIVE_START_REPORT,JSON.stringify({scope:'Actual native host and three Edge signed RTC start channels, synthetic prepared runtimes',allPlayerAgreementVerified:true,withheldAcknowledgmentBlocksStart:true,nativeInputFrameRemainsZero:true,browserCommitValidationVerified:true,nativeExitCode:0,nativeGameIntegrationVerified:false,playableNetplayVerified:false},null,2)+'\n');console.log('PASS native host / three Edge RTC start agreement; withheld ack blocks start, matching commit accepted by all three. Synthetic prepared runtime only.');
 }catch(error){if(stderr)console.error(stderr);throw error;}finally{for(const waiter of pending)clearTimeout(waiter.timer);if(native.exitCode===null)native.kill();server?.kill();await browser?.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
