import {reconcilePausedPoll} from './paused-poll-reconciliation.mjs';
import {agreePausedState,consumePausedStateAgreement} from './paused-state-agreement.mjs';
import {createReplacementTransport} from './replacement-transport.mjs';
import {receiveSessionInputLoss} from './session-input-loss.mjs';
import {recoveryAcknowledgment} from './recovery-barrier.mjs';
import {resumeCommittedInput} from './committed-input-recovery.mjs';
// Connected acceptance harness only. Uses production input/recovery protocols
// and actual runtime capture; it never changes game frame/checkpoint/RNG.
export function nativeGameplayRecoveryTestSupport({runtime,input,router,plan,configuration,credential,control,inputChannel,identity,error}){
 let activeControl=control,activeInput=inputChannel,round,alignment,commits=0,replacementTask;
 const completed=new Set(),usedNonces=new Set();
 const fail=cause=>error(cause);
 const capture=()=>{
  const policy=input.queue.status(performance.now()),pending=input.bridge.pendingState();
  const binding=plan.bindings.find(binding=>policy.missingSlots[binding.slot]);
  if(policy.state!=='Paused'||!pending||!binding||pending.frame!==policy.frame)throw Error('Actual suspended assigned poll required');
  return {input:policy,attempt:{peer:binding.owner,epoch:policy.epoch,revision:policy.revision,...pending},
   state:{frame:runtime.module._PartyBoard_GameFrameCount(),checkpoint:pending.checkpoint,frand:pending.frand,rand8:pending.rand8}};
 };
 const context=()=>({identity:identity(),capture,current:()=>input.queue.status(performance.now()).state==='Paused',
  peer:plan.peer,host:'host',queue:input.queue,bridge:input.bridge,transportReady:()=>activeInput.readyState==='open'&&activeControl.readyState==='open'});
 const align=async(channel,nonce,strict=false)=>{
  const policy=input.queue.status(performance.now()),initial=capture(),deadline=performance.now()+policy.remainingMs;
  const current=()=>{const status=input.queue.status(performance.now());return status.state==='Paused'&&status.epoch===policy.epoch&&status.revision===policy.revision&&
   (status.frame===initial.input.frame||status.frame===initial.input.frame+1n);};
  const options={channel,identity:identity(),peer:plan.peer,remotePeer:'host',nonce,capture,current,bridge:input.bridge,deadline,previousNonces:completed};
  alignment=nonce;
  try{
   await reconcilePausedPoll(options);
   if(strict){const receipt=await agreePausedState(options);
    if(!consumePausedStateAgreement(receipt,{channel,identity:identity(),captured:capture(),peer:plan.peer,remotePeer:'host',nonce}))throw Error('Original current state agreement receipt rejected');}
   completed.add(nonce);
  }finally{alignment=undefined;}
 };
 const attachControl=channel=>{
  channel.addEventListener('message',event=>{
   if(channel!==activeControl)return;
   try{
    if(typeof event.data!=='string'||event.data.length>4096)throw Error('Bounded native recovery packet required');
    const packet=JSON.parse(event.data);
    if(['paused-poll-report','paused-poll-ack','paused-state-report','paused-state-ack'].includes(packet.type)&&
       (packet.nonce===alignment||completed.has(packet.nonce)))return;
    if(packet.type==='session-input-loss'){
     const missing=receiveSessionInputLoss(packet,{session:configuration.session,plan});
     const binding=plan.bindings.find(binding=>binding.peer===missing);input.queue.disconnected(binding.owner,performance.now());input.bridge.notify();return;
    }
    if(packet.type==='session-poll-align'){
     if(round||alignment||packet.version!==1||Object.keys(packet).length!==6||!['session','buildId','assetId'].every(key=>packet[key]===identity()[key]))throw Error('Original assigned route alignment required');
     void align(channel,packet.nonce).catch(fail);return;
    }
    if(packet.type==='session-recovery-proposal'){
     if(round||alignment)throw Error('Prior recovery phase still pending');
     const owned=context();channel.send(JSON.stringify(recoveryAcknowledgment(packet,owned.identity,capture(),owned)));round=structuredClone(packet);return;
    }
    if(packet.type==='session-recovery-commit'){
     if(!round)throw Error('Original acknowledged proposal required');
     resumeCommittedInput({...context(),message:packet,acknowledged:round});round=undefined;commits++;return;
    }
    throw Error('Unexpected native recovery packet');
   }catch(cause){fail(cause);}
  });
 };
 attachControl(control);
 return {
  commits:()=>commits,
  status:()=>{const status=input.queue.status(performance.now());return {state:status.state,frame:status.frame.toString(),remainingMs:status.remainingMs,configurationRetained:configuration.session===identity()?.session};},
  disconnect(){activeInput.close();},
  async acceptOffer(offer){
   const policy=input.queue.status(performance.now());if(policy.state!=='Paused')throw Error('Original suspended game required');
   const owner=createReplacementTransport({credential,role:'guest',usedNonces,deadline:performance.now()+policy.remainingMs,createConnection:()=>new RTCPeerConnection({iceServers:[]})});
   const answer=await owner.acceptOffer(offer);
   replacementTask=(async()=>{
    await owner.ready();const transferred=owner.handoff(),channel=transferred.channels.get('partyboard-recovery-v1');
    await align(channel,transferred.nonce,true);
    const nextInput=transferred.channels.get('partyboard-input-v1');router.replace('host',nextInput);
    activeInput=nextInput;activeControl=channel;attachControl(channel);
    return {session:transferred.session,peer:transferred.peer,paused:input.queue.status(performance.now()).state==='Paused'};
   })();replacementTask.catch(fail);return answer;
  },verified:()=>replacementTask,
 };
}
