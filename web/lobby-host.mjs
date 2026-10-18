// Lobby signaling only, independent of game assets. Caller assigns each peer
// identity after establishing the intended connection; message bodies cannot.
export class LobbyHost {
  #state; #channels=new Map(); #listeners=new Map(); #changed; #error;
  #closed=false; #closing=false; #failedCloses=new Set();
  #frozen=false;
  constructor(state,{changed=()=>{},error=()=>{}}={}) {
    this.#state=state; this.#changed=changed; this.#error=error;
  }
  broadcast() {
    if(this.#closed)return;
    const snapshot=this.#state.snapshot();
    const message=JSON.stringify({type:'lobby-snapshot',version:1,snapshot});
    for(const [peer,channel] of [...this.#channels]) {
      if(channel.readyState!=='open') continue;
      try { channel.send(message); }
      catch(error) { this.detach(peer); this.#error(peer,error); return; }
    }
    this.#changed(snapshot);
  }
  attach(peer,name,channel) {
    if(this.#closed||this.#frozen||this.#channels.has(peer)||!['connecting','open'].includes(channel.readyState)) throw Error('Unavailable peer channel');
    this.#state.join(peer,name); this.#channels.set(peer,channel);
    const open=()=>{if(this.#channels.get(peer)===channel)this.broadcast();};
    const close=()=>{if(this.#channels.get(peer)===channel)this.detach(peer);};
    const message=event=>{
      if(this.#channels.get(peer)!==channel)return;
      try {
        if(typeof event.data!=='string'||event.data.length>2048) throw Error('Invalid lobby message');
        const message=JSON.parse(event.data);
        if(message.type!=='lobby-report'||message.version!==1||!message.report) throw Error('Invalid lobby report');
        this.#state.report(peer,message.report);
        this.broadcast();
      } catch(error) { this.detach(peer); this.#error(peer,error); }
    };
    this.#listeners.set(peer,()=>{
      channel.removeEventListener('open',open);
      channel.removeEventListener('close',close);
      channel.removeEventListener('message',message);
    });
    channel.addEventListener('open',open);
    channel.addEventListener('close',close);
    channel.addEventListener('message',message);
    this.broadcast();
  }
  setSaveSnapshot(hash) {
    if(this.#closed)throw Error('Lobby closed.');
    if(this.#frozen)throw Error('Lobby reports unavailable.');
    this.#state.setSaveSnapshot(hash);
    this.broadcast();
  }
  reportLocal(peer,report) {if(this.#closed)throw Error('Lobby closed.');if(this.#frozen)throw Error('Lobby reports unavailable.');this.#state.report(peer,report);this.broadcast();}
  detach(peer) {
    if(this.#frozen&&!this.#closed)throw Error('Running membership requires owned session teardown.');
    const channel=this.#channels.get(peer); if(!channel)return;
    this.#channels.delete(peer);
    this.#listeners.get(peer)?.();this.#listeners.delete(peer);
    this.#state.leave(peer);
    try {channel.close();}
    catch(error){this.#failedCloses.add(channel);throw error;}
    finally {this.broadcast();}
  }
  freezeMembership() {
    if(this.#closed)throw Error('Lobby closed.');
    this.#frozen=true;
    // Running-session recovery owns loss. A signaling-channel close must no
    // longer delete a player or replace the agreed input ownership revision.
    for(const remove of this.#listeners.values())remove();
    this.#listeners.clear();
    return this.#state.snapshot();
  }
  close() {
    if(this.#closing)return;
    this.#closed=true;this.#closing=true;
    const retry=[...this.#failedCloses],failures=[];
    try {
      for(const peer of [...this.#channels.keys()]) {
        try {this.detach(peer);}catch(error){failures.push(error);}
      }
      for(const channel of retry) {
        try {channel.close();this.#failedCloses.delete(channel);}catch(error){failures.push(error);}
      }
    } finally {this.#closing=false;}
    if(failures.length)throw new AggregateError(failures,'Lobby channel cleanup failed.');
  }
}
