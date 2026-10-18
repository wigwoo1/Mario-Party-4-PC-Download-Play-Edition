import {prepareStartAcknowledgment} from './start-client.mjs';
import {bindClientStartCommit} from './start-commit-channel.mjs';

// Caller supplies the current trusted host channel. This owns preparation and
// commit reception until handoff; it does not establish host authentication.
export function bindClientStartPreparation(channel,options,{error=()=>{}}={}) {
  if(!channel||!['connecting','open'].includes(channel.readyState)||channel.ordered===false||
      channel.maxRetransmits!=null||channel.maxPacketLifeTime!=null)
    throw Error('Reliable host start channel required.');
  const abort=new AbortController();
  const callerSignal=options?.signal;
  let active=true,preparation,proposalIdentity,receiver,closing,transferred=false;
  const detach=()=>{
    channel.removeEventListener('message',message);channel.removeEventListener('close',lost);
    channel.removeEventListener('error',lost);
  };
  const close=()=>{
    if(transferred)return Promise.resolve();
    if(closing)return closing;
    active=false;abort.abort();detach();callerSignal?.removeEventListener('abort',cancel);
    closing=Promise.resolve().then(async()=>{
      // Preparation checks the abort signal and releases any late runtime.
      await preparation?.catch(()=>{});
      if(receiver)await receiver.close();else channel.close();
    }).catch(cause=>{closing=undefined;throw cause;});
    return closing;
  };
  const fail=cause=>{if(!active)return;void close().catch(error);error(cause);};
  const lost=()=>fail(Error('Host lost during start preparation.'));
  const message=event=>{
    if(!active)return;
    let proposal;
    try {
      if(typeof event.data!=='string'||event.data.length>2048)throw Error('Invalid host start proposal.');
      proposal=JSON.parse(event.data);
      const identity=JSON.stringify(proposal);
      if(preparation){
        if(identity!==proposalIdentity)throw Error('Host changed proposal during preparation.');
        return;
      }
      proposalIdentity=identity;
    } catch(cause){fail(cause);return;}
    preparation=Promise.resolve().then(async()=>{
      const prepared=await prepareStartAcknowledgment(proposal,{...options,signal:abort.signal});
      // No await between cancellation check, receiver binding and ack send.
      try {
        abort.signal.throwIfAborted();
        receiver=bindClientStartCommit(channel,prepared,options,{error:fail});
        detach();
        channel.send(JSON.stringify(prepared.acknowledgment));
      } catch(cause) {
        if(receiver)await receiver.close();else await prepared.runtime.dispose();
        throw cause;
      }
    });
    preparation.catch(fail);
  };
  const cancel=()=>{abort.abort(callerSignal.reason);void close().catch(error);};
  channel.addEventListener('message',message);channel.addEventListener('close',lost);channel.addEventListener('error',lost);
  callerSignal?.addEventListener('abort',cancel,{once:true});
  if(callerSignal?.aborted)cancel();
  return {close,get execution(){return receiver?.execution;},get committed(){return receiver?.committed===true;},
    handoff(){
      if(!active||!receiver)throw Error('Prepared committed session required.');
      const session=receiver.handoff();transferred=true;active=false;detach();callerSignal?.removeEventListener('abort',cancel);return session;
    },
  };
}
