// Drain already-discovered renderer work. This does not discover game shaders
// or authorize lobby shader readiness; caller still owns coverage evidence.
export async function drainPreparedPipelines({read,advance,signal,maxSteps=600,progress=()=>{}}) {
  if(typeof read!=='function'||typeof advance!=='function'||typeof progress!=='function'||
      !Number.isInteger(maxSteps)||maxSteps<1||maxSteps>10000)
    throw Error('Pipeline statistics, renderer step and bounded limit required.');
  for(let steps=0;steps<=maxSteps;steps++) {
    signal?.throwIfAborted();
    const stats=await read();signal?.throwIfAborted();
    if(!stats||!['queued','created'].every(key=>Number.isSafeInteger(stats[key])&&stats[key]>=0))
      throw Error('Invalid renderer pipeline statistics.');
    // Snapshot only: do not let callbacks mutate the decision or receipt.
    const queued=stats.queued,created=stats.created;
    progress({steps,queued,created});signal?.throwIfAborted();
    if(queued===0)return {steps,queued,created,scope:'discovered-pipelines-only'};
    if(steps===maxSteps)throw Error('Renderer pipeline drain limit reached.');
    await advance({signal});signal?.throwIfAborted();
  }
}

// Keep renderer preparation bound to the same idle initialized simulation.
export async function drainIdleEnginePipelines(engine,{read,advance,...options}) {
  if(typeof engine?.preparedCheckpoint!=='function'||engine.phase!=='Idle'||
      typeof read!=='function'||typeof advance!=='function')throw Error('Idle prepared engine and renderer callbacks required.');
  const checkpoint=engine.preparedCheckpoint();
  const validate=()=>{
    if(engine.phase!=='Idle'||engine.preparedCheckpoint()!==checkpoint)
      throw Error('Renderer preparation changed the prepared simulation.');
  };
  const receipt=await drainPreparedPipelines({...options,
    read:async()=>{validate();const stats=await read();validate();return stats;},
    advance:async options=>{validate();await advance(options);validate();},
  });
  validate();return {...receipt,checkpoint};
}
