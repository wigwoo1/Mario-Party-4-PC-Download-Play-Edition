// Host-owned state only. Transport must bind reports to an authenticated peer.
// Asset identity is the transfer manifest hash; no game assets are needed here.
export class LobbyState {
  #players=new Map(); #revision=0; #membership=0; #saveRequired; #saveHash=null;
  constructor(assetId,total,{requireSave=false}={}) {
    if(typeof requireSave!=='boolean')throw TypeError('Invalid save policy');
    this.#saveRequired=requireSave;
    if(!/^[a-f0-9]{64}$/.test(assetId)||!Number.isSafeInteger(total)||total<=0||total>2*1024**3) throw TypeError('Invalid asset');
    Object.defineProperties(this,{assetId:{value:assetId},total:{value:total}});
  }
  setSaveSnapshot(hash) {
    if(!this.#saveRequired || typeof hash!=='string' || !/^[a-f0-9]{64}$/.test(hash))throw Error('Invalid save snapshot');
    if(hash===this.#saveHash)return;
    this.#saveHash=hash;
    for(const p of this.#players.values()){p.ready=false;p.saveHash=null;}
    this.#revision++;
  }
  join(peer,name) {
    if(typeof peer!=='string'||!peer||peer.length>128||typeof name!=='string'||!name.trim()||name.length>40) throw TypeError('Invalid player');
    if(this.#players.has(peer)||this.#players.size===4) throw Error('Slot unavailable');
    const used=new Set([...this.#players.values()].map(p=>p.slot));
    const slot=[0,1,2,3].find(s=>!used.has(s));
    for(const p of this.#players.values()) p.ready=false;
    this.#players.set(peer,{peer,name,slot,sequence:0,bytes:0,verified:false,shaders:false,ready:false,saveHash:null});
    this.#membership++;this.#revision++; return slot;
  }
  leave(peer) {
    if(this.#players.delete(peer)) {
      // Membership changes invalidate the entire ready barrier.
      for(const p of this.#players.values()) p.ready=false;
      this.#membership++;this.#revision++;
    }
  }
  report(peer,{assetId,sequence,bytes,verified,shaders,ready,membership,saveHash=null}) {
    const p=this.#players.get(peer);
    if(!p||assetId!==this.assetId||!Number.isSafeInteger(sequence)||sequence<=p.sequence||
      !Number.isSafeInteger(bytes)||bytes<p.bytes||bytes>this.total||
      ![verified,shaders,ready].every(v=>typeof v==='boolean')||
      (verified&&bytes!==this.total)||(shaders&&!verified)||(ready&&!shaders)||
      (ready&&(!Number.isSafeInteger(membership)||membership<0||membership>this.#membership))) throw Error('Invalid player status');
    if(saveHash!==null&&(typeof saveHash!=='string'||!/^[a-f0-9]{64}$/.test(saveHash)))throw Error('Invalid save acknowledgment');
    if(this.#saveRequired && (!this.#saveHash || saveHash!==this.#saveHash))ready=false;
    // A legitimate report may cross a membership update in flight. Keep its
    // progress, but require a new ready action for the current player lineup.
    if(ready&&membership!==this.#membership)ready=false;
    Object.assign(p,{sequence,bytes,verified,shaders,ready,saveHash}); this.#revision++;
  }
  snapshot() {
    const players=[...this.#players.values()].sort((a,b)=>a.slot-b.slot).map(p=>({...p,
      progress:p.bytes/this.total,
      phase:p.ready?'Ready':p.verified&&this.#saveRequired&&(!this.#saveHash||p.saveHash!==this.#saveHash)?'Awaiting host save':p.shaders?'Waiting for ready':p.verified?'Awaiting shader preparation':p.bytes===this.total?'Verifying assets':'Downloading assets'}));
    return {revision:this.#revision,membership:this.#membership,assetId:this.assetId,total:this.total,players,saveRequired:this.#saveRequired,saveHash:this.#saveHash,
      canStart:players.length>=2&&players.every(p=>p.ready&&p.verified&&p.shaders)};
  }
}
