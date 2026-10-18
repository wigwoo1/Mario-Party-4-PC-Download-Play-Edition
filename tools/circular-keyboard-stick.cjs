'use strict';
// Real keyboard transitions, with no neutral gap between adjacent stick angles.
// The caller waits on actual simulation frames and owns operation deadlines.
async function circularKeyboardStick({hold,waitFrames,cycles=4}) {
 const directions=[['w'],['w','d'],['d'],['d','s'],['s'],['s','a'],['a'],['a','w']];
 const held=new Set();
 let operationFailure;
 try {
  for(let cycle=0;cycle<cycles;cycle++)for(const keys of directions){
   for(const key of keys)if(!held.has(key)){held.add(key);await hold(key,true);}
   for(const key of [...held])if(!keys.includes(key)){await hold(key,false);held.delete(key);}
   await waitFrames(2);
  }
 } catch(error) {
  operationFailure=error;
 } finally {
  // Attempt every release even when one fails; never leave another direction held.
  const releases=await Promise.allSettled([...held].map(key=>hold(key,false)));
  const failures=releases.filter(result=>result.status==='rejected').map(result=>result.reason);
  if(operationFailure&&failures.length)throw new AggregateError([operationFailure,...failures],'Stick operation and key release failed');
  if(operationFailure)throw operationFailure;
  if(failures.length)throw new AggregateError(failures,'Stick key release failed');
 }
}
module.exports={circularKeyboardStick};
