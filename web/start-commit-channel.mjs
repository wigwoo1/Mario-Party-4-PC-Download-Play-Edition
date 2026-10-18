import {resumeCommittedClientRuntime} from './start-client.mjs';

// Caller supplies the current authenticated host channel and owns recovery.
// Channel loss disposes this runtime; it does not reconnect or resume it.
export function bindClientStartCommit(channel,prepared,options,{error=()=>{}}={}) {
  if(!channel||!['connecting','open'].includes(channel.readyState)||channel.ordered===false||
      channel.maxRetransmits!=null||channel.maxPacketLifeTime!=null||typeof prepared?.runtime?.dispose!=='function')
    throw Error('Reliable host channel and owned prepared runtime required.');
  let active=true,accepted,execution,cleanup,transferred=false,channelClosed=false,runtimeReleased=false;
  const detach=()=>{
    channel.removeEventListener('message',message);channel.removeEventListener('close',lost);channel.removeEventListener('error',lost);
  };
  const close=()=>{
    if(transferred)return Promise.resolve();
    if(cleanup)return cleanup;
    active=false;detach();
    // Publish ownership before invoking channel or runtime cleanup callbacks.
    // A failing channel close must not strand the prepared runtime.
    cleanup=Promise.resolve().then(async()=>{
      const failures=[];
      try {if(!channelClosed){channel.close();channelClosed=true;}}catch(cause){failures.push(cause);}
      try {if(!runtimeReleased){await prepared.runtime.dispose();runtimeReleased=true;}}catch(cause){failures.push(cause);}
      if(failures.length===1)throw failures[0];
      if(failures.length)throw new AggregateError(failures,'Start channel and runtime cleanup failed.');
    }).catch(cause=>{cleanup=undefined;throw cause;});
    return cleanup;
  };
  const fail=cause=>{if(!active)return;void close().catch(error);error(cause);};
  const message=event=>{
    if(!active)return;
    try {
      if(typeof event.data!=='string'||event.data.length>2048)throw Error('Invalid host start commit.');
      const commit=JSON.parse(event.data);
      const identity=JSON.stringify(commit);
      if(accepted!==undefined) {
        if(identity!==accepted)throw Error('Conflicting host start commit.');
        return;
      }
      execution=resumeCommittedClientRuntime(commit,prepared,options);
      accepted=identity;
      Promise.resolve(execution).catch(fail);
    } catch(cause){fail(cause);}
  };
  const lost=()=>fail(Error('Host start channel lost.'));
  channel.addEventListener('message',message);channel.addEventListener('close',lost);channel.addEventListener('error',lost);
  return {close,get execution(){return execution;},get committed(){return accepted!==undefined;},
    handoff(){
      if(!active||accepted===undefined||channel.readyState!=='open')
        throw Error('Only a committed live start channel can transfer session ownership.');
      active=false;transferred=true;detach();
      // New owner must monitor channel/execution and own stop/dispose/recovery.
      // This receiver no longer closes or releases any transferred resources.
      return {channel,runtime:prepared.runtime,execution};
    },
  };
}
