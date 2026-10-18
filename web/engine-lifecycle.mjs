// Platform/disc setup must already be complete. This owns only core execution;
// callers authorize resume after their shader/state/start agreement gates.
export function createEngineLifecycle(module, {shutdown, sessionSeed}) {
  if(typeof module?.ccall!=='function'||typeof module?._PartyBoard_GameRequestStop!=='function'||typeof shutdown!=='function')
    throw Error('Engine exports and shutdown callback required.');
  if(sessionSeed!==undefined && (!Number.isInteger(sessionSeed)||sessionSeed<0||sessionSeed>0xffffffff||
      typeof module._PartyBoard_GameConfigureSessionSeed!=='function'))
    throw Error('Session seed must be an unsigned 32-bit integer with engine seed export.');
  let phase='New', pending, disposing=false, disposed=false, disposal, clockAligned=false, hasRun=false;
  const previousAbort=module.onAbort;
  let rejectAbort,abortError;
  const aborted=new Promise((resolve,reject)=>{rejectAbort=reject;});
  aborted.catch(()=>{});
  const onAbort=reason=>{
    abortError??=Error(`Engine aborted: ${String(reason)}`);
    rejectAbort(abortError);
    if(!disposing)phase='Failed';
    // Emscripten sets its ABORT flag after this callback returns. Callback
    // failures must not interrupt that transition or mask the engine abort.
    const callbackErrors=[];
    try {module.partyBoardCancelInputExchange?.();}catch(error){callbackErrors.push(error);}
    try {previousAbort?.(reason);}catch(error){callbackErrors.push(error);}
    if(callbackErrors.length)abortError.callbackErrors=callbackErrors;
  };
  module.onAbort=onAbort;
  const previousRendererLost=module.partyBoardRendererLost;
  const rendererLost=reason=>{
    abortError??=Error(`Renderer stopped: ${String(reason)}`);
    // Unlike abort, wait for C to exit normally before releasing its resources.
    const callbackErrors=[];
    try {module._PartyBoard_GameRequestStop();}catch(error){callbackErrors.push(error);}
    try {module.partyBoardCancelInputExchange?.();}catch(error){callbackErrors.push(error);}
    try {previousRendererLost?.(reason);}catch(error){callbackErrors.push(error);}
    if(callbackErrors.length)abortError.callbackErrors=callbackErrors;
    if(phase!=='Running'&&phase!=='Initializing'&&!disposing)phase='Failed';
  };
  module.partyBoardRendererLost=rendererLost;
  const invoke = (name, next) => {
    let result;
    try {result=module.ccall(name,'number',[],[],{async:true});}
    catch(error){result=Promise.reject(error);}
    pending=Promise.race([Promise.resolve(result),aborted]).then(code=>{
      if(abortError)throw abortError;
      if(code!==0)throw Error(`${name} rejected engine lifecycle operation.`);
      if(!disposing)phase=next;
    }).catch(error=>{if(!disposing)phase='Failed';throw error;});
    return pending;
  };
  return {
    get phase(){return phase;},
    initialize(){
      if(phase!=='New'||disposing)throw Error('Engine initialization unavailable.');
      phase='Initializing';
      if(sessionSeed!==undefined) {
        try {
          if(module._PartyBoard_GameConfigureSessionSeed(sessionSeed)!==0)
            throw Error('Engine rejected session seed configuration.');
        } catch(error) {phase='Failed';return Promise.reject(error);}
      }
      return invoke('PartyBoard_GameInitialize','Idle');
    },
    alignStartClock(){
      if(phase!=='Idle'||disposing||clockAligned||hasRun)
        throw Error('Start clock alignment requires a fresh initialized idle engine.');
      if(typeof module._PartyBoard_GameAlignStartClock!=='function')
        throw Error('Engine start clock export required.');
      if(module._PartyBoard_GameAlignStartClock()!==0)throw Error('Engine rejected start clock alignment.');
      clockAligned=true;
    },
    pausedState(){
      if(phase!=='Idle'||disposing||typeof module._PartyBoard_GamePausedState!=='function')
        throw Error('Paused state requires an initialized idle engine and state export.');
      const state={};
      for(const [field,key] of ['frame','checkpoint','frand','rand8'].entries()) {
        const value=module._PartyBoard_GamePausedState(field);
        if(!Number.isInteger(value)||value<0||value>0xffffffff)throw Error('Engine paused state unavailable.');
        state[key]=value;
      }
      return state;
    },
    preparedCheckpoint(){
      if(phase!=='Idle'||disposing||!clockAligned||hasRun)
        throw Error('Prepared checkpoint requires an aligned pre-frame idle engine.');
      if(typeof module._PartyBoard_GamePreparedCheckpoint!=='function')
        throw Error('Engine prepared checkpoint export required.');
      const checkpoint=module._PartyBoard_GamePreparedCheckpoint();
      if(!Number.isInteger(checkpoint)||checkpoint<0||checkpoint>0xffffffff)
        throw Error('Engine prepared checkpoint unavailable.');
      return checkpoint;
    },
    resume(){
      if(phase!=='Idle'||disposing)throw Error('Engine must be initialized and idle.');
      phase='Running';
      hasRun=true;
      return invoke('PartyBoard_GameResume','Idle');
    },
    requestStop(){if(phase==='Running'){module.partyBoardCancelInputExchange?.();module._PartyBoard_GameRequestStop();}},
    dispose(){
      if(disposed)return Promise.resolve();
      if(disposal)return disposal;
      const wasRunning=phase==='Running';
      disposing=true; phase='Disposing';
      // Invoke stop immediately, after resume has entered C (never defer its
      // entry to a microtask, which could let resume overwrite an early stop).
      if(wasRunning) {
        try {module.partyBoardCancelInputExchange?.();module._PartyBoard_GameRequestStop();}
        catch(error){disposing=false;phase='Running';throw error;}
      }
      disposal=(async()=>{
        // Operation errors belong to the initialize/resume promise. Cleanup
        // still waits for that operation to settle before releasing resources.
        await pending?.catch(()=>{});
        await shutdown(module);
        if(module.onAbort===onAbort)module.onAbort=previousAbort;
        if(module.partyBoardRendererLost===rendererLost)module.partyBoardRendererLost=previousRendererLost;
        disposed=true;phase='Disposed';module=undefined;
      })().catch(error=>{disposal=undefined;throw error;});
      return disposal;
    },
  };
}

// Takes ownership of an already configured platform module. The caller must
// still pass every readiness/start gate before authorizing engine.resume().
export async function prepareEngineLifecycle(module,{shutdown,sessionSeed,signal}) {
  const engine=createEngineLifecycle(module,{shutdown,sessionSeed});
  try {
    signal?.throwIfAborted();
    if(sessionSeed===undefined)throw Error('Session preparation requires an agreed seed.');
    await engine.initialize();
    signal?.throwIfAborted();
    engine.alignStartClock();
    const checkpoint=engine.preparedCheckpoint();
    signal?.throwIfAborted();
    return {checkpoint,sessionSeed,engine,dispose:()=>engine.dispose()};
  } catch(error) {
    try {await engine.dispose();}
    catch(cleanupError){throw new AggregateError([error,cleanupError],'Engine preparation and cleanup failed.');}
    throw error;
  }
}
