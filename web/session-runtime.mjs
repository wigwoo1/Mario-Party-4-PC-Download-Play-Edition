import {installPipelineSeed} from './pipeline-seed.mjs';
import {prepareMountedRuntime} from './runtime-mount.mjs';
import {prepareEngineLifecycle} from './engine-lifecycle.mjs';
import {prepareFingerprintedRuntimeFactory} from './runtime-build.mjs';
import {installBrowserInputExchange} from './browser-input-exchange.mjs';

// Carries the measured executable identity into the owned prepared session.
export async function prepareFingerprintedSessionRuntime(store,assetId,files,{
  runtimeBuild={},runtimeOptions={},...sessionOptions
}) {
  const factory=await prepareFingerprintedRuntimeFactory(files,{...runtimeBuild,signal:sessionOptions.signal});
  const runtime=await prepareSessionRuntime(store,assetId,
    options=>factory.createRuntime({...runtimeOptions,...options}),sessionOptions);
  Object.defineProperty(runtime,'buildId',{value:factory.buildId,enumerable:true});
  return runtime;
}

// Prepare only; synchronized start owns permission to resume this runtime.
export async function prepareSessionRuntime(store,assetId,createRuntime,{
  sessionSeed,setup,shutdown,signal,progress,mount,prepareEngine=prepareEngineLifecycle,prepareRenderer,inputExchange,pipelineSeed,
}) {
  if(!Number.isInteger(sessionSeed)||sessionSeed<0||sessionSeed>0xffffffff||
      typeof setup!=='function'||typeof shutdown!=='function'||typeof prepareEngine!=='function'||
      (prepareRenderer!==undefined&&typeof prepareRenderer!=='function')||
      (inputExchange!==undefined&&typeof inputExchange!=='function'))
    throw Error('Agreed seed, platform setup and shutdown required.');
  let prepared,inputBinding,seedMount,platformStopped=false;
  const stopPlatform=async module=>{if(!platformStopped){await shutdown(module);platformStopped=true;}inputBinding?.close();seedMount?.dispose();};
  const mounted=await prepareMountedRuntime(store,assetId,createRuntime,{
    signal,progress,mount,shutdown:async module=>{
      inputBinding?.cancel();
      if(prepared)await prepared.dispose();else await stopPlatform(module);
    },
  });
  try {
    if(pipelineSeed)seedMount=await installPipelineSeed(mounted.module,pipelineSeed,{signal});
    await setup(mounted.module,{signal});signal?.throwIfAborted();
    prepared=await prepareEngine(mounted.module,{sessionSeed,signal,shutdown:stopPlatform});
    signal?.throwIfAborted();
    let rendererPreparation;
    if(prepareRenderer) {
      const validate=()=>{
        if(prepared.engine?.phase!=='Idle'||prepared.engine.preparedCheckpoint()!==prepared.checkpoint)
          throw Error('Renderer preparation requires the same idle engine checkpoint.');
      };
      validate();
      rendererPreparation=await prepareRenderer(mounted.module,{engine:prepared.engine,signal});
      signal?.throwIfAborted();validate();
    }
    if(inputExchange) {
      if(prepared.engine?.phase!=='Idle'||prepared.engine.preparedCheckpoint()!==prepared.checkpoint)
        throw Error('Input exchange installation requires the prepared idle engine.');
      inputBinding=installBrowserInputExchange(mounted.module,inputExchange);
      signal?.throwIfAborted();
    }
    // Receipt is caller-defined evidence, not automatic lobby shader readiness.
    return {...prepared,pipelineSeed:seedMount?{sha256:seedMount.sha256,size:seedMount.size,scope:seedMount.scope}:undefined,rendererPreparation,inputExchangeInstalled:!!inputBinding,assetId:mounted.assetId,assetReadStats:mounted.stats,get module(){return mounted.module;},dispose:mounted.dispose};
  } catch(cause) {
    try {await mounted.dispose();}
    catch(cleanup){throw new AggregateError([cause,cleanup],'Session preparation and cleanup failed.');}
    throw cause;
  }
}
