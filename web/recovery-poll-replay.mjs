import {decodeInput,encodeInput} from './input-packet.mjs';
import {pausedStateReport} from './paused-state-agreement.mjs';
const equal=(a,b)=>a.length===b.length&&a.every((value,index)=>value===b[index]);
const uint32=value=>Number.isInteger(value)&&value>=0&&value<=0xffffffff;

// Call only on the original peer's authenticated replacement channel. Validation
// produces candidate packets, not permission to resume or advance the engine.
// The resulting actual pending state must still pass strict two-sided agreement.
export function validateRecoveryPollReplay({identity,peer,remotePeer,nonce,captured,report,receipt,owners,pendingPackets}){
 const local=pausedStateReport(identity,captured,{peer,nonce});
 if(peer===remotePeer||!identity.players.some(player=>player.peer===remotePeer)||
    !Array.isArray(owners)||owners.length!==4||!owners.every(uint32)||
    !Array.isArray(pendingPackets)||!pendingPackets.length||!report||
    Object.keys(report).length!==Object.keys(local).length)
  throw Error('Authenticated assigned replay and original pending packets required.');
 const changing=new Set(['reporter','frame','inputFrame','checkpoint','frand','rand8']);
 for(const key of Object.keys(local)){
  if(changing.has(key))continue;
  if(key==='players'?JSON.stringify(report[key])!==JSON.stringify(local[key]):report[key]!==local[key])
   throw Error('Replay session identity differs.');
 }
 if(report.reporter!==remotePeer||report.inputFrame!==(captured.input.frame+1n).toString()||
    report.frame!==local.frame+1||!['checkpoint','frand','rand8'].every(key=>uint32(report[key])))
  throw Error('Replay requires exactly one completed game and input poll.');
 if(!receipt||Object.keys(receipt).length!==5||receipt.frame!==local.inputFrame||
    !['checkpoint','frand','rand8'].every(key=>receipt[key]===local[key])||
    !Array.isArray(receipt.pads)||receipt.pads.length!==44||
    !receipt.pads.every(value=>Number.isInteger(value)&&value>=0&&value<=255))
  throw Error('Replay does not match the original pending checkpoint and RNG.');
 const session=Uint8Array.from(identity.session.match(/../g),byte=>parseInt(byte,16)),pads=Uint8Array.from(receipt.pads);
 if(owners.some((owner,slot)=>Boolean(owner)!==identity.players.some(player=>player.slot===slot)))
  throw Error('Replay controller membership differs.');
 const missingOwnerSlots=owners.flatMap((owner,slot)=>owner===captured.attempt.peer?[slot]:[]);
 // attempt.peer identifies the missing remote owner, not the local owner. Infer
 // the local owned slots from its original packets and require one owner only.
 const slots=new Set();let localOwner;
 for(const bytes of pendingPackets){
  const packet=decodeInput(bytes);
  if(!packet||!equal(packet.session,session)||packet.frame!==captured.input.frame||
     packet.checkpoint!==local.checkpoint||!owners[packet.slot]||slots.has(packet.slot)||
     !equal(packet.pad,pads.slice(packet.slot*11,packet.slot*11+11)))
   throw Error('Replay changes original local input.');
  localOwner??=owners[packet.slot];if(owners[packet.slot]!==localOwner)throw Error('Original local input ownership differs.');
  slots.add(packet.slot);
 }
 if(owners.filter(owner=>owner===localOwner).length!==slots.size||!missingOwnerSlots.length)
  throw Error('Complete assigned original local input required.');
 const packets=[];
 owners.forEach((owner,slot)=>{
  const pad=pads.slice(slot*11,slot*11+11);
  if(!owner){if(pad.some((value,index)=>value!==(index===10?255:0)))throw Error('Replay changes inactive controller input.');return;}
  packets.push({owner,bytes:encodeInput({session,frame:captured.input.frame,checkpoint:local.checkpoint,slot,pad})});
 });
 return {packets,target:{frame:report.frame,inputFrame:report.inputFrame,checkpoint:report.checkpoint,frand:report.frand,rand8:report.rand8}};
}
