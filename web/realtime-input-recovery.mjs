import {createSuspendedInputRecovery} from './suspended-input-recovery.mjs';

// Own the monotonic deadline independently of network traffic or game frames.
// The caller still authenticates replacement peers and verifies recovery state.
export function createRealtimeInputRecovery(queue,bridge,{
  channels,dispose,returnToMenu,transportReady,execution,now=()=>performance.now(),
  intervalMs=100,schedule=setTimeout,cancel=clearTimeout,error=()=>{},
}) {
  if(typeof channels?.close!=='function'||typeof dispose!=='function'||
      typeof returnToMenu!=='function'||!Number.isFinite(intervalMs)||intervalMs<1||
      typeof schedule!=='function'||typeof cancel!=='function'||typeof error!=='function')
    throw Error('Owned transport, runtime, menu and deadline scheduler required.');
  let stopped=false,timer;
  const stop=()=>{stopped=true;if(timer!==undefined){cancel(timer);timer=undefined;}};
  const recovery=createSuspendedInputRecovery(queue,bridge,{
    now,transportReady,
    dispose:async()=>{await channels.close();await dispose();},
    returnToMenu:async()=>{await returnToMenu();stop();},
  });
  const check=async()=>{
    timer=undefined;if(stopped)return;
    try {await recovery.tick();}
    catch(cause){error(cause);}
    finally {if(!stopped)timer=schedule(check,intervalMs);}
  };
  timer=schedule(check,intervalMs);
  // Own termination as well as transport loss. A stopped/failed execution must
  // never leave an input queue running or retain connections until the deadline.
  if(execution!==undefined)void Promise.resolve(execution).then(
    ()=>{queue.cancel();return recovery.tick();},
    cause=>{queue.cancel();error(cause);return recovery.tick();},
  ).catch(error);
  return {
    attempt:recovery.attempt,restore:recovery.restore,tick:recovery.tick,
    async close(){stop();await recovery.close();},
  };
}
