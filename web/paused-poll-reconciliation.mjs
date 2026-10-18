import {pausedStateReport} from './paused-state-agreement.mjs';
const same=(a,b)=>JSON.stringify(a)===JSON.stringify(b);
const matches=(a,b)=>a&&Object.keys(a).length===Object.keys(b).length&&Object.keys(b).every(key=>
 key==='reporter'||(key==='players'?same(a[key],b[key]):a[key]===b[key]));

// The channel has already proved possession of the original recovery credential.
// Reconciliation is bounded by the original monotonic deadline and never resumes
// ordinary input. The caller must follow with strict paused state agreement.
export function reconcilePausedPoll({channel,identity,peer,remotePeer,nonce,capture,current,bridge,signal,deadline,allowStartChannel=false,previousNonces=new Set(),now=()=>performance.now()}){
 if(!(previousNonces instanceof Set)||previousNonces.size>16||[...previousNonces].some(value=>!/^[a-f0-9]{32}$/.test(value)))
  throw Error('Owned completed reconciliation nonces required.');
 const completedNonces=new Set(previousNonces);
 if(!(channel?.label==='partyboard-recovery-v1'||(allowStartChannel===true&&channel?.label==='partyboard-start-v1'))||channel.readyState!=='open'||channel.ordered!==true||
    channel.maxRetransmits!=null||channel.maxPacketLifeTime!=null||peer===remotePeer||
    !identity.players.some(player=>player.peer===remotePeer)||typeof capture!=='function'||typeof current!=='function'||
    typeof bridge?.replayCompletedPoll!=='function'||!Number.isFinite(deadline))throw Error('Authenticated paused reconciliation required.');
 const initial=pausedStateReport(identity,capture(),{peer,nonce}),completed=bridge.completedState();
 const receipt=completed?{frame:completed.frame.toString(),checkpoint:completed.checkpoint,frand:completed.frand,rand8:completed.rand8,pads:Array.from(completed.pads)}:null;
 const local={type:'paused-poll-report',version:1,nonce,peer,report:initial,receipt};
 return new Promise((resolve,reject)=>{
  let done=false,timer,remote,remoteAck,ready=false,replaying=false,target;
  const cleanup=()=>{clearTimeout(timer);channel.removeEventListener('message',message);channel.removeEventListener('close',lost);channel.removeEventListener('error',lost);signal?.removeEventListener('abort',aborted);};
  const fail=error=>{if(done)return;done=true;cleanup();reject(error);};
  const valid=()=>{if(signal?.aborted)throw signal.reason||Error('Recovery aborted.');if(now()>=deadline)throw Error('Paused reconciliation deadline expired.');
   if(current()!==true||channel.readyState!=='open')throw Error('Paused reconciliation ownership changed.');};
  const actual=()=>pausedStateReport(identity,capture(),{peer,nonce});
  const check=()=>{
   valid();if(!remote)return;
   if(replaying&&!bridge.pendingState())return;
   const state=actual();
   if(target){if(state.frame!==target.frame||state.inputFrame!==target.inputFrame)return;
    if(!['checkpoint','frand','rand8'].every(key=>state[key]===target[key]))throw Error('Replayed actual state differs.');ready=true;}
   if(!ready)return;
   channel.send(JSON.stringify({type:'paused-poll-ack',version:1,nonce,peer,accepted:remote,state}));
   if(remoteAck){if(!matches(remoteAck.state,state))throw Error('Reconciled peer state differs.');done=true;cleanup();resolve(state);}
  };
  const message=event=>{if(done)return;try{
   valid();if(typeof event.data!=='string'||event.data.length>4096)throw Error('Invalid reconciliation packet.');
   const packet=JSON.parse(event.data);
   // Discard only retries from rounds completed on this owned peer/route.
   // They supply no permission or state to the current reconciliation.
   if(['paused-poll-report','paused-poll-ack'].includes(packet.type)&&packet.version===1&&
      packet.peer===remotePeer&&packet.nonce!==nonce&&completedNonces.has(packet.nonce))return;
   if(packet.type==='paused-state-report'||packet.type==='paused-state-ack')return; // next phase may arrive before our final ack
   if(packet.version!==1||packet.nonce!==nonce||packet.peer!==remotePeer)
    throw Error('Reconciliation peer identity differs: type='+String(packet.type).slice(0,80)+
      ', versionMatches='+(packet.version===1)+', nonceMatches='+(packet.nonce===nonce)+', peerMatches='+(packet.peer===remotePeer)+'.');
   if(packet.type==='paused-poll-report'){
    if(Object.keys(packet).length!==6||!packet.report||packet.report.reporter!==remotePeer)throw Error('Invalid reconciliation report.');
    if(remote){if(!same(remote,packet))throw Error('Original reconciliation report changed.');check();return;}
    remote=packet;
    if(matches(packet.report,initial)){ready=true;}
    else{
     const remoteFrame=packet.report.inputFrame;
     if(remoteFrame===(BigInt(initial.inputFrame)+1n).toString()){
      replaying=true;target=bridge.replayCompletedPoll({identity,peer,remotePeer,nonce,captured:capture(),report:packet.report,receipt:packet.receipt});
     }else if(initial.inputFrame===(BigInt(remoteFrame)+1n).toString()){
      // Validate identity and the old receipt before asking the other endpoint
      // to consume it; that endpoint also verifies its original local samples.
      const common={...packet.report,reporter:peer,frame:initial.frame,inputFrame:initial.inputFrame,
       checkpoint:initial.checkpoint,frand:initial.frand,rand8:initial.rand8};
      if(!matches(common,initial)||packet.report.frame!==initial.frame-1||!receipt||receipt.frame!==remoteFrame||
         !['checkpoint','frand','rand8'].every(key=>receipt[key]===packet.report[key]))throw Error('Ahead peer receipt does not match original pending state.');
      ready=true;
     }else throw Error('Paused polls cannot be reconciled within one frame.');
    }
   }else if(packet.type==='paused-poll-ack'){
    if(Object.keys(packet).length!==6||!same(packet.accepted,local)||packet.state?.reporter!==remotePeer)throw Error('Invalid reconciliation acknowledgment.');
    if(remoteAck&&!same(remoteAck,packet))throw Error('Reconciliation acknowledgment changed.');remoteAck=packet;
   }else throw Error('Unexpected reconciliation packet.');
   check();
  }catch(error){fail(error);}};
  const lost=()=>fail(Error('Reconciliation channel closed.')),aborted=()=>fail(signal.reason||Error('Reconciliation aborted.'));
  const tick=()=>{if(done)return;try{valid();if(!remoteAck)channel.send(JSON.stringify(local));
   // The original PAD promise can settle before the next callback registers.
   // A transient absent capture during this one-frame execution is a wait only.
   if(replaying&&!bridge.pendingState()){timer=setTimeout(tick,10);return;}
   check();if(!done)timer=setTimeout(tick,20);
  }catch(error){fail(error);}};
  channel.addEventListener('message',message);channel.addEventListener('close',lost);channel.addEventListener('error',lost);signal?.addEventListener('abort',aborted,{once:true});
  tick();
 });
}
