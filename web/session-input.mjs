import {Lockstep} from './lockstep.mjs';
import {ReconnectPolicy} from './reconnect-policy.mjs';

// Session-level gate only. The transport must authenticate peers and verify
// recovered simulation state before reporting recovery or synchronized resume.
export class SessionInput {
  #queue; #reconnect; #owners; #replayedEpoch=null; #waitingSince=null;
  constructor(session,owners) {
    this.#queue=new Lockstep(session,owners);
    this.#reconnect=new ReconnectPolicy(owners);
    this.#owners=new Set(owners.filter(Boolean));
  }
  hasPeer(peer) {return this.#owners.has(peer);}
  status(now) {
    this.#reconnect.update(now);
    if(this.#queue.stopped!=='None')this.#reconnect.cancel();
    if(this.#reconnect.state==='ReturnToMenu')this.#queue.cancel();
    return {state:this.#reconnect.state,frame:this.#queue.nextFrame,
      epoch:this.#reconnect.epoch,revision:this.#reconnect.revision,
      missingSlots:this.#reconnect.missingSlots,remainingMs:this.#reconnect.remainingMs(now),
      stopReason:this.#queue.stopped};
  }
  receive(peer,bytes,now) {
    if(this.status(now).state==='ReturnToMenu')return 'Halted';
    const result=this.#queue.receive(peer,bytes);this.status(now);return result;
  }
  advance(checkpoint,now) {
    if(this.status(now).state!=='Running')return null;
    const frame=this.#queue.advance(checkpoint);this.status(now);
    if(frame)this.#waitingSince=null;
    else if(this.#reconnect.state==='Running')this.#waitingSince??=now;
    return frame;
  }
  // Internal recovery consumption, after authenticated replay validation. This
  // leaves missing owners and policy untouched; ordinary advance stays frozen.
  // At most one consumed poll is allowed in a disconnect epoch, even if another
  // lost route changes the policy revision while recovery is in progress.
  canReplayPaused({epoch,revision,frame},now) {
    const state=this.status(now);
    return state.state==='Paused'&&state.epoch===epoch&&state.revision===revision&&
       state.frame===frame&&this.#replayedEpoch!==epoch;
  }
  advancePausedReplay({epoch,revision,frame,checkpoint},now) {
    if(!this.canReplayPaused({epoch,revision,frame},now))return null;
    const result=this.#queue.advance(checkpoint);this.status(now);
    if(result){this.#replayedEpoch=epoch;this.#waitingSince=null;}
    return result;
  }
  disconnected(peer,now) {
    this.status(now);return this.#reconnect.disconnected(peer,now,this.#waitingSince??now);
  }
  recovered(peer,{epoch,revision,frame},now) {
    this.status(now);
    if(frame!==this.#queue.nextFrame)return false;
    return this.#reconnect.markRecovered(peer,epoch,revision,now);
  }
  resume({epoch,revision,frame},now) {
    this.status(now);
    if(frame!==this.#queue.nextFrame)return false;
    return this.#reconnect.resume(epoch,revision,now);
  }
  cancel(){this.#reconnect.cancel();this.#queue.cancel();}
}
