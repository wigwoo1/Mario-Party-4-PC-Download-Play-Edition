import {ReconnectPolicy} from './reconnect-policy.mjs';

// Owns a transferred client session during connection loss. Caller drives tick
// using monotonic wall time; state recovery/resume remains separately gated.
export function createSessionRecovery(session,{owners,hostPeer,now=()=>performance.now(),returnToMenu}) {
  if(typeof returnToMenu!=='function'||typeof session?.runtime?.engine?.requestStop!=='function'||
      typeof session.runtime.dispose!=='function'||!owners.includes(hostPeer)||!hostPeer)
    throw Error('Transferred session and assigned host required.');
  const policy=new ReconnectPolicy(owners);
  let paused,cleanup;
  const waitForStop=()=>{
    paused=Promise.resolve(session.execution);
    // Observe failure immediately, even while waiting for the wall-clock
    // deadline, and make it terminal instead of permitting recovery.
    void paused.catch(()=>policy.cancel());
  };
  const detach=()=>{session.channel.removeEventListener('close',lost);session.channel.removeEventListener('error',lost);};
  const finish=()=>{
    if(cleanup)return cleanup;
    detach();session.channel.close();
    cleanup=(async()=>{await paused?.catch(()=>{});await session.runtime.dispose();await returnToMenu();})();
    cleanup=cleanup.catch(error=>{cleanup=undefined;throw error;});return cleanup;
  };
  const lost=()=>{
    if(policy.state!=='Running')return;
    policy.disconnected(hostPeer,now());
    try {session.runtime.engine.requestStop();waitForStop();}
    catch {policy.cancel();}
  };
  session.channel.addEventListener('close',lost);session.channel.addEventListener('error',lost);
  if(session.channel.readyState!=='open')lost();
  return {get state(){return policy.state;},get remainingMs(){return policy.remainingMs(now());},
    get attempt(){return {epoch:policy.epoch,revision:policy.revision,peer:hostPeer};},
    async verifyRecoveredHost(attempt,verify){
      attempt=attempt?Object.freeze({...attempt}):undefined;
      if(typeof verify!=='function'||policy.update(now())!=='Paused'||attempt?.peer!==hostPeer||
          attempt.epoch!==policy.epoch||attempt.revision!==policy.revision)return false;
      // Actual stopped execution must settle before caller compares recovered
      // state. The callback owns authenticated identity and state verification.
      try {await paused;}catch{return false;}
      if(policy.update(now())!=='Paused'||attempt.epoch!==policy.epoch||attempt.revision!==policy.revision)return false;
      const verified=await verify(Object.freeze({...attempt}));
      if(verified!==true)return false;
      return policy.markRecovered(hostPeer,attempt.epoch,attempt.revision,now());
    },
    resumeRecoveredHost(attempt,channel,authorize){
      attempt=attempt?Object.freeze({...attempt}):undefined;
      if(attempt?.peer!==hostPeer||attempt.epoch!==policy.epoch||attempt.revision!==policy.revision||
          !policy.readyToResume(now())||session.runtime.engine.phase!=='Idle'||
          channel?.readyState!=='open'||channel.ordered===false||channel.maxRetransmits!=null||
          channel.maxPacketLifeTime!=null||typeof authorize!=='function')return false;
      // Caller owns the session-bound synchronized recovery commit. Merely
      // reopening a socket or verifying a checkpoint never authorizes resume.
      if(authorize(attempt)!==true||channel.readyState!=='open'||session.runtime.engine.phase!=='Idle'||
          !policy.resume(attempt.epoch,attempt.revision,now()))return false;
      detach();session.channel.close();session.channel=channel;
      channel.addEventListener('close',lost);channel.addEventListener('error',lost);
      try {session.execution=session.runtime.engine.resume();paused=undefined;}
      catch(error){policy.cancel();throw error;}
      void Promise.resolve(session.execution).catch(()=>policy.cancel());
      return true;
    },
    tick(){if(policy.update(now())==='ReturnToMenu')return finish();return Promise.resolve();},
    close(){policy.cancel();session.runtime.engine.requestStop();waitForStop();return finish();}};
}
