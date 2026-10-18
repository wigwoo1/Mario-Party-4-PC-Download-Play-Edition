const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const {spawn}=require('node:child_process');const assert=require('node:assert/strict');
(async()=>{
 const server=spawn(process.execPath,[__dirname+'/serve.cjs'],{env:{...process.env,PARTYBOARD_TEST_PORT:'8801'},stdio:['ignore','pipe','inherit']});let browser;
 try{
  await new Promise((resolve,reject)=>{server.stdout.once('data',resolve);server.once('error',reject);server.once('exit',code=>reject(Error('Server exited '+code)));});
  browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});const pages=[],errors=[];
  for(let i=0;i<3;i++){const context=await browser.newContext(),page=await context.newPage();page.on('pageerror',error=>errors.push(error.message));await page.goto('http://127.0.0.1:8801');pages.push(page);}
  const [host,guest,rejected]=pages;
  const credential=await host.evaluate(async()=>{const {createRecoveryCredential}=await import('/recovery-identity.mjs');return createRecoveryCredential({session:'1'.repeat(32),assetId:'a'.repeat(64)},'guest1');});
  for(const [i,page] of pages.entries())await page.evaluate(async({credential,role})=>{
   const {createReplacementTransport}=await import('/replacement-transport.mjs');globalThis.allocations=0;
   globalThis.replacement=createReplacementTransport({credential,role,usedNonces:new Set(),deadline:performance.now()+60000,
    createConnection:()=>{allocations++;return new RTCPeerConnection({iceServers:[]});}});
  },{credential,role:i===0?'host':'guest'});
  const offer=await host.evaluate(()=>replacement.createOffer());
  const refusal=await rejected.evaluate(async packet=>{try{await replacement.acceptOffer({...packet,description:{...packet.description,sdp:packet.description.sdp+'\r\na=changed:1'}});return false;}catch{return allocations===0;}},offer);
  assert.equal(refusal,true);
  const answer=await guest.evaluate(packet=>replacement.acceptOffer(packet),offer);
  await host.evaluate(packet=>replacement.acceptAnswer(packet),answer);
  await Promise.all([host,guest].map(page=>page.evaluate(()=>replacement.ready())));
  for(const page of [host,guest])await page.evaluate(()=>{
   globalThis.transferred=replacement.handoff();globalThis.received=[];
   for(const [label,channel] of transferred.channels)channel.onmessage=event=>received.push({label,value:typeof event.data==='string'?event.data:Array.from(new Uint8Array(event.data))});
  });
  await host.evaluate(()=>{transferred.channels.get('partyboard-input-v1').send(new Uint8Array([1,2,3]));transferred.channels.get('partyboard-recovery-v1').send('host-control');});
  await guest.evaluate(()=>{transferred.channels.get('partyboard-input-v1').send(new Uint8Array([4,5]));transferred.channels.get('partyboard-recovery-v1').send('guest-control');});
  for(const page of [host,guest])await page.waitForFunction(()=>received.length===2);
  assert.deepEqual((await host.evaluate(()=>received)).sort((a,b)=>a.label.localeCompare(b.label)),[{label:'partyboard-input-v1',value:[4,5]},{label:'partyboard-recovery-v1',value:'guest-control'}]);
  assert.deepEqual((await guest.evaluate(()=>received)).sort((a,b)=>a.label.localeCompare(b.label)),[{label:'partyboard-input-v1',value:[1,2,3]},{label:'partyboard-recovery-v1',value:'host-control'}]);
  for(const [index,page] of [host,guest].entries())await page.evaluate(async index=>{
   const {agreePausedState}=await import('/paused-state-agreement.mjs');
   const {adoptVerifiedInput}=await import('/verified-input-adoption.mjs'),{createLobbyInputRouter}=await import('/lobby-input-router.mjs'),{lobbyInputOwnership}=await import('/lobby-input-ownership.mjs');
   const peer=index===0?'host':'guest1',remotePeer=index===0?'guest1':'host',revision=index===0?1n:3n;
   const identity={session:'1'.repeat(32),assetId:'a'.repeat(64),buildId:'b'.repeat(64),profile:'unlocked-no-save-v1',seed:0,revision:8,membership:4,players:[{peer:'host',slot:0},{peer:'guest1',slot:1}]};
   const capture=()=>({attempt:{peer:index===0?2:1,epoch:1n,revision,frame:7n,checkpoint:42,frand:0,rand8:5},
    state:{frame:10,checkpoint:42,frand:0,rand8:5},input:{state:'Paused',epoch:1n,revision,frame:7n}});
   const channel=transferred.channels.get('partyboard-recovery-v1'),nonce=transferred.nonce;
   const old=new EventTarget();old.readyState='closed';old.send=()=>{};
   globalThis.routedOwners=[];globalThis.routerErrors=[];
   globalThis.adoptedRouter=createLobbyInputRouter({plan:lobbyInputOwnership({membership:4,players:identity.players},peer),
    session:new Uint8Array(16).fill(17),host:index===0,channels:new Map([[remotePeer,old]]),receive:owner=>routedOwners.push(owner),error:(_peer,error)=>routerErrors.push(error.message)});
   globalThis.agreeSyntheticPausedState=async()=>{
    const receipt=await agreePausedState({channel,identity,peer,remotePeer,nonce,capture,current:()=>true,deadline:performance.now()+5000});
    const options={receipt,identity,peer,remotePeer,nonce,input:transferred.channels.get('partyboard-input-v1'),control:channel,router:adoptedRouter,capture,current:()=>true};
    const adopted=adoptVerifiedInput(options);let reused=false;try{adoptVerifiedInput(options);}catch{reused=true;}
    return [adopted.previous===old,reused];
   };
  },index);
  const agreements=await Promise.all([host,guest].map(page=>page.evaluate(()=>agreeSyntheticPausedState())));
  assert.deepEqual(agreements,[[true,true],[true,true]]);
  for(const [slot,page] of [host,guest].entries())await page.evaluate(async slot=>{
   const {encodeInput}=await import('/input-packet.mjs');adoptedRouter.send(encodeInput({session:new Uint8Array(16).fill(17),frame:7n,checkpoint:42,slot,pad:new Uint8Array(11)}));
  },slot);
  for(const page of [host,guest])await page.waitForFunction(()=>routedOwners.length===1);
  assert.deepEqual(await host.evaluate(()=>routedOwners),[2]);assert.deepEqual(await guest.evaluate(()=>routedOwners),[1]);
  for(const page of [host,guest]){assert.deepEqual(await page.evaluate(()=>routerErrors),[]);await page.evaluate(()=>adoptedRouter.close());}
  for(const [index,page] of [host,guest].entries())await page.evaluate(async index=>{
   const {SessionInput}=await import('/session-input.mjs'),{createSessionInputExchange}=await import('/session-input-exchange.mjs');
   const {RecoveryBarrier,recoveryAcknowledgment}=await import('/recovery-barrier.mjs'),{resumeCommittedInput}=await import('/committed-input-recovery.mjs');
   const {createLobbyInputRouter}=await import('/lobby-input-router.mjs'),{lobbyInputOwnership}=await import('/lobby-input-ownership.mjs');
   const peer=index===0?'host':'guest1',remotePeer=index===0?'guest1':'host',session=new Uint8Array(16).fill(17);
   const identity={session:'1'.repeat(32),assetId:'a'.repeat(64),buildId:'b'.repeat(64),profile:'unlocked-no-save-v1',seed:0,revision:8,membership:4,players:[{peer:'host',slot:0},{peer:'guest1',slot:1}]};
   const control=transferred.channels.get('partyboard-recovery-v1'),queue=new SessionInput(session,[1,2,0,0]);
   const router=createLobbyInputRouter({plan:lobbyInputOwnership({membership:4,players:identity.players},peer),session,host:index===0,
    channels:new Map([[remotePeer,transferred.channels.get('partyboard-input-v1')]]),receive:(owner,bytes)=>{queue.receive(owner,bytes,performance.now());bridge.notify();},error:(_peer,error)=>routerErrors.push(error.message)});
   const packets=[];
   const bridge=createSessionInputExchange(queue,{session,owners:[1,2,0,0],peer:index+1,localPadSlots:index===0?[0,null,null,null]:[null,0,null,null],checkpoint:()=>42,
    send:bytes=>{packets.push(Array.from(bytes));router.send(bytes);},suspendOnPause:true});
   // Both enter their original poll already paused, preventing early completion.
   queue.disconnected(index===0?2:1,performance.now());
   const pads=new Uint8Array(44);pads.fill(index+21,0,11);globalThis.recoveryPoll=bridge.exchange(pads,{frand:0,rand8:5});
   const capture=()=>{const input=queue.status(performance.now());return {input,attempt:{peer:index===0?2:1,epoch:input.epoch,revision:input.revision,...bridge.pendingState()},state:{frame:10,checkpoint:42,frand:0,rand8:5}};};
   const options={identity,peer,host:'host',capture,current:()=>bridge.pendingState()!==null,queue,bridge,transportReady:()=>control.readyState==='open'&&transferred.channels.get('partyboard-input-v1').readyState==='open'};
   globalThis.recoveryErrors=[];globalThis.recoveryProposal=null;globalThis.recoveryCommitted=false;
   let barrier;
   control.onmessage=event=>{try{const message=JSON.parse(event.data);
    if(index===1&&message.type==='session-recovery-proposal'){recoveryProposal=message;return;}
    if(index===0&&message.type==='session-recovery-ack'){
     if(!barrier.acknowledge('guest1',message))throw Error('Incomplete recovery');const commit=barrier.commit();control.send(JSON.stringify(commit));
     resumeCommittedInput({...options,message:commit,acknowledged:barrier.proposal});recoveryCommitted=true;return;
    }
    if(index===1&&message.type==='session-recovery-commit'){resumeCommittedInput({...options,message,acknowledged:recoveryProposal});recoveryCommitted=true;return;}
    throw Error('Unexpected control');
   }catch(error){recoveryErrors.push(error.message);}};
   globalThis.proposeRecovery=()=>{barrier=new RecoveryBarrier({...options,nonce:'f'.repeat(32),deadline:performance.now()+queue.status(performance.now()).remainingMs});control.send(JSON.stringify(barrier.proposal));};
   globalThis.ackRecovery=()=>control.send(JSON.stringify(recoveryAcknowledgment(recoveryProposal,identity,capture(),options)));
   globalThis.recoveryStatus=()=>({state:queue.status(performance.now()).state,frame:String(queue.status(performance.now()).frame),pending:!!bridge.pendingState(),packets});
   globalThis.closeRecoveryRouter=()=>router.close();
  },index);
  await host.evaluate(()=>proposeRecovery());await guest.waitForFunction(()=>recoveryProposal!==null);
  for(const page of [host,guest])assert.equal((await page.evaluate(()=>recoveryStatus())).state,'Paused');
  await guest.evaluate(()=>ackRecovery());
  for(const page of [host,guest]){
   await page.waitForFunction(()=>recoveryCommitted&&recoveryStatus().frame==='1');
   const pads=await page.evaluate(async()=>Array.from(await recoveryPoll));
   assert.deepEqual(pads.slice(0,11),Array(11).fill(21));assert.deepEqual(pads.slice(11,22),Array(11).fill(22));
   const state=await page.evaluate(()=>recoveryStatus());assert.equal(state.pending,false);assert.equal(state.packets.length,1);
   assert.deepEqual(await page.evaluate(()=>recoveryErrors),[]);await page.evaluate(()=>closeRecoveryRouter());
  }
  for(const page of [host,guest])await page.evaluate(()=>{for(const channel of transferred.channels.values())channel.close();transferred.connection.close();});
  assert.deepEqual(errors,[]);console.log('PASS: actual browser signed replacement, state receipt adoption, assigned-owner RTC input routing and all-player recovery commit releasing two real suspended input polls with distinct PAD bytes. Engine state synthetic; no actual C gameplay recovery.');
 }finally{await browser?.close();server.kill();}
})().catch(error=>{console.error(error);process.exitCode=1;});
