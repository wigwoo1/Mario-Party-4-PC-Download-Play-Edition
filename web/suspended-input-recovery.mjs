// Caller supplies authenticated state verification and a monotonic clock.
// Engine stays suspended inside its PAD callback; never restart its game loop.
export function createSuspendedInputRecovery(queue,bridge,{dispose,returnToMenu,transportReady,now=()=>performance.now()}) {
  if(typeof transportReady!=='function')throw Error('Transport readiness required.');
  let finishing,disposed=false;
  const verificationController=new AbortController();
  let endVerification;
  const terminal=new Promise(resolve=>{endVerification=()=>resolve(false);});
  const finish=()=>{
    endVerification();
    verificationController.abort();
    // Publish ownership before callbacks run: transport teardown may synchronously
    // request another close, which must join this cleanup instead of starting one.
    if(!finishing)finishing=Promise.resolve().then(async()=>{bridge.notify();if(!disposed){await dispose();disposed=true;}await returnToMenu();})
      .catch(error=>{finishing=undefined;throw error;});
    return finishing;
  };
  const tick=async()=>{
    const state=queue.status(now());bridge.notify();
    if(state.state==='ReturnToMenu')await finish();
    return state;
  };
  const attempt=peer=>{
    const state=queue.status(now()),pending=bridge.pendingState();
    if(state.state!=='Paused'||!queue.hasPeer(peer)||!pending||pending.frame!==state.frame)return null;
    return Object.freeze({peer,epoch:state.epoch,revision:state.revision,...pending});
  };
  const current=token=>{
    const actual=attempt(token.peer);
    return actual&&['peer','epoch','revision','frame','checkpoint','frand','rand8'].every(key=>actual[key]===token[key]);
  };
  const restore=async(token,verify)=>{
    token=Object.freeze({...token});
    if(!current(token)||transportReady(token.peer)!==true){await tick();return false;}
    let verified;
    try { verified=await Promise.race([Promise.resolve().then(()=>{
      if(verificationController.signal.aborted||!current(token)||transportReady(token.peer)!==true)return false;
      return verify(token,{signal:verificationController.signal});
    }),terminal]); }
    catch(error) { await tick();throw error; }
    if(verified!==true){await tick();return false;}
    if(!current(token)||transportReady(token.peer)!==true){await tick();return false;}
    if(!queue.recovered(token.peer,token,now())){await tick();return false;}
    // Other missing peers can be verified separately without resuming yet.
    if(transportReady()!==true||!queue.resume(token,now())){await tick();return false;}
    try { bridge.resend(); }
    catch(error) { await tick();throw error; }
    if(queue.status(now()).state!=='Running'){await tick();return false;}
    return true;
  };
  return {attempt,restore,tick,async close(){queue.cancel();await finish();}};
}
