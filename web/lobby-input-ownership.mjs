// Numeric lockstep owners derive from global slots, never guest invitation IDs.
// Freeze a membership snapshot before starting; a replacement invalidates it.
export function lobbyInputOwnership(snapshot,peer){
  if(!snapshot||!Number.isSafeInteger(snapshot.membership)||snapshot.membership<0||
    !Array.isArray(snapshot.players)||snapshot.players.length<1||snapshot.players.length>4)
    throw Error('Current lobby membership required.');
  const owners=[0,0,0,0],peers=new Set(),bindings=[];let local;
  for(const player of snapshot.players){
    if(typeof player.peer!=='string'||!player.peer||peers.has(player.peer)||
      !Number.isInteger(player.slot)||player.slot<0||player.slot>3||owners[player.slot])
      throw Error('Unique lobby peers and slots required.');
    peers.add(player.peer);owners[player.slot]=player.slot+1;
    bindings.push(Object.freeze({peer:player.peer,slot:player.slot,owner:player.slot+1}));
    if(player.peer===peer)local=player.slot;
  }
  if(local===undefined)throw Error('Local peer is not in this lobby.');
  return Object.freeze({membership:snapshot.membership,peer,owner:local+1,
    owners:Object.freeze(owners),localPadSlots:Object.freeze(owners.map((_,slot)=>slot===local?0:null)),
    bindings:Object.freeze(bindings.sort((a,b)=>a.slot-b.slot))});
}
export function requireCurrentInputOwnership(plan,snapshot){
  const current=lobbyInputOwnership(snapshot,plan.peer);
  if(JSON.stringify(current)!==JSON.stringify(plan))throw Error('Input ownership membership changed.');
  return plan;
}
