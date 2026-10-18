// Simulation observations prove overlay execution only, never full GX coverage.
function validatePipelineSceneVisits(visits,{minigameRun=false,selectedMinigame}={}){
 if(!visits||typeof visits!=='object'||!Object.keys(visits).length)throw Error('Pipeline capture requires actual simulation scene visits.');
 for(const [key,visit] of Object.entries(visits)){
  if(!visit||String(visit.overlay)!==key||!Number.isInteger(visit.overlay)||visit.overlay<0||
   !Number.isInteger(visit.samples)||visit.samples<1||!Number.isInteger(visit.firstFrame)||visit.firstFrame<0||
   !Number.isInteger(visit.lastFrame)||visit.lastFrame<visit.firstFrame)
   throw Error('Invalid simulation scene visit.');
 }
 if(minigameRun&&(!Number.isInteger(selectedMinigame?.overlay)||!visits[selectedMinigame.overlay]))
  throw Error('Selected minigame was not observed executing.');
 return true;
}
module.exports={validatePipelineSceneVisits};
