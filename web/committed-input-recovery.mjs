import {acceptRecoveryCommit} from './recovery-barrier.mjs';
// Synchronous validation and mutation prevent another recovery round from
// interleaving between the live token check and resuming the retained poll.
export function resumeCommittedInput({message,acknowledged,identity,peer,host,capture,current,queue,bridge,transportReady,now=()=>performance.now()}){
 if(current()!==true||transportReady()!==true)throw Error('Current attempt and all assigned transports required.');
 const captured=capture(),token=captured.attempt,time=now();
 acceptRecoveryCommit(message,acknowledged,identity,captured,{peer,host});
 const status=queue.status(time);
 if(status.state!=='Paused'||status.frame!==token.frame||status.epoch!==token.epoch||status.revision!==token.revision)
  throw Error('Recovery input token is stale or expired.');
 const owners=new Set(identity.players.filter(player=>status.missingSlots[player.slot]).map(player=>player.slot+1));
 for(let slot=0;slot<4;slot++)if(status.missingSlots[slot]&&!owners.has(slot+1))throw Error('Missing input owner is not assigned.');
 for(const owner of owners)if(!queue.recovered(owner,token,time))throw Error('Assigned input owner recovery rejected.');
 if(!queue.resume(token,time))throw Error('All-player input resume rejected.');
 bridge.resend();bridge.notify();return queue.status(now()).state==='Running';
}
