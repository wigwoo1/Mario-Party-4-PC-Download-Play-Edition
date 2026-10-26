import {drainIdleEnginePipelines} from './pipeline-drain.mjs';
import {configureAudioDestination} from './browser-audio-destination.mjs';

const supportedWidths=new Set([640,960,1280,1920,2560]);
function supportedResolution({width,height}={}){
  if(!supportedWidths.has(width)||!Number.isInteger(height)||height<240||height>1920)return false;
  // Only expose the page's supported aspect presets. Integer rounding may
  // differ by one pixel for 21:9 at the smallest width.
  return ['4:3','16:9','16:10','21:9'].some(value=>{
    const [w,h]=value.split(':').map(Number);
    return Math.abs(width/height-w/h)<0.003;
  });
}

// Platform callbacks for prepareSessionRuntime. Game initialization, execution,
// input ownership and mounted asset lifetime remain owned by that adapter.
export function browserPlatformCallbacks({maxSteps=60,progress,resolution,audioChannels,
    checkWebGPUAdapter=false,webgpu=globalThis.navigator?.gpu,secureContext=globalThis.isSecureContext}={}) {
  if(!Number.isInteger(maxSteps)||maxSteps<1||maxSteps>10000)throw Error('Invalid renderer preparation bound.');
  if(resolution&&!supportedResolution(resolution))throw Error('Invalid game resolution.');
  if(audioChannels!==undefined&&audioChannels!==2&&audioChannels!==6)throw Error('Invalid speaker layout.');
  const selectedResolution=resolution&&{width:resolution.width,height:resolution.height};
  const platformAttempts=new WeakSet();
  const call=async(module,name,signal,onInvoke)=>{
    signal?.throwIfAborted();
    if(typeof module?.ccall!=='function'||typeof module['_'+name]!=='function')throw Error('Browser platform export missing: '+name);
    onInvoke?.();
    const result=await module.ccall(name,'number',[],[],{async:true});
    if(result!==0)throw Error('Browser platform operation failed: '+name+' ('+result+')');
    signal?.throwIfAborted();
  };
  return {
    async setup(module,{signal}={}) {
      signal?.throwIfAborted();
      if(checkWebGPUAdapter){
        if(!secureContext)throw Error('The lobby page is not a secure browser context. Open it using its HTTPS address.');
        if(typeof webgpu?.requestAdapter!=='function')throw Error('WebGPU is unavailable in this browser. Use a current Edge version with graphics acceleration enabled.');
        let adapter;
        try{adapter=await webgpu.requestAdapter({powerPreference:'high-performance'});}
        catch(cause){throw new Error('Edge could not query a WebGPU graphics adapter. Check graphics acceleration and the display driver.',{cause});}
        if(!adapter)throw Error('Edge found no WebGPU graphics adapter. Enable graphics acceleration and update the display driver, then restart Edge.');
        signal?.throwIfAborted();
      }
      if(selectedResolution){
        if(typeof module?._PartyBoardBrowserSetInitialResolution!=='function')throw Error('Browser runtime does not support selected resolution.');
        const result=await module.ccall('PartyBoardBrowserSetInitialResolution','number',['number','number'],
          [selectedResolution.width,selectedResolution.height],{async:true});
        if(result!==0)throw Error('Selected game resolution rejected.');
      }
      await call(module,'PartyBoardBrowserPreparePlatform',signal,()=>platformAttempts.add(module));
      // The effects stream opens during engine initialization, after setup.
      // Apply the local preference before that stream chooses its format.
      if(audioChannels!==undefined){
        signal?.throwIfAborted();
        if(typeof module?._PartyBoardBrowserSetAudioChannels!=='function')throw Error('Browser runtime does not support speaker selection.');
        const result=await module.ccall('PartyBoardBrowserSetAudioChannels','number',['number'],[audioChannels],{async:true});
        if(result!==0)throw Error('Selected speaker layout rejected.');
        signal?.throwIfAborted();
      }
    },
    async prepareRenderer(module,{engine,signal}) {
      if(engine?.phase!=='Idle'||typeof engine.preparedCheckpoint!=='function')throw Error('Idle prepared engine required.');
      const checkpoint=engine.preparedCheckpoint();
      // Engine initialization has opened SDL audio. Honor the selected output
      // now, while Idle; SDL keeps its default downmix on stereo-only devices.
      if(audioChannels!==undefined)configureAudioDestination(module,audioChannels);
      if(typeof module?._PartyBoardBrowserPipelineCount!=='function')throw Error('Browser pipeline counters required.');
      // Submit a renderer frame before inspecting pending/discovered pipelines.
      // This discovers no future gameplay scenes and is not full shader readiness.
      await call(module,'PartyBoardBrowserAdvancePreparedRenderer',signal);
      if(engine.phase!=='Idle'||engine.preparedCheckpoint()!==checkpoint)throw Error('Renderer changed prepared simulation.');
      return drainIdleEnginePipelines(engine,{signal,maxSteps,progress,
        read:()=>({queued:module._PartyBoardBrowserPipelineCount(0),created:module._PartyBoardBrowserPipelineCount(1)}),
        advance:()=>call(module,'PartyBoardBrowserAdvancePreparedRenderer',signal)});
    },
    async shutdown(module){
      // Resolution validation can fail before platform allocation begins.
      // Once preparation was invoked, retain cleanup even if it rejected.
      if(!platformAttempts.has(module))return;
      await call(module,'PartyBoardBrowserShutdownPlatform');
      platformAttempts.delete(module);
    },
  };
}
