'use strict';
// Test-driver PRNG only: never consume the game's synchronized RNG.
async function variedKeyboardStick({hold,waitFrames,shouldContinue=async()=>true}) {
 const directions=[['a'],['d'],['s'],['w'],['a','s'],['d','s'],['a','w'],['d','w']];
 const held=new Set();let seed=12345,failure;
 try{
  for(let step=0;step<512;step++){
   if(!await shouldContinue())break;
   seed=(Math.imul(seed,1664525)+1013904223)>>>0;
   const next=new Set(directions[seed>>>29]);
   for(const key of held)if(!next.has(key)){await hold(key,false);held.delete(key);}
   for(const key of next)if(!held.has(key)){held.add(key);await hold(key,true);}
   await waitFrames(4);
  }
 }catch(error){failure=error;}
 const released=await Promise.allSettled([...held].map(key=>hold(key,false)));
 const errors=released.filter(result=>result.status==='rejected').map(result=>result.reason);
 if(failure&&errors.length)throw new AggregateError([failure,...errors],'Varied stick and release failed');
 if(failure)throw failure;
 if(errors.length)throw new AggregateError(errors,'Varied stick release failed');
}
module.exports={variedKeyboardStick};
