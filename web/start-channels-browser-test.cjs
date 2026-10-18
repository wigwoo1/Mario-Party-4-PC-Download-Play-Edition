// Synthetic readiness/checkpoint fixture; no game engine or assets executed.
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const http=require('node:http'),fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const files=new Set(['start-barrier.mjs','start-host.mjs','start-channels.mjs','start-client.mjs','start-commit-channel.mjs','start-preparation-channel.mjs','session-recovery.mjs','reconnect-policy.mjs']);
files.add('online-input-gate.mjs');
const server=http.createServer((req,res)=>{
 if(req.method==='GET'&&req.url==='/'){res.end('<!doctype html><title>Start agreement transport test</title>');return;}
 const name=req.url.slice(1);if(req.method!=='GET'||!files.has(name)){res.writeHead(404);res.end();return;}
 res.setHeader('Content-Type','text/javascript');res.end(fs.readFileSync(path.join(__dirname,name)));
});
(async()=>{
 let browser;
 try {
  await new Promise((resolve,reject)=>{server.once('error',reject);server.listen(0,'127.0.0.1',resolve);});
  browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});const pages=[],errors=[];
  for(let i=0;i<4;i++) {
   const page=await(await browser.newContext()).newPage();pages.push(page);
   page.on('pageerror',e=>errors.push(e.message));await page.goto(`http://127.0.0.1:${server.address().port}/`);
   await page.evaluate(()=>{
    globalThis.gather=async pc=>{if(pc.iceGatheringState!=='complete')await new Promise(resolve=>{
      const changed=()=>{if(pc.iceGatheringState==='complete'){pc.removeEventListener('icegatheringstatechange',changed);resolve();}};
      pc.addEventListener('icegatheringstatechange',changed);changed();
    });return pc.localDescription.toJSON();};
   });
  }
  const [host,...guests]=pages;
  await host.evaluate(async()=>{
   const {StartBarrier}=await import('/start-barrier.mjs'),{StartChannels}=await import('/start-channels.mjs');
   const snapshot={revision:8,membership:4,total:100,assetId:'a'.repeat(64),players:
    ['host','second','third','fourth'].map((peer,slot)=>({peer,slot,bytes:100,verified:true,shaders:true,ready:true}))};
   globalThis.barrier=new StartBarrier(snapshot,{session:new Uint8Array(16).fill(1),seed:1,buildId:'b'.repeat(64),checkpoint:42});
   globalThis.failures=[];globalThis.transport=new StartChannels(barrier,{snapshot:()=>snapshot,error:(peer,e)=>failures.push([peer,e.message])});
   transport.acknowledgeLocal('host',{...barrier.proposal,type:'session-start-ack'});
   globalThis.pcs=[];globalThis.startChannels=[];
   globalThis.hostRuntime={assetId:snapshot.assetId,sessionSeed:1,checkpoint:42,
    engine:{phase:'Idle',preparedCheckpoint:()=>42,resume(){throw Error('Host fixture must remain idle');}}};
  });
  for(let index=0;index<guests.length;index++) {
   const guest=guests[index],peer=['second','third','fourth'][index];
   await guest.evaluate(async({hold,peer})=>{
    const {bindClientStartPreparation}=await import('/start-preparation-channel.mjs');
    const snapshot={revision:8,membership:4,total:100,assetId:'a'.repeat(64),players:
      ['host','second','third','fourth'].map((peer,slot)=>({peer,slot,bytes:100,verified:true,shaders:true,ready:true}))};
    globalThis.prepared=0;globalThis.disposed=0;globalThis.resumed=0;
    globalThis.pc=new RTCPeerConnection({iceServers:[]});
    pc.ondatachannel=event=>{
      globalThis.channel=event.channel;globalThis.commitErrors=[];
      if(hold){
        const send=channel.send.bind(channel);
        channel.send=data=>{globalThis.heldAcknowledgment=data;};
        globalThis.releaseAcknowledgment=()=>send(heldAcknowledgment);
      }
      globalThis.commitReceiver=bindClientStartPreparation(channel,{peer,buildId:'b'.repeat(64),snapshot:()=>snapshot,
        prepare:async proposal=>{
          await new Promise(resolve=>setTimeout(resolve,5));prepared++;
          let closed=false;return {assetId:snapshot.assetId,checkpoint:42,sessionSeed:proposal.seed,
            engine:{phase:'Idle',preparedCheckpoint:()=>42,requestStop(){globalThis.stopRequests=(globalThis.stopRequests||0)+1;},resume(){this.phase='Running';resumed++;return Promise.resolve();}},
            dispose:async()=>{if(!closed){closed=true;disposed++;}}};
        }},{error:cause=>commitErrors.push(cause.message)});
    };
   },{hold:index===2,peer});
   const offer=await host.evaluate(async({index,peer})=>{
    const pc=pcs[index]=new RTCPeerConnection({iceServers:[]});
    const channel=startChannels[index]=pc.createDataChannel('session-start',{ordered:true});transport.attach(peer,channel);
    await pc.setLocalDescription(await pc.createOffer());return gather(pc);
   },{index,peer});
   const answer=await guest.evaluate(async offer=>{await pc.setRemoteDescription(offer);await pc.setLocalDescription(await pc.createAnswer());return gather(pc);},offer);
   await host.evaluate(({index,answer})=>pcs[index].setRemoteDescription(answer),{index,answer});
   await guest.waitForFunction(()=>prepared===1);
  }
  await host.waitForFunction(()=>barrier.pending.length===1);
  assert.deepEqual(await host.evaluate(()=>({state:barrier.state,pending:barrier.pending})),{state:'Awaiting',pending:['fourth']});
  assert.equal(await host.evaluate(()=>{try{transport.prepareRuntimeCommit('host',hostRuntime);return false;}catch{return true;}}),true);
  for(const guest of guests)assert.equal(await guest.evaluate(()=>resumed),0);
  await guests[2].evaluate(()=>releaseAcknowledgment());
  await host.waitForFunction(()=>barrier.state==='Agreed');
  assert.deepEqual(await host.evaluate(()=>failures),[]);
  assert.equal(await host.evaluate(()=>{
    hostRuntime.assetId='c'.repeat(64);
    try{transport.prepareRuntimeCommit('host',hostRuntime);return false;}
    catch{return true;}
    finally{hostRuntime.assetId=barrier.proposal.assetId;}
  }),true);
  for(const guest of guests)assert.equal(await guest.evaluate(()=>resumed),0);
  await host.evaluate(()=>transport.sendRuntimeCommit('host',hostRuntime));
  for(const guest of guests)await guest.waitForFunction(()=>resumed===1);
  await host.evaluate(()=>transport.sendRuntimeCommit('host',hostRuntime));
  for(const guest of guests) {
    await guest.evaluate(()=>new Promise(resolve=>setTimeout(resolve,30)));
    assert.equal(await guest.evaluate(()=>resumed),1);assert.deepEqual(await guest.evaluate(()=>commitErrors),[]);
  }
  await guests[0].evaluate(()=>channel.close());
  await host.waitForFunction(()=>barrier.state==='Cancelled');
  assert.equal(await host.evaluate(()=>{try{transport.prepareRuntimeCommit('host',hostRuntime);return false;}catch{return true;}}),true);
  await guests[0].waitForFunction(()=>disposed===1);
  assert.equal(await guests[0].evaluate(()=>commitErrors.length),1);
  await guests[0].evaluate(()=>commitReceiver.close());
  assert.equal(await guests[0].evaluate(()=>disposed),1);
  await guests[1].evaluate(async()=>{
    const {createSessionRecovery}=await import('/session-recovery.mjs');
    globalThis.sessionOwner=commitReceiver.handoff();globalThis.recoveryTime=100;globalThis.menuReturns=0;
    globalThis.recovery=createSessionRecovery(sessionOwner,{owners:[1,2,3,4],hostPeer:1,
      now:()=>recoveryTime,returnToMenu:async()=>menuReturns++});
  });
  await guests[1].evaluate(async()=>{await commitReceiver.close();channel.close();});
  assert.equal(await guests[1].evaluate(()=>disposed),0);
  assert.deepEqual(await guests[1].evaluate(()=>commitErrors),[]);
  assert.deepEqual(await guests[1].evaluate(()=>({state:recovery.state,stopRequests})),{state:'Paused',stopRequests:1});
  await guests[1].evaluate(async()=>{recoveryTime=60099;await recovery.tick();});
  assert.equal(await guests[1].evaluate(()=>disposed),0);
  await guests[1].evaluate(async()=>{recoveryTime=60100;await recovery.tick();await recovery.tick();});
  assert.equal(await guests[1].evaluate(()=>disposed),1);
  assert.equal(await guests[1].evaluate(()=>menuReturns),1);
  for(const guest of guests)assert.equal(await guest.evaluate(()=>prepared),1);
  await host.evaluate(()=>{transport.close();pcs.forEach(pc=>pc.close());});
  // Independent connections: conflicting proposal and caller cancellation.
  for(const cancelByCaller of [false,true]) {
  await guests[0].evaluate(async()=>{
    const {bindClientStartPreparation}=await import('/start-preparation-channel.mjs');
    const snapshot={revision:8,membership:4,total:100,assetId:'a'.repeat(64),players:
      ['host','second','third','fourth'].map((peer,slot)=>({peer,slot,bytes:100,verified:true,shaders:true,ready:true}))};
    globalThis.conflictErrors=[];globalThis.conflictPrepared=0;globalThis.conflictDisposed=0;globalThis.conflictRuns=0;
    globalThis.callerAbort=new AbortController();
    globalThis.conflictPc=new RTCPeerConnection({iceServers:[]});
    conflictPc.ondatachannel=event=>{
      globalThis.conflictChannel=event.channel;
      globalThis.conflictOwner=bindClientStartPreparation(conflictChannel,{peer:'second',buildId:'b'.repeat(64),snapshot:()=>snapshot,signal:callerAbort.signal,
        prepare:async(proposal,{signal})=>{
          conflictPrepared++;globalThis.conflictSignal=signal;
          await new Promise(resolve=>globalThis.releaseConflict=resolve);
          return {assetId:snapshot.assetId,checkpoint:42,sessionSeed:proposal.seed,
            engine:{phase:'Idle',preparedCheckpoint:()=>42,resume(){conflictRuns++;}},dispose:async()=>conflictDisposed++};
        }},{error:cause=>conflictErrors.push(cause.message)});
    };
  });
  const conflictOffer=await host.evaluate(async()=>{
    globalThis.conflictPc=new RTCPeerConnection({iceServers:[]});
    globalThis.conflictChannel=conflictPc.createDataChannel('conflicting-start',{ordered:true});
    globalThis.conflictAcks=0;conflictChannel.onmessage=()=>conflictAcks++;
    await conflictPc.setLocalDescription(await conflictPc.createOffer());return gather(conflictPc);
  });
  const conflictAnswer=await guests[0].evaluate(async offer=>{
    await conflictPc.setRemoteDescription(offer);await conflictPc.setLocalDescription(await conflictPc.createAnswer());return gather(conflictPc);
  },conflictOffer);
  await host.evaluate(answer=>conflictPc.setRemoteDescription(answer),conflictAnswer);
  await host.waitForFunction(()=>conflictChannel.readyState==='open');
  await host.evaluate(()=>conflictChannel.send(JSON.stringify(barrier.proposal)));
  await guests[0].waitForFunction(()=>conflictPrepared===1);
  if(cancelByCaller)await guests[0].evaluate(()=>callerAbort.abort());
  else await host.evaluate(()=>conflictChannel.send(JSON.stringify({...barrier.proposal,seed:2})));
  await guests[0].waitForFunction(()=>conflictSignal.aborted);
  await guests[0].evaluate(async()=>{releaseConflict();await conflictOwner.close();conflictPc.close();});
  assert.deepEqual(await guests[0].evaluate(()=>({prepared:conflictPrepared,disposed:conflictDisposed,runs:conflictRuns,errors:conflictErrors})),
    {prepared:1,disposed:1,runs:0,errors:cancelByCaller?[]:['Host changed proposal during preparation.']});
  assert.equal(await host.evaluate(()=>conflictAcks),0);
  await host.evaluate(()=>conflictPc.close());
  }
  assert.deepEqual(errors,[]);
  console.log('PASS: changed host proposal over real WebRTC aborted held client preparation, released late runtime once and sent no acknowledgement or game frames. Synthetic runtime only.');
  console.log('PASS: caller abort cancelled held preparation on a real WebRTC start channel, released late runtime once and sent no acknowledgement or host-error report. Synthetic runtime only.');
  console.log('PASS: four isolated Edge contexts validate preparation/commits and session recovery over real WebRTC; disconnect pauses, retains runtime until 60-second injected-clock deadline, then cleans up and returns to menu once. Synthetic state only; no gameplay or public NAT verification.');
 } finally {await browser?.close();await new Promise(resolve=>server.close(resolve));}
})().catch(error=>{console.error(error);process.exitCode=1;});
