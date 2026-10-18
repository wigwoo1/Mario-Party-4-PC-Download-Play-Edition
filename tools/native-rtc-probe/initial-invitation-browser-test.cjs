'use strict';
const assert=require('node:assert/strict'),{spawn}=require('node:child_process'),{createInterface}=require('node:readline');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE);
const fs=require('node:fs'),path=require('node:path');
(async()=>{
 const count=Number(process.env.PARTYBOARD_INVITATION_PEERS||1);assert.ok(count===1||count===3);
 const lobbyMode=process.env.PARTYBOARD_INVITATION_LOBBY==='1';assert.ok(!lobbyMode||count===3);
 const clientSource=fs.readFileSync(path.resolve(__dirname,'../../web/lobby-client.mjs'),'utf8');
 const native=spawn(process.env.PARTYBOARD_NATIVE_CONTROL_TEST,[lobbyMode?'--invitation-lobby-three-peer':count===3?'--invitation-three-peer':'--invitation-peer'],{stdio:['pipe','pipe','pipe']});
 const events=[],waiters=[];let browser,stderr='';native.stderr.on('data',data=>stderr+=data);
 const completion=new Promise((resolve,reject)=>{native.on('error',reject);native.on('exit',code=>code===0?resolve():reject(Error('Native exit '+code+' '+stderr)));});
 completion.catch(()=>{});
 createInterface({input:native.stdout}).on('line',line=>{const event=JSON.parse(line);events.push(event);for(const waiter of [...waiters])if(event.type===waiter.type&&(!waiter.peer||event.peer===waiter.peer)){clearTimeout(waiter.timer);waiters.splice(waiters.indexOf(waiter),1);waiter.resolve(event);}});
 const wait=(type,peer)=>new Promise((resolve,reject)=>{const found=events.find(event=>event.type===type&&(!peer||event.peer===peer));if(found)return resolve(found);const waiter={type,peer,resolve};waiter.timer=setTimeout(()=>reject(Error('Event timeout '+type)),35000);waiters.push(waiter);});
 try{
  browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});const pages=[];
  for(let slot=1;slot<=count;slot++){
  const invitation=await wait('invitation','guest'+slot);const page=await browser.newPage();pages.push(page);
  const reply=await page.evaluate(async({code,peer,lobbyMode,clientSource})=>{
   const invitation=JSON.parse(atob(code));if(invitation.version!==1||invitation.playerId!==peer||invitation.description.type!=='offer')throw Error('Invalid native offer');
   const pc=globalThis.invitationPeer=new RTCPeerConnection({iceServers:[]});globalThis.delivered=[];
   const {LobbyClient}=await import('data:text/javascript;base64,'+btoa(clientSource));
   const client=new LobbyClient('a'.repeat(64),50000);let reported=false;globalThis.finalLobby=null;globalThis.lobbyError=null;
   const labels=['partyboard-preparation-v1','partyboard-start-v1','partyboard-lobby-v1'];
   pc.ondatachannel=event=>{const channel=event.channel;if(!labels.includes(channel.label))throw Error('Unexpected label');channel.onmessage=event=>{
    try{
     if(lobbyMode&&channel.label==='partyboard-lobby-v1'&&event.data!==channel.label){
      const snapshot=client.receive(event.data);if(!snapshot)return;
      if(!reported){reported=true;channel.send(JSON.stringify({type:'lobby-report',version:1,report:{assetId:snapshot.assetId,sequence:1,bytes:50000,verified:true,shaders:true,ready:true,membership:snapshot.membership}}));}
      if(snapshot.canStart)finalLobby=snapshot;return;
     }
     if(event.data!==channel.label)throw Error('Crossed protocol route');delivered.push(channel.label);channel.send('reply:'+channel.label);
    }catch(error){lobbyError=String(error);}
   };};
   await pc.setRemoteDescription(invitation.description);await pc.setLocalDescription(await pc.createAnswer());
   if(pc.iceGatheringState!=='complete')await new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('Gather timeout')),15000);pc.addEventListener('icegatheringstatechange',()=>{if(pc.iceGatheringState==='complete'){clearTimeout(timer);resolve();}});});
   return btoa(JSON.stringify({version:1,playerId:invitation.playerId,description:pc.localDescription}));
  },{code:invitation.code,peer:'guest'+slot,lobbyMode,clientSource});native.stdin.write(reply+'\n');
  }
  if(lobbyMode){
   const result=await wait('lobby-passed');
   for(const page of pages){await page.waitForFunction(()=>finalLobby||lobbyError,{},{timeout:30000});assert.equal(await page.evaluate(()=>lobbyError),null);assert.deepEqual(await page.evaluate(()=>finalLobby),result.snapshot);}
   native.stdin.write('lobby-observed\n');
  }
  native.stdin.end();await wait('passed');await completion;
  for(const page of pages)assert.deepEqual((await page.evaluate(()=>delivered)).sort(),['partyboard-lobby-v1','partyboard-preparation-v1','partyboard-start-v1']);
  console.log('PASS actual native invitation owner / '+count+' independent Edge peers and bidirectional preparation/start/lobby channels; no game execution.');
  if(lobbyMode)console.log('PASS all three actual Edge LobbyClients validated the native four-player ready snapshot; readiness is fixture state.');
 }finally{for(const waiter of waiters)clearTimeout(waiter.timer);await browser?.close();if(native.exitCode===null)native.kill();}
})().catch(error=>{console.error(error);process.exitCode=1;});
