// Host-owned start agreement. Transport binds peer identity; this policy does
// not authenticate messages, prepare shaders, seed the engine or run frames.
const uint32=value=>Number.isInteger(value)&&value>=0&&value<=0xffffffff;
const hash=value=>typeof value==='string'&&/^[a-f0-9]{64}$/.test(value);
function lineup(snapshot) {
  if(!snapshot||!hash(snapshot.assetId)||!Number.isSafeInteger(snapshot.total)||snapshot.total<1||snapshot.total>2*1024**3||
    !Number.isSafeInteger(snapshot.revision)||snapshot.revision<0||
    !Number.isSafeInteger(snapshot.membership)||snapshot.membership<0||snapshot.membership>snapshot.revision||
    !Array.isArray(snapshot.players)||snapshot.players.length<2||snapshot.players.length>4||snapshot.saveRequired)
    throw Error('Lobby cannot prepare a session start.');
  const peers=new Set(),slots=new Set();
  for(const player of snapshot.players) {
    if(typeof player.peer!=='string'||!player.peer||player.peer.length>128||peers.has(player.peer)||
      !Number.isInteger(player.slot)||player.slot<0||player.slot>3||slots.has(player.slot)||
      player.ready!==true||player.verified!==true||player.shaders!==true||player.bytes!==snapshot.total)
      throw Error('Every player must finish preparation and be ready.');
    peers.add(player.peer);slots.add(player.slot);
  }
  return snapshot.players.map(({peer,slot})=>({peer,slot})).sort((a,b)=>a.slot-b.slot);
}
export class StartBarrier {
  #proposal; #acks=new Set(); #state='Awaiting';
  constructor(snapshot,{session,seed,buildId,checkpoint}) {
    const players=lineup(snapshot);
    if(!(session instanceof Uint8Array)||session.length!==16||!session.some(Boolean)||
      !uint32(seed)||!hash(buildId)||!uint32(checkpoint))throw Error('Invalid session start identity.');
    this.#proposal={type:'session-start',version:1,
      session:Array.from(session,b=>b.toString(16).padStart(2,'0')).join(''),
      revision:snapshot.revision,membership:snapshot.membership,assetId:snapshot.assetId,total:snapshot.total,
      buildId,seed,checkpoint,profile:'unlocked-no-save-v1',players};
  }
  get proposal(){return structuredClone(this.#proposal);}
  get state(){return this.#state;}
  get pending(){return this.#proposal.players.filter(p=>!this.#acks.has(p.peer)).map(p=>p.peer);}
  update(snapshot) {
    if(this.#state==='Cancelled'||this.#state==='Mismatch')return false;
    try {
      const players=lineup(snapshot);
      if(snapshot.revision!==this.#proposal.revision||snapshot.membership!==this.#proposal.membership||
        snapshot.assetId!==this.#proposal.assetId||snapshot.total!==this.#proposal.total||JSON.stringify(players)!==JSON.stringify(this.#proposal.players))
        throw Error('Lobby changed.');
    } catch {this.cancel();return false;}
    return true;
  }
  acknowledge(peer,message,snapshot) {
    if(!this.update(snapshot)||!this.#proposal.players.some(p=>p.peer===peer))return false;
    if(!message||message.type!=='session-start-ack'||message.version!==1)return false;
    for(const key of ['session','revision','membership','assetId','total','buildId','seed','profile'])
      if(message[key]!==this.#proposal[key])return false;
    if(JSON.stringify(message.players)!==JSON.stringify(this.#proposal.players))return false;
    // A correctly addressed acknowledgment with different initialized state
    // blocks everyone, rather than accepting a partial agreement.
    if(!uint32(message.checkpoint)||message.checkpoint!==this.#proposal.checkpoint) {
      this.#state='Mismatch';return false;
    }
    this.#acks.add(peer);
    if(this.pending.length===0)this.#state='Agreed';
    return true;
  }
  cancel(){this.#state='Cancelled';this.#acks.clear();}
}
