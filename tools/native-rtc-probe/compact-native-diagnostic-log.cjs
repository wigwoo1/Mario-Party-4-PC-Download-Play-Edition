'use strict';
// Full observations still reach the live driver and explicit evidence windows.
// Only their repetitive stdout representation is compacted.
function compactNativeDiagnosticLine(line){
 for(const prefix of ['Native frontend event: ','Native external Join event: ']){
  const index=line.indexOf(prefix);if(index<0)continue;
  let event;try{event=JSON.parse(line.slice(index+prefix.length));}catch{return line;}
  if(event.type!=='full-match-observation')return line;
  if(!Number.isSafeInteger(event.frame)||!Array.isArray(event.observations))return line;
  const rows=event.observations;
  return line.slice(0,index)+prefix+JSON.stringify({type:'full-match-observation-summary',requestId:event.requestId,frame:event.frame,observationCount:rows.length,firstFrame:rows[0]?.frame,lastFrame:rows.at(-1)?.frame,fullObservationDeliveredToDriver:true});
 }
 return line;
}
module.exports={compactNativeDiagnosticLine};
