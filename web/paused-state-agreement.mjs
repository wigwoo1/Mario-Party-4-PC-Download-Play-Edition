import {createRecoveryCommit} from './recovery-commit.mjs';
const receipts=new WeakMap();
export function pausedStateReport(identity,{attempt,state,input},{peer,nonce}){
 if(!/^[a-f0-9]{32}$/.test(nonce)||!identity.players.some(player=>player.peer===peer)||
    input.frame!==attempt.frame||!['checkpoint','frand','rand8'].every(key=>state[key]===attempt[key]))
  throw Error('Current suspended input and assigned report identity required.');
 const {peer:owner,epoch,revision,...common}=createRecoveryCommit(identity,attempt,state,input);
 // Local recovery policy revisions can differ after upstream loss. Compare
 // actual game/input state; retain each local attempt as a separate stale gate.
 return Object.freeze({...common,type:'paused-state-report',reporter:peer,nonce});
}
const same=(actual,expected)=>actual&&Object.keys(actual).length===Object.keys(expected).length&&
 Object.keys(expected).every(key=>key==='players'?JSON.stringify(actual[key])===JSON.stringify(expected[key]):actual[key]===expected[key]);
export function agreePausedState({channel,identity,peer,remotePeer,nonce,capture,current,signal,deadline,now=()=>performance.now()}){
 if(channel?.label!=='partyboard-recovery-v1'||channel.readyState!=='open'||channel.ordered!==true||
    channel.maxRetransmits!=null||channel.maxPacketLifeTime!=null||typeof capture!=='function'||typeof current!=='function'||
    peer===remotePeer||!identity.players.some(player=>player.peer===remotePeer)||!Number.isFinite(deadline))
  throw Error('Authenticated reliable channel and current paused state required.');
 const local=pausedStateReport(identity,capture(),{peer,nonce}),remote={...local,reporter:remotePeer};
 return new Promise((resolve,reject)=>{
  let finished=false,timer,receivedReport=false,receivedAck=false;
  const cleanup=()=>{clearTimeout(timer);channel.removeEventListener('message',message);channel.removeEventListener('close',lost);channel.removeEventListener('error',lost);signal?.removeEventListener('abort',abort);};
  const fail=cause=>{if(finished)return;finished=true;cleanup();reject(cause);};
  const valid=()=>{if(signal?.aborted)throw signal.reason||Error('Recovery aborted.');if(now()>=deadline)throw Error('Paused state agreement deadline expired.');
   if(current()!==true||channel.readyState!=='open'||!same(pausedStateReport(identity,capture(),{peer,nonce}),local))throw Error('Paused recovery state changed.');};
  const complete=()=>{if(!receivedReport||!receivedAck)return;valid();finished=true;cleanup();
   const receipt=Object.freeze({peer,remotePeer,nonce});receipts.set(receipt,{channel,report:local});resolve(receipt);};
  const message=event=>{if(finished)return;try{
   valid();if(typeof event.data!=='string'||event.data.length>4096)throw Error('Invalid paused state packet.');
   const packet=JSON.parse(event.data);
   // Reliable ordered delivery can carry the previous phase's final retry after
   // this endpoint moves on. It never supplies state-agreement permission.
   if(['paused-poll-report','paused-poll-ack'].includes(packet.type)&&packet.nonce===nonce&&packet.peer===remotePeer)return;
   if(packet.type==='paused-state-report'){
    if(!same(packet,remote))throw Error('Peer paused state differs.');receivedReport=true;
    channel.send(JSON.stringify({type:'paused-state-ack',version:1,nonce,peer,accepted:packet}));
   }else if(packet.type==='paused-state-ack'){
    if(packet.version!==1||packet.nonce!==nonce||packet.peer!==remotePeer||!same(packet.accepted,local))throw Error('Invalid paused state acknowledgment.');receivedAck=true;
   }else throw Error('Unexpected paused state packet.');
   complete();
  }catch(cause){fail(cause);}};
  const lost=()=>fail(Error('Recovery state channel closed.'));
  const abort=()=>fail(signal.reason||Error('Recovery state agreement aborted.'));
  const tick=()=>{if(finished)return;try{valid();if(!receivedAck)channel.send(JSON.stringify(local));timer=setTimeout(tick,100);}catch(cause){fail(cause);}};
  channel.addEventListener('message',message);channel.addEventListener('close',lost);channel.addEventListener('error',lost);signal?.addEventListener('abort',abort,{once:true});
  try{valid();channel.send(JSON.stringify(local));if(!finished)timer=setTimeout(tick,100);}catch(cause){fail(cause);}
 });
}
export function consumePausedStateAgreement(receipt,{channel,identity,captured,peer,remotePeer,nonce}){
 try{
 const owned=receipts.get(receipt);
 if(!owned||owned.channel!==channel||channel.readyState!=='open'||receipt.peer!==peer||receipt.remotePeer!==remotePeer||receipt.nonce!==nonce||
    !same(owned.report,pausedStateReport(identity,captured,{peer,nonce})))return false;
 receipts.delete(receipt);return true;
 }catch{return false;}
}
