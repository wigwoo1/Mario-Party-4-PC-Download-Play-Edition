import {receiveRecoveryCredential,signRecoveryDescription,verifyRecoveryDescription} from './recovery-identity.mjs';
const labels=['partyboard-input-v1','partyboard-recovery-v1'];
// Establishes authenticated transport only. Caller still verifies paused game
// state and owns synchronized recovery authorization before resuming input.
export function createReplacementTransport({credential,role,createConnection,usedNonces,signal,deadline,now=()=>performance.now()}){
 credential=receiveRecoveryCredential(credential,credential);
 if(!['host','guest'].includes(role)||typeof createConnection!=='function'||!(usedNonces instanceof Set)||
    !Number.isFinite(deadline)||typeof now!=='function')throw Error('Owned replacement identity, replay set and deadline required.');
 let connection,stage='New',nonce,closed=false,handed=false,failure,timer;
 const channels=new Map(),listeners=[],waiters=new Set();
 const remove=()=>{clearTimeout(timer);signal?.removeEventListener('abort',abort);for(const detach of listeners)detach();listeners.length=0;};
 const close=(cause=Error('Replacement transport closed.'))=>{
  if(closed||handed)return;closed=true;failure=cause;remove();
  for(const channel of channels.values())channel.close();connection?.close();
  for(const waiter of waiters)waiter.reject(cause);waiters.clear();
 };
 const check=()=>{if(closed)throw failure;if(handed)throw Error('Replacement transport handed off.');signal?.throwIfAborted();if(now()>=deadline){close(Error('Replacement deadline expired.'));throw failure;}};
 const abort=()=>close(signal.reason instanceof Error?signal.reason:Error('Replacement aborted.'));
 signal?.addEventListener('abort',abort,{once:true});
 const tick=()=>{if(closed||handed)return;if(now()>=deadline){close(Error('Replacement deadline expired.'));return;}timer=setTimeout(tick,Math.min(100,Math.max(1,deadline-now())));};
 timer=setTimeout(tick,1);
 if(signal?.aborted)abort();
 const listen=(target,type,fn)=>{target.addEventListener(type,fn);listeners.push(()=>target.removeEventListener(type,fn));};
 const wake=()=>{for(const waiter of [...waiters]){if(waiter.ready()){waiters.delete(waiter);waiter.resolve();}}};
 const wait=ready=>{check();if(ready())return Promise.resolve();return new Promise((resolve,reject)=>waiters.add({ready,resolve,reject}));};
 const attach=channel=>{
  if(closed||handed){channel.close();return;}
  if(!labels.includes(channel.label)||channels.has(channel.label)||channel.ordered!==true||channel.maxRetransmits!=null||channel.maxPacketLifeTime!=null){channel.close();close(Error('Invalid replacement channel.'));return;}
  channels.set(channel.label,channel);channel.binaryType='arraybuffer';
  listen(channel,'open',wake);listen(channel,'close',()=>close(Error('Replacement channel closed before handoff.')));listen(channel,'error',()=>close(Error('Replacement channel failed.')));wake();
 };
 const allocate=()=>{
  check();connection=createConnection();
  listen(connection,'connectionstatechange',()=>{if(connection.connectionState==='failed')close(Error('Replacement connection failed.'));});
  listen(connection,'icegatheringstatechange',wake);
  if(role==='guest')listen(connection,'datachannel',event=>attach(event.channel));
  else for(const label of labels)attach(connection.createDataChannel(label,{ordered:true}));
  check();
 };
 const gathered=async()=>{await wait(()=>connection.iceGatheringState==='complete');check();};
 const owned=async action=>{try{return await action();}catch(cause){close(cause);throw cause;}};
 return {
  createOffer:()=>owned(async()=>{
   check();if(role!=='host'||stage!=='New')throw Error('New host replacement required.');stage='Offering';allocate();
   const offer=await connection.createOffer();check();await connection.setLocalDescription(offer);await gathered();
   const packet=await signRecoveryDescription(credential,connection.localDescription);check();nonce=packet.nonce;stage='WaitingAnswer';return packet;
  }),
  acceptOffer:packet=>owned(async()=>{
   check();if(role!=='guest'||stage!=='New')throw Error('New guest replacement required.');stage='VerifyingOffer';
   const verified=await verifyRecoveryDescription(packet,credential,{type:'offer',usedNonces});check();
   if(!verified)throw Error('Replacement offer authentication failed.');nonce=verified.nonce;allocate();
   await connection.setRemoteDescription(verified.description);check();
   const localAnswer=await connection.createAnswer();check();await connection.setLocalDescription(localAnswer);await gathered();
   const answer=await signRecoveryDescription(credential,connection.localDescription,{nonce});check();stage='WaitingChannels';wake();return answer;
  }),
  acceptAnswer:packet=>owned(async()=>{
   check();if(role!=='host'||stage!=='WaitingAnswer')throw Error('Pending replacement answer required.');stage='VerifyingAnswer';
   const verified=await verifyRecoveryDescription(packet,credential,{type:'answer',usedNonces,expectedNonce:nonce});check();
   if(!verified)throw Error('Replacement answer authentication failed.');
   await connection.setRemoteDescription(verified.description);check();stage='WaitingChannels';wake();
  }),
  ready:()=>owned(async()=>{
   await wait(()=>stage==='WaitingChannels'&&labels.every(label=>channels.get(label)?.readyState==='open'));check();
   return {peer:credential.peer,session:credential.session};
  }),
  handoff(){check();if(stage!=='WaitingChannels'||!labels.every(label=>channels.get(label)?.readyState==='open'))throw Error('Authenticated open replacement channels required.');
   handed=true;remove();return Object.freeze({connection,peer:credential.peer,session:credential.session,nonce,channels:new Map(channels)});
  },close,
 };
}
