const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const {spawn}=require('node:child_process');const assert=require('node:assert/strict');
(async()=>{
 const server=spawn(process.execPath,[__dirname+'/serve.cjs'],{env:{...process.env,PARTYBOARD_TEST_PORT:'8802'},stdio:['ignore','pipe','inherit']});let browser;
 try{
  await new Promise((resolve,reject)=>{server.stdout.once('data',resolve);server.once('error',reject);server.once('exit',code=>reject(Error('Server exited '+code)));});
  browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});const pages=[],errors=[];
  for(let i=0;i<4;i++){const context=await browser.newContext(),page=await context.newPage();page.on('pageerror',error=>errors.push(error.message));await page.goto('http://127.0.0.1:8802');pages.push(page);}
  const host=pages[0];
  for(const [index,page] of pages.entries())await page.evaluate(index=>{
   globalThis.index=index;globalThis.inputChannels=new Map();globalThis.controlChannels=new Map();globalThis.connections=[];
   globalThis.gather=connection=>new Promise((resolve,reject)=>{
    if(connection.iceGatheringState==='complete'){resolve();return;}
    const timer=setTimeout(()=>{connection.removeEventListener('icegatheringstatechange',changed);reject(Error('ICE gathering timeout'));},5000);
    const changed=()=>{if(connection.iceGatheringState==='complete'){clearTimeout(timer);connection.removeEventListener('icegatheringstatechange',changed);resolve();}};
    connection.addEventListener('icegatheringstatechange',changed);
   });
  },index);
  for(let i=1;i<4;i++){
   const offer=await host.evaluate(async i=>{
    const connection=new RTCPeerConnection({iceServers:[]});connections.push(connection);
    inputChannels.set('g'+i,connection.createDataChannel('partyboard-input-v1',{ordered:true}));
    controlChannels.set('g'+i,connection.createDataChannel('partyboard-recovery-v1',{ordered:true}));
    await connection.setLocalDescription(await connection.createOffer());await gather(connection);return connection.localDescription.toJSON();
   },i);
   const answer=await pages[i].evaluate(async offer=>{
    const connection=new RTCPeerConnection({iceServers:[]});connections.push(connection);
    connection.ondatachannel=event=>{(event.channel.label==='partyboard-input-v1'?inputChannels:controlChannels).set('host',event.channel);};
    await connection.setRemoteDescription(offer);await connection.setLocalDescription(await connection.createAnswer());await gather(connection);return connection.localDescription.toJSON();
   },offer);
   await host.evaluate(async({i,answer})=>connections[i-1].setRemoteDescription(answer),{i,answer});
  }
  try {
   for(const page of pages)await page.waitForFunction(()=>inputChannels.size===(index===0?3:1)&&controlChannels.size===(index===0?3:1)&&[...inputChannels.values(),...controlChannels.values()].every(channel=>channel.readyState==='open'));
  } catch(error) {
   for(const [peer,page] of pages.entries()) {
    try { console.error('RTC readiness diagnostic',JSON.stringify(await page.evaluate(async()=>({
     peer:index,connections:connections.map(connection=>({connection:connection.connectionState,ice:connection.iceConnectionState,gathering:connection.iceGatheringState,signaling:connection.signalingState,
      localCandidates:(connection.localDescription?.sdp.match(/^a=candidate:/gm)||[]).length,
      remoteCandidates:(connection.remoteDescription?.sdp.match(/^a=candidate:/gm)||[]).length})),
     statistics:await Promise.all(connections.map(async connection=>{
      const stats=await connection.getStats(),pairs=[],candidates=[];
      stats.forEach(stat=>{
       if(stat.type==='candidate-pair')pairs.push({state:stat.state,nominated:stat.nominated,requestsSent:stat.requestsSent,responsesReceived:stat.responsesReceived});
       if(stat.type==='local-candidate'||stat.type==='remote-candidate')candidates.push({side:stat.type,protocol:stat.protocol,candidateType:stat.candidateType});
      });return {pairs,candidates};
     })),
     input:[...inputChannels].map(([peer,channel])=>({peer,state:channel.readyState})),
     recovery:[...controlChannels].map(([peer,channel])=>({peer,state:channel.readyState}))
    })))); } catch(diagnosticError) { console.error('RTC diagnostic unavailable',peer,diagnosticError.message); }
   }
   throw error;
  }
  for(const page of pages)await page.evaluate(async()=>{
   const {SessionInput}=await import('/session-input.mjs'),{createSessionInputExchange}=await import('/session-input-exchange.mjs');
   const {RecoveryBarrier,recoveryAcknowledgment}=await import('/recovery-barrier.mjs'),{resumeCommittedInput}=await import('/committed-input-recovery.mjs');
   const {createLobbyInputRouter}=await import('/lobby-input-router.mjs'),{lobbyInputOwnership}=await import('/lobby-input-ownership.mjs');
   const peer=index===0?'host':'g'+index,session=new Uint8Array(16).fill(1),owners=[1,2,3,4];
   const identity={session:'01'.repeat(16),assetId:'a'.repeat(64),buildId:'b'.repeat(64),profile:'unlocked-no-save-v1',seed:0,revision:8,membership:4,players:owners.map((_,slot)=>({peer:slot===0?'host':'g'+slot,slot}))};
   const queue=new SessionInput(session,owners);globalThis.recoveryErrors=[];globalThis.proposal=null;globalThis.committed=false;
   const router=createLobbyInputRouter({plan:lobbyInputOwnership({membership:4,players:identity.players},peer),session,host:index===0,channels:inputChannels,
    receive:(owner,bytes)=>{queue.receive(owner,bytes,performance.now());bridge.notify();},error:(_peer,error)=>recoveryErrors.push(error.message)});
   const packets=[];
   const bridge=createSessionInputExchange(queue,{session,owners,peer:index+1,localPadSlots:owners.map((_,slot)=>slot===index?0:null),checkpoint:()=>42,
    send:bytes=>{packets.push(Array.from(bytes));router.send(bytes);},suspendOnPause:true});
   for(const owner of owners)if(owner!==index+1)queue.disconnected(owner,performance.now());
   const pads=new Uint8Array(44);pads.fill(index+31,0,11);globalThis.poll=bridge.exchange(pads,{frand:0,rand8:5});
   const capture=()=>{const input=queue.status(performance.now());return {input,attempt:{peer:owners.find(owner=>owner!==index+1),epoch:input.epoch,revision:input.revision,...bridge.pendingState()},state:{frame:10,checkpoint:42,frand:0,rand8:5}};};
   const options={identity,peer,host:'host',capture,current:()=>!!bridge.pendingState(),queue,bridge,transportReady:()=>[...inputChannels.values(),...controlChannels.values()].every(channel=>channel.readyState==='open')};
   let barrier;
   for(const [boundPeer,channel] of controlChannels)channel.onmessage=event=>{try{
    const message=JSON.parse(event.data);
    if(index!==0&&message.type==='session-recovery-proposal'){proposal=message;return;}
    if(index===0&&message.type==='session-recovery-ack'){
     if(barrier.acknowledge(boundPeer,message)){
      const commit=barrier.commit();for(const channel of controlChannels.values())channel.send(JSON.stringify(commit));
      resumeCommittedInput({...options,message:commit,acknowledged:barrier.proposal});committed=true;
     }return;
    }
    if(index!==0&&message.type==='session-recovery-commit'){resumeCommittedInput({...options,message,acknowledged:proposal});committed=true;return;}
    throw Error('Unexpected recovery packet');
   }catch(error){recoveryErrors.push(error.message);}};
   globalThis.begin=()=>{barrier=new RecoveryBarrier({...options,nonce:'d'.repeat(32),deadline:performance.now()+queue.status(performance.now()).remainingMs});for(const channel of controlChannels.values())channel.send(JSON.stringify(barrier.proposal));};
   globalThis.ack=()=>controlChannels.get('host').send(JSON.stringify(recoveryAcknowledgment(proposal,identity,capture(),options)));
   globalThis.snapshot=()=>({state:queue.status(performance.now()).state,frame:String(queue.status(performance.now()).frame),pending:!!bridge.pendingState(),packets:packets.length,missing:barrier?.pending});
   globalThis.cleanup=()=>{router.close();for(const connection of connections)connection.close();};
  });
  await host.evaluate(()=>begin());for(const page of pages.slice(1))await page.waitForFunction(()=>proposal!==null);
  await pages[1].evaluate(()=>ack());await pages[2].evaluate(()=>ack());
  await host.waitForFunction(()=>snapshot().missing.length===1);
  for(const page of pages){const value=await page.evaluate(()=>snapshot());assert.equal(value.state,'Paused');assert.equal(value.frame,'0');assert.equal(value.pending,true);assert.equal(value.packets,0);}
  await pages[3].evaluate(()=>ack());
  for(const page of pages){
   await page.waitForFunction(()=>committed&&snapshot().frame==='1');
   const bytes=await page.evaluate(async()=>Array.from(await poll));
   for(let slot=0;slot<4;slot++)assert.deepEqual(bytes.slice(slot*11,slot*11+11),Array(11).fill(slot+31));
   const value=await page.evaluate(()=>snapshot());assert.equal(value.pending,false);assert.equal(value.packets,1);
   assert.deepEqual(await page.evaluate(()=>recoveryErrors),[]);await page.evaluate(()=>cleanup());
  }
  assert.deepEqual(errors,[]);console.log('PASS: four independent browsers, three actual reliable RTC routes, withheld fourth acknowledgment blocks all polls, complete recovery commit releases same four real input queues/polls with identical distinct-controller PAD values. Synthetic engine state; helper dispatch, not actual C gameplay recovery.');
 }finally{await browser?.close();server.kill();}
})().catch(error=>{console.error(error);process.exitCode=1;});
