import {pausedStateReport} from './paused-state-agreement.mjs';
const equal=(a,b)=>a===b||(a!==null&&b!==null&&typeof a==='object'&&typeof b==='object'&&
 Object.keys(a).length===Object.keys(b).length&&Object.keys(a).every(key=>Object.hasOwn(b,key)&&equal(a[key],b[key])));
function proposal(identity,captured,host,nonce){
 const {reporter,...state}=pausedStateReport(identity,captured,{peer:host,nonce});
 return {...state,type:'session-recovery-proposal',host};
}
// Received proposals must be checked against locally captured engine state.
// The caller independently retains its local attempt token and deadline.
export function recoveryAcknowledgment(message,identity,captured,{peer,host}){
 if(!identity.players.some(player=>player.peer===peer)||!equal(message,proposal(identity,captured,host,message?.nonce)))
  throw Error('Recovery proposal differs from actual paused state.');
 return {type:'session-recovery-ack',version:1,peer,accepted:structuredClone(message)};
}
// Invoke only on the authenticated host's control channel. Bind the commit to
// the exact proposal acknowledged earlier and recheck live local state.
export function acceptRecoveryCommit(message,acknowledged,identity,captured,{peer,host}){
 const ack=recoveryAcknowledgment(acknowledged,identity,captured,{peer,host});
 const expected={...ack.accepted,type:'session-recovery-commit',acknowledged:identity.players.map(player=>player.peer)};
 if(!equal(message,expected))throw Error('All-player recovery commit does not match acknowledged round.');
 return true;
}
export class RecoveryBarrier{
 #proposal;#identity;#capture;#current;#deadline;#now;#acks=new Set();#finished=false;
 constructor({identity,capture,current,host,nonce,deadline,now=()=>performance.now()}){
  if(typeof capture!=='function'||typeof current!=='function'||!Number.isFinite(deadline))throw Error('Current paused state and deadline required.');
  this.#identity=structuredClone(identity);this.#capture=capture;this.#current=current;this.#deadline=deadline;this.#now=now;
  this.#proposal=proposal(this.#identity,capture(),host,nonce);this.#validate();this.#acks.add(host);
 }
 #validate(){
  if(this.#finished||this.#now()>=this.#deadline||this.#current()!==true||
   !equal(this.#proposal,proposal(this.#identity,this.#capture(),this.#proposal.host,this.#proposal.nonce)))
   throw Error('Recovery round expired or paused state changed.');
 }
 get proposal(){return structuredClone(this.#proposal);}
 get pending(){return this.#identity.players.filter(player=>!this.#acks.has(player.peer)).map(player=>player.peer);}
 acknowledge(boundPeer,message){
  this.#validate();
  if(!this.#identity.players.some(player=>player.peer===boundPeer)||message?.peer!==boundPeer||
   message.type!=='session-recovery-ack'||message.version!==1||Object.keys(message).length!==4||!equal(message.accepted,this.#proposal))
   throw Error('Bound assigned peer acknowledgment required.');
  this.#acks.add(boundPeer);return this.pending.length===0;
 }
 commit(){
  this.#validate();if(this.pending.length)throw Error('Every assigned player must acknowledge recovery.');
  this.#finished=true;return {...this.proposal,type:'session-recovery-commit',acknowledged:this.#identity.players.map(player=>player.peer)};
 }
 cancel(){this.#finished=true;}
}
