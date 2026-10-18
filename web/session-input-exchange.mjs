import {encodeInput} from './input-packet.mjs';
import {validateRecoveryPollReplay} from './recovery-poll-replay.mjs';

// Caller owns authenticated transport, checkpoint capture and recovery ticks.
// Queue frame counts PAD polls, independently of the game's render counter.
export function createSessionInputExchange(queue,{session,owners,peer,checkpoint,send,localPadSlots,suspendOnPause=false,now=()=>performance.now()}) {
  if(!(session instanceof Uint8Array)||session.length!==16||!session.some(Boolean)||
      !Array.isArray(owners)||owners.length!==4||!owners.every(owner=>Number.isInteger(owner)&&owner>=0&&owner<=0xffffffff)||
      !peer||!owners.includes(peer)||typeof checkpoint!=='function'||typeof send!=='function')throw Error('Valid session input ownership required.');
  // Global game slots map to local physical PAD lanes. Legacy callers retain
  // identity mapping; lobby callers assign local devices in their own order.
  localPadSlots ??= owners.map((owner,slot)=>owner===peer?slot:null);
  if(!Array.isArray(localPadSlots)||localPadSlots.length!==4||
      !localPadSlots.every((source,slot)=>owners[slot]===peer?
        Number.isInteger(source)&&source>=0&&source<4:source===null)||
      new Set(localPadSlots.filter(source=>source!==null)).size!==owners.filter(owner=>owner===peer).length)
    throw Error('Unique local PAD lanes required for owned game slots.');
  session=session.slice();owners=owners.slice();localPadSlots=localPadSlots.slice();let pending,retained,completed,dispatching=false;
  const complete=frame=>{
    const bytes=new Uint8Array(44);frame.pads.forEach((pad,slot)=>bytes.set(pad,slot*11));
    completed={frame:frame.index,checkpoint:frame.checkpoint,frand:retained.frand,rand8:retained.rand8,
      pads:bytes.slice(),packets:retained.packets.map(packet=>packet.slice())};
    retained=undefined;pending.finish(bytes);
  };
  const notify=()=>{
    if(!pending||dispatching)return;
    const state=queue.status(now());
    if(state.state==='Paused'&&suspendOnPause)return;
    if(state.state!=='Running'){pending.finish(null);return;}
    const frame=queue.advance(pending.checkpoint,now());
    if(frame){
      // Keep exactly the last consumed poll. A disconnect can leave another
      // peer waiting for this poll while this runtime already waits for the next.
      // This receipt is evidence only; it does not authorize paused advancement.
      complete(frame);
    }
    else if(queue.status(now()).state!=='Running')pending.finish(null);
  };
  const exchange=(pads,options={})=>{
    const {signal}=options;
    if(pending||!(pads instanceof Uint8Array)||pads.length!==44)throw Error('One complete PAD poll required.');
    const initialState=queue.status(now()).state;
    if(signal?.aborted||(initialState!=='Running'&&!(suspendOnPause&&initialState==='Paused')))return Promise.resolve(null);
    const hash=checkpoint(options);const frame=queue.status(now()).frame;
    if(retained&&(retained.frame!==frame||retained.checkpoint!==hash||retained.frand!==options.frand||retained.rand8!==options.rand8)) {
      queue.cancel();throw Error('Pending input checkpoint or poll changed.');
    }
    if(!retained)retained={frame,checkpoint:hash,frand:options.frand,rand8:options.rand8,packets:owners.flatMap((owner,slot)=>owner===peer?
      [encodeInput({session,frame,checkpoint:hash,slot,pad:pads.slice(localPadSlots[slot]*11,localPadSlots[slot]*11+11)})]:[])};
    const packets=retained.packets;
    return new Promise((resolve,reject)=>{
      const abort=()=>pending?.finish(null);
      pending={checkpoint:hash,finish(value,error){signal?.removeEventListener('abort',abort);pending=undefined;error?reject(error):resolve(value);}};
      signal?.addEventListener('abort',abort,{once:true});
      if(initialState==='Paused')return;
      try {
        dispatching=true;
        for(const packet of packets){
          if(!pending)break;
          if(!['Accepted','Duplicate'].includes(queue.receive(peer,packet,now())))throw Error('Local input rejected.');
          send(packet.slice());
        }
        dispatching=false;notify();
      } catch(error){dispatching=false;queue.cancel();pending?.finish(null,error);}
    });
  };
  // Recovery owner must authenticate state and explicitly resume the queue first.
  const resend=()=>{
    if(!pending||!retained||queue.status(now()).state!=='Running')throw Error('Running pending input required for resend.');
    try {
      dispatching=true;
      for(const packet of retained.packets){
        if(!pending||queue.status(now()).state!=='Running')break;
        if(!['Accepted','Duplicate'].includes(queue.receive(peer,packet,now())))throw Error('Retained local input rejected.');
        send(packet.slice());
      }
      dispatching=false;
    }
    catch(error){dispatching=false;queue.cancel();pending?.finish(null,error);throw error;}
    notify();
  };
  // The caller must own the authenticated replacement and fresh actual capture.
  // Completion releases only the original PAD promise; the next exchange remains
  // suspended. Strict state agreement and all-player commit remain mandatory.
  const replayCompletedPoll=options=>{
    const captured=options.captured,state=queue.status(now());
    if(!suspendOnPause||!pending||!retained||dispatching||state.state!=='Paused'||
       captured?.input?.state!==state.state||!['epoch','revision','frame'].every(key=>captured.input[key]===state[key])||
       !['frame','checkpoint','frand','rand8'].every(key=>captured.attempt[key]===retained[key]))
      throw Error('Current original paused poll required for replay.');
    const replay=validateRecoveryPollReplay({...options,owners,pendingPackets:retained.packets});
    if(!queue.canReplayPaused(state,now()))throw Error('Paused replay consumption rejected.');
    for(const packet of replay.packets){
      const result=queue.receive(packet.owner,packet.bytes,now());
      if(!['Accepted','Duplicate'].includes(result)){notify();throw Error('Replay input rejected: '+result);}
    }
    const frame=queue.advancePausedReplay({...state,checkpoint:retained.checkpoint},now());
    if(!frame){notify();throw Error('Paused replay consumption rejected.');}
    complete(frame);return replay.target;
  };
  return {exchange,notify,resend,replayCompletedPoll,pendingPackets(){return pending&&retained?retained.packets.map(packet=>packet.slice()):[];},completedState(){return completed?Object.freeze({
    frame:completed.frame,checkpoint:completed.checkpoint,frand:completed.frand,rand8:completed.rand8,
    pads:completed.pads.slice(),packets:completed.packets.map(packet=>packet.slice())}):null;},pendingState(){return pending&&retained?
    Object.freeze({frame:retained.frame,checkpoint:retained.checkpoint,frand:retained.frand,rand8:retained.rand8}):null;}};
}
