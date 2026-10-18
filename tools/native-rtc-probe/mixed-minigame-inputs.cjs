'use strict';
const assert=require('node:assert/strict');
// Normal logical keyboard actions, shared by the native and bound-browser peers.
// Keep edges held for simulation frames, regardless of render/network speed.
async function holdForTwoFrames(peer,keys,waitFrames){
 let failure;
 try{
  const pressed=await Promise.allSettled(keys.map(key=>peer.keyboard.down(key)));
  const rejected=pressed.find(result=>result.status==='rejected');
  if(rejected)throw rejected.reason;
  await waitFrames(peer,2);
 }catch(error){failure=error;}
 const released=await Promise.allSettled(keys.map(key=>peer.keyboard.up(key)));
 if(failure)throw failure;
 const rejected=released.find(result=>result.status==='rejected');if(rejected)throw rejected.reason;
}
function pressMixedMarioMedleyInput(peer,waitFrames){
 return holdForTwoFrames(peer,['z','x','c','v'],waitFrames);
}
function pressMixedM432ObstacleInput(peer,waitFrames,movementKey='w'){
 assert.ok(['w','a','s','d'].includes(movementKey),'m432 steering must be a logical main-stick direction');
 return holdForTwoFrames(peer,['x','c','v',movementKey],waitFrames);
}
module.exports={pressMixedMarioMedleyInput,pressMixedM432ObstacleInput};
