// Two sweeps cover a one-poll gap even when the host meets an ahead peer last:
// the second sweep brings earlier lagging peers to the host's final poll.
// Each route must perform authenticated reconciliation; no resume occurs here.
export async function alignRecoveryRoutes({peers,reconcile,current}){
 if(!Array.isArray(peers)||peers.length<1||peers.length>3||new Set(peers).size!==peers.length||
    !peers.every(peer=>typeof peer==='string'&&peer)||typeof reconcile!=='function'||typeof current!=='function')
  throw Error('Assigned recovery routes required.');
 for(let sweep=0;sweep<2;sweep++)for(const peer of peers){
  if(current()!==true)throw Error('Recovery route alignment became stale.');
  await reconcile(peer,sweep);
  if(current()!==true)throw Error('Recovery route alignment became stale.');
 }
}
