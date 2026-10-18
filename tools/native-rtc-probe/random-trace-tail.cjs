'use strict';
// Each peer retains its own tail so a noisier browser cannot evict native evidence.
function createRandomTraceTail(capacity=32768){
 if(!Number.isSafeInteger(capacity)||capacity<1)throw Error('Invalid RNG trace capacity');
 const peers=new Map(), targetPeers=new Map();
 function retain(map,entry){
  let state=map.get(entry.peer);
  if(!state){state={entries:new Array(capacity),count:0};map.set(entry.peer,state);}
  state.entries[state.count%capacity]={...entry};state.count++;
 }
 function snapshot(map){
  return [...map].map(([peer,state])=>({peer,observed:state.count,dropped:Math.max(0,state.count-capacity),
   entries:Array.from({length:Math.min(state.count,capacity)},(_,i)=>state.entries[(Math.max(0,state.count-capacity)+i)%capacity])}));
 }
 return {
  add(entry){
   if(!/^RNG (call|hook) /.test(String(entry.message)))return false;
   retain(peers,entry);
   // Results and unrelated overlays cannot evict the latest target occurrence.
   if(/^RNG hook .*source=m440-player /.test(String(entry.message)))retain(targetPeers,entry);
   return true;
  },
  snapshot(){
   return snapshot(peers);
  },
  targetSnapshot(){return snapshot(targetPeers);}
 };
}
module.exports={createRandomTraceTail};

