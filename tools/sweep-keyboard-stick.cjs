'use strict';
// Cover a rectangular stick-driven interaction with real key transitions.
// Lottery TicketUpdate moves eight screen units per frame and clamps to a
// 192 by 96 rectangle. Sweep every eight-unit row rather than repeat a circle.
async function sweepKeyboardStick({hold,waitFrames}) {
 let held=null,operationFailure;
 async function move(key,frames){
  if(held!==key){
   if(held){await hold(held,false);held=null;}
   held=key;await hold(key,true);
  }
  await waitFrames(frames);
 }
 try{
  await move('a',24);await move('w',12);
  for(let row=0;row<=12;row++){
   await move(row%2?'a':'d',24);
   if(row<12)await move('s',1);
  }
 }catch(error){operationFailure=error;}
 finally{
  try{if(held)await hold(held,false);}
  catch(error){if(operationFailure)throw new AggregateError([operationFailure,error],'Sweep and release failed');throw error;}
  if(operationFailure)throw operationFailure;
 }
}
module.exports={sweepKeyboardStick};
