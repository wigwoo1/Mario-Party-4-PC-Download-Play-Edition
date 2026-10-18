import {host} from './transfer.mjs';
import {receiveSaveTransfer} from './save-transfer.mjs';

// Dedicated ordered channel. Expected context is supplied by the owning lobby.
// Handshake installs the receiver before the host emits any transfer packets.
export function sendSaveChannel(channel, prepared, callbacks={}) {
  const fail=error=>{channel.close();callbacks.error?.(error);};
  const ready=event=>{
    channel.removeEventListener('message',ready);
    try {
      if(typeof event.data!=='string'||event.data.length>256)throw Error('Invalid save acceptance');
      const message=JSON.parse(event.data);
      if(message.type!=='save-accept'||message.transferId!==prepared.offer.transferId)
        throw Error('Invalid save acceptance');
      host(channel,prepared.asset,fail,callbacks.complete,callbacks.progress);
    }catch(error){fail(error);}
  };
  channel.addEventListener('message',ready);
  const offer=()=>channel.send(JSON.stringify({type:'save-offer',offer:prepared.offer}));
  if(channel.readyState==='open')offer();else channel.addEventListener('open',offer,{once:true});
  channel.addEventListener('close',()=>{channel.removeEventListener('message',ready);channel.removeEventListener('open',offer);},{once:true});
}

export function receiveSaveChannel(channel,context,callbacks={}) {
  let receiver,stopped=false;
  const first=event=>{
    channel.removeEventListener('message',first);
    try {
      if(typeof event.data!=='string'||event.data.length>2048)throw Error('Invalid save offer');
      const message=JSON.parse(event.data);
      if(message.type!=='save-offer')throw Error('Expected save offer');
      receiver=receiveSaveTransfer(channel,message.offer,context,callbacks);
      channel.send(JSON.stringify({type:'save-accept',transferId:message.offer.transferId}));
    }catch(error){channel.close();callbacks.error?.(error);}
  };
  channel.addEventListener('message',first);
  channel.addEventListener('close',()=>{stopped=true;channel.removeEventListener('message',first);},{once:true});
  return {cancel(){if(stopped)return;stopped=true;channel.removeEventListener('message',first);receiver?.cancel();channel.close();}};
}
