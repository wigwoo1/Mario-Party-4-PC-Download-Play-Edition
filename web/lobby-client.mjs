// Construct per authenticated host connection. Asset identity comes from the
// accepted manifest, not from a status message. This never starts the game.
export class LobbyClient {
  #asset; #total; #revision=-1;
  constructor(assetId,total) {
    if(!/^[a-f0-9]{64}$/.test(assetId)||!Number.isSafeInteger(total)||total<=0||total>2*1024**3) throw TypeError('Invalid asset');
    this.#asset=assetId;this.#total=total;
  }
  receive(data) {
    if(typeof data!=='string'||data.length>16384)throw Error('Invalid lobby snapshot');
    const message=JSON.parse(data),s=message?.snapshot;
    if(message?.type!=='lobby-snapshot'||message.version!==1||!s||s.assetId!==this.#asset||s.total!==this.#total||
      !Number.isSafeInteger(s.revision)||s.revision<0||!Number.isSafeInteger(s.membership)||s.membership<0||s.membership>s.revision||!Array.isArray(s.players)||s.players.length>4)throw Error('Invalid lobby snapshot');
    const peers=new Set(),slots=new Set();
    const saveRequired=s.saveRequired??false,saveHash=s.saveHash??null;
    if(typeof saveRequired!=='boolean'||(saveHash!==null&&(typeof saveHash!=='string'||!/^[a-f0-9]{64}$/.test(saveHash))))throw Error('Invalid save snapshot');
    const players=s.players.map(p=>{
      if(!p||typeof p.peer!=='string'||!p.peer||p.peer.length>128||peers.has(p.peer)||
        typeof p.name!=='string'||!p.name.trim()||p.name.length>40||!Number.isInteger(p.slot)||p.slot<0||p.slot>3||slots.has(p.slot)||
        !Number.isSafeInteger(p.sequence)||p.sequence<0||!Number.isSafeInteger(p.bytes)||p.bytes<0||p.bytes>this.#total||
        ![p.verified,p.shaders,p.ready].every(v=>typeof v==='boolean')||
        (p.verified&&p.bytes!==this.#total)||(p.shaders&&!p.verified)||(p.ready&&!p.shaders))throw Error('Invalid lobby player');
      peers.add(p.peer);slots.add(p.slot);
      if(p.saveHash!=null&&(typeof p.saveHash!=='string'||!/^[a-f0-9]{64}$/.test(p.saveHash)))throw Error('Invalid save acknowledgment');
      const ready=p.ready&&(!saveRequired||(saveHash!==null&&p.saveHash===saveHash));
      return {peer:p.peer,name:p.name,slot:p.slot,sequence:p.sequence,bytes:p.bytes,verified:p.verified,shaders:p.shaders,ready,saveHash:p.saveHash??null,
        progress:p.bytes/this.#total,phase:ready?'Ready':p.verified&&saveRequired&&(!saveHash||p.saveHash!==saveHash)?'Awaiting host save':p.shaders?'Waiting for ready':p.verified?'Awaiting shader preparation':p.bytes===this.#total?'Verifying assets':'Downloading assets'};
    }).sort((a,b)=>a.slot-b.slot);
    if(s.revision<=this.#revision)return null;
    this.#revision=s.revision;
    return {revision:s.revision,membership:s.membership,assetId:this.#asset,total:this.#total,players,saveRequired,saveHash,
      canStart:players.length>=2&&players.every(p=>p.ready&&p.verified&&p.shaders)};
  }
}
