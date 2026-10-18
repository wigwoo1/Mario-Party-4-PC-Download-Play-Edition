import {decodeInput} from './input-packet.mjs';
// Host-routed peer input. Each guest channel is bound to a lobby identity.
// Lockstep still validates frame windows/checkpoints; this layer never advances.
export function createLobbyInputRouter({plan,session,host=false,channels,receive,disconnected=()=>{},error=()=>{}}){
 if(!(session instanceof Uint8Array)||session.length!==16||typeof receive!=='function'||!(channels instanceof Map))throw Error('Input routing configuration required.');
 const bindings=new Map(plan.bindings.map(binding=>[binding.peer,binding]));let closed=false;
 channels=new Map(channels);
 const listeners=new Map();
 const reportedLoss=new Set();
 const reportLoss=(peer,reason,cause)=>{
  if(closed||reportedLoss.has(peer))return;
  reportedLoss.add(peer);
  const owners=host?[bindings.get(peer).owner]:plan.bindings.filter(binding=>binding.peer!==plan.peer).map(binding=>binding.owner);
  const channel=channels.get(peer);
  disconnected(owners,{peer,reason,readyState:channel.readyState,bufferedAmount:channel.bufferedAmount??null,
   error:cause?{name:cause.name,message:cause.message}:null});
 };
 const checkChannels=()=>{for(const [peer,channel] of channels){if(channel.readyState==='open')reportedLoss.delete(peer);else reportLoss(peer,'channel-unavailable');}};
 const validate=(bytes,peer)=>{
  const packet=decodeInput(bytes),binding=bindings.get(peer);
  if(!packet||!binding||packet.slot!==binding.slot||!packet.session.every((b,i)=>b===session[i]))throw Error('Unauthorized input packet.');
  return binding.owner;
 };
 const broadcast=(bytes,except)=>{checkChannels();for(const [peer,channel] of channels){if(peer===except||channel.readyState!=='open')continue;try{channel.send(bytes.slice());}catch(cause){reportLoss(peer,'send-failed',cause);}}};
 const attach=(peer,channel)=>{
  if(host&&!bindings.has(peer))throw Error('Input channel peer unassigned.');
  channel.binaryType='arraybuffer';
  const message=event=>{if(closed||channels.get(peer)!==channel)return;try{
   if(!(event.data instanceof ArrayBuffer))throw Error('Binary input packet required.');
   const bytes=new Uint8Array(event.data);let owner;
   if(host)owner=validate(bytes,peer);
   else {const packet=decodeInput(bytes),binding=packet&&plan.bindings.find(b=>b.slot===packet.slot);if(!binding||binding.peer===plan.peer)throw Error('Invalid relayed owner.');owner=validate(bytes,binding.peer);}
   checkChannels();receive(owner,bytes.slice());if(host)broadcast(bytes,peer);
  }catch(cause){error(peer,cause);}};
  channel.addEventListener('message',message);
  const lost=()=>{if(closed||channels.get(peer)!==channel)return;try{
   // A guest receives every remote lane through its host channel. Losing that
   // channel makes all remote owners unavailable, not only the host's PAD lane.
   reportLoss(peer,'channel-close');
  }catch(cause){error(peer,cause);}};
  channel.addEventListener('close',lost);
  listeners.set(peer,()=>{channel.removeEventListener('message',message);channel.removeEventListener('close',lost);});
 };
 for(const [peer,channel] of channels)attach(peer,channel);
 return {send(bytes){if(closed)throw Error('Input router closed.');validate(bytes,plan.peer);broadcast(bytes);},
  replace(peer,channel){
   if(closed||!channels.has(peer)||channel?.readyState!=='open'||channel.ordered!==true||channel.maxRetransmits!=null||channel.maxPacketLifeTime!=null)
    throw Error('Assigned peer and reliable open replacement input channel required.');
   if([...channels].some(([other,current])=>other!==peer&&current===channel))throw Error('Replacement channel already assigned.');
   const previous=channels.get(peer);if(previous===channel)return previous;
   listeners.get(peer)();channels.set(peer,channel);attach(peer,channel);reportedLoss.delete(peer);return previous;
  },
  close(){if(closed)return;closed=true;for(const remove of listeners.values())remove();listeners.clear();}};
}
