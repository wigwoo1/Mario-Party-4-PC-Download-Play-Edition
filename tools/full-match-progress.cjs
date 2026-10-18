'use strict';
// Diagnostic gate over actual read-only game observations. It cannot change
// turns, select overlays, or declare rendered rankings without separate evidence.
function createFullMatchProgress({board,maxTurn,awardOverlay}){
 if(!Number.isInteger(board)||board<0||!Number.isInteger(maxTurn)||maxTurn<5||maxTurn>50||maxTurn%5!==0||
    !Number.isInteger(awardOverlay)||awardOverlay<0)throw Error('Ordinary match configuration required.');
 const turns=new Set();let awards=false,rankings=false,lastTurn=0;
 return{observe(progress){
  if(!progress||!['board','maxTurn','turn','overlay','event'].every(key=>Number.isInteger(progress[key]))||progress.event<0)
   throw Error('Actual game progress and overlay event required.');
  if(progress.board!==board||progress.maxTurn!==maxTurn||progress.turn<1||progress.turn>maxTurn||progress.turn<lastTurn)
   throw Error('Ordinary match configuration or progression changed.');
  lastTurn=progress.turn;turns.add(progress.turn);
  if(progress.turn===maxTurn&&progress.overlay===awardOverlay){
   if(progress.event===0)awards=true;
   if(progress.event===1&&awards)rankings=true;
  }
  return turns.size===maxTurn&&awards&&rankings;
 },snapshot(){return{board,maxTurn,observedTurns:[...turns].sort((a,b)=>a-b),awards,rankings,
  completeProgress:turns.size===maxTurn&&awards&&rankings,renderedRankingsVerified:false};}};
}
module.exports={createFullMatchProgress};
