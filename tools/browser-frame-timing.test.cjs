const test=require('node:test'),assert=require('node:assert/strict');
const {frameStages,summarizeFrameTiming}=require('./browser-frame-timing.cjs');
const snapshot=(frames,value=0)=>({completedFrames:frames,...Object.fromEntries(frameStages.map(stage=>[stage,value]))});
test('interval subtraction separates input, drawing and intentional pacing waits',()=>{
 const before=snapshot(120,100),after=snapshot(240,100);
 after.inputMs+=960;after.drawMs+=480;after.pacingMs+=720;
 const result=summarizeFrameTiming(before,after);
 assert.equal(result.completedFrames,120);assert.equal(result.totalMs,2160);assert.equal(result.averageFrameMs,18);
 assert.equal(result.stages.inputMs.averageMs,8);assert.equal(result.stages.drawMs.averageMs,4);assert.equal(result.stages.pacingMs.averageMs,6);
 assert.equal(result.largestStage,'inputMs');assert.equal(result.measuredFramesPerSecond,1000/18);
 assert.equal(before.inputMs,100);assert.equal(after.inputMs,1060);
});
test('idle, reset, malformed and zero-duration measurements cannot become FPS claims',()=>{
 assert.throws(()=>summarizeFrameTiming(snapshot(120),snapshot(120)),/advancing/);
 assert.throws(()=>summarizeFrameTiming(snapshot(120),snapshot(0)),/advancing/);
 assert.throws(()=>summarizeFrameTiming(snapshot(120,100),snapshot(240,99)),/reset/);
 assert.throws(()=>summarizeFrameTiming(snapshot(120),snapshot(240)),/Positive/);
 for(const value of [NaN,Infinity,-1])assert.throws(()=>summarizeFrameTiming(snapshot(120),{...snapshot(240),drawMs:value}),/Invalid/);
 assert.throws(()=>summarizeFrameTiming(null,snapshot(240)),/counter/);
 assert.throws(()=>summarizeFrameTiming(snapshot(120),{completedFrames:240}),/Invalid/);
});
