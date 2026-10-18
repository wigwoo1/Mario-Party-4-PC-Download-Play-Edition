// Owns only input channels. Peer identities must be authenticated by the lobby.
// Attaching a replacement never implies recovered state or permission to resume.
export class InputChannels {
  #session; #now; #received; #error; #changed; #bindings=new Map(); #closed=false;
  #attaching=new Set();
  #attachingChannels=new Set();
  constructor(session,{now=()=>performance.now(),received=()=>{},error=()=>{},changed=()=>{}}={}) {
    this.#session=session;this.#now=now;this.#received=received;this.#error=error;this.#changed=changed;
  }
  ready(peer){return !this.#closed&&this.#bindings.get(peer)?.channel.readyState==='open';}
  attach(peer,channel) {
    if(this.#attaching.has(peer))throw Error('Input channel attachment already in progress');
    if(this.#attachingChannels.has(channel))throw Error('Input channel attachment already in progress');
    this.#attaching.add(peer);
    this.#attachingChannels.add(channel);
    try { return this.#attach(peer,channel); }
    finally { this.#attaching.delete(peer);this.#attachingChannels.delete(channel); }
  }
  #attach(peer,channel) {
    if(this.#closed||!Number.isInteger(peer)||peer<=0||peer>0xffffffff||!this.#session.hasPeer(peer)||
        !['open','connecting'].includes(channel.readyState)||channel.ordered===false||
        channel.maxRetransmits!=null||channel.maxPacketLifeTime!=null)throw Error('Invalid input channel');
    if([...this.#bindings].some(([owner,binding])=>owner!==peer&&binding.channel===channel))throw Error('Input channel already assigned');
    const old=this.#bindings.get(peer);
    if(old?.channel===channel)return;
    if(old){old.detach();this.#bindings.delete(peer);this.#session.disconnected(peer,this.#now());old.channel.close();this.#changed(peer,'Replaced');}
    if(this.#closed){channel.close();throw Error('Input transport closed during replacement');}
    channel.binaryType='arraybuffer';
    const active=()=>!this.#closed&&this.#bindings.get(peer)===binding;
    const lost=()=>{
      if(!active())return;
      binding.detach();this.#bindings.delete(peer);this.#session.disconnected(peer,this.#now());this.#changed(peer,'Disconnected');
    };
    const message=event=>{
      if(!active())return;
      const result=event.data instanceof ArrayBuffer?this.#session.receive(peer,new Uint8Array(event.data),this.#now()):'Malformed';
      if(result==='Accepted'||result==='Duplicate'||result==='Stale'){this.#received(peer,result);return;}
      this.#session.cancel();lost();channel.close();this.#error(peer,Error('Input rejected: '+result));
    };
    const failed=()=>{lost();channel.close();};
    const opened=()=>{if(active()&&channel.readyState==='open')this.#changed(peer,'Open');};
    const binding={channel,detach(){channel.removeEventListener('message',message);channel.removeEventListener('close',lost);channel.removeEventListener('error',failed);channel.removeEventListener('open',opened);}};
    this.#bindings.set(peer,binding);
    channel.addEventListener('message',message);channel.addEventListener('close',lost);channel.addEventListener('error',failed);channel.addEventListener('open',opened);
  }
  close() {
    if(this.#closed)return;this.#closed=true;
    for(const binding of this.#bindings.values()){binding.detach();binding.channel.close();}
    this.#bindings.clear();this.#session.cancel();this.#changed(null,'Closed');
  }
}
