'use strict';
const frameStages=['eventsMs','inputMs','beginFrameMs','simulationMs','drawMs','submitMs','pacingMs'];
function validate(snapshot){
 if(!snapshot||!Number.isSafeInteger(snapshot.completedFrames)||snapshot.completedFrames<0)throw Error('Completed browser frame counter required');
 for(const stage of frameStages)if(!Number.isFinite(snapshot[stage])||snapshot[stage]<0)throw Error('Invalid cumulative browser timing: '+stage);
}
// Counters come from non-overlapping boundaries in game/main.c. They include
// asynchronous waits and are presentation diagnostics, never simulation input.
function summarizeFrameTiming(previous,next){
 validate(previous);validate(next);
 const frames=next.completedFrames-previous.completedFrames;
 if(frames<=0)throw Error('An advancing frame timing window is required');
 const stages={};let totalMs=0;
 for(const stage of frameStages){
  const elapsedMs=next[stage]-previous[stage];
  if(elapsedMs<0)throw Error('Browser timing counter reset: '+stage);
  stages[stage]={elapsedMs,averageMs:elapsedMs/frames};totalMs+=elapsedMs;
 }
 if(totalMs<=0)throw Error('Positive measured frame time required');
 return {completedFrames:frames,totalMs,averageFrameMs:totalMs/frames,measuredFramesPerSecond:frames*1000/totalMs,
  stages,largestStage:frameStages.reduce((largest,stage)=>stages[stage].elapsedMs>stages[largest].elapsedMs?stage:largest,frameStages[0])};
}
module.exports={frameStages,summarizeFrameTiming};
