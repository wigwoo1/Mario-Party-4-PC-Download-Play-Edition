'use strict';
const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),os=require('node:os'),path=require('node:path');
const {driveMixedPostRanking,pressMixedRankingAction}=require('./drive-mixed-post-ranking.cjs');
const {matchingRecords}=require('./drive-mixed-full-match.cjs');
const inventory={overlays:[{name:'mstory3dll',overlay:78},{name:'modeseldll',overlay:74}]};
function fixture(phase='details'){
 const world={phase,frame:1000,ready:false,ignoreSkip:false,ignoreIntro:false,paused:false,keys:[],waits:[],captures:[],badCheckpoint:false};
 const peers=Array.from({length:4},(_,slot)=>({slot,observe:async()=>({frame:world.frame+=100,progress:{overlay:world.phase==='menu'?74:78,event:1},input:{state:world.paused?'Paused':'Running'},observations:Array.from({length:64},(_,frame)=>({frame:String(frame),checkpoint:world.badCheckpoint&&slot===2?2:1,frand:3,rand8:4,pads:[]}))}),modeEntryCount:()=>1+Number(world.ready),capture:async filename=>{world.captures.push(filename);return {path:filename};},keyboard:{down:async key=>world.keys.push({slot,key,down:true}),up:async key=>{
  world.keys.push({slot,key,down:false});if(key==='x'&&world.phase==='details')world.phase='summary';
  if(key==='Enter'&&world.phase==='summary'&&!world.ignoreSkip)world.phase='menu';
  if(key==='z'&&world.phase==='menu'&&!world.ignoreIntro)world.ready=true;
 }}}));
 const waitFrames=async(peer,count)=>{world.waits.push({slot:peer.slot,count});world.frame+=count;if(world.waitError)throw world.waitError;};
 return {world,peers,waitFrames};
}
async function run(fixture,check){
 const directory=fs.mkdtempSync(path.join(os.tmpdir(),'partyboard-mixed-return-'));
 try{return await check({peers:fixture.peers,inventory,outputDirectory:directory,waitFrames:fixture.waitFrames,matchingRecords,pause:async()=>{fixture.world.frame+=200;}});}finally{assert.equal(path.dirname(directory),os.tmpdir());assert.ok(path.basename(directory).startsWith('partyboard-mixed-return-'));fs.rmSync(directory,{recursive:true,force:true});}
}
test('mixed return handles original details/summary rules, fresh menu loop and bound main-stick navigation',async()=>{
 const f=fixture();await f.peers[0].keyboard.up('Enter');assert.equal(f.world.phase,'details','Skip alone must remain ineffective in the detailed view');f.world.keys=[];
 const result=await run(f,driveMixedPostRanking);assert.equal(f.world.phase,'menu');assert.equal(f.world.ready,true);
 assert.deepEqual(result.inputs.map(row=>row.key),['x','Enter','z','d','a']);assert.deepEqual(result.finalModeEntryCounts,[2,2,2,2]);
 assert.equal(result.captures.length,3);assert.equal(f.world.captures.length,12);assert.ok(result.captures.every(row=>row.matchingRecords===64));
 assert.equal(result.renderedMenuReviewed,false);assert.equal(result.noCardScreenVisuallyVerified,false);
 assert.ok(f.world.waits.some(row=>row.count===3));assert.ok(f.world.waits.some(row=>row.count===90));
 assert.ok(f.world.keys.filter(row=>row.key==='d'||row.key==='a').every(row=>row.slot===0),'Only actual menu owner supplies navigation');
});
test('rankings summary uses the same harmless Back normalization',async()=>{
 const f=fixture('summary');await run(f,driveMixedPostRanking);assert.equal(f.world.phase,'menu');assert.equal(f.world.keys.filter(row=>row.down&&row.key==='x').length,4);
});
test('observation failure releases every held mixed-peer key before rejection',async()=>{
 const f=fixture();f.world.waitError=Error('native observation failed');await assert.rejects(pressMixedRankingAction(f.peers,'Enter',f.waitFrames),/native observation failed/);
 assert.equal(f.world.keys.filter(row=>row.down).length,4);assert.equal(f.world.keys.filter(row=>!row.down).length,4);
});
test('paused peers and missing native capture fail before any game input',async()=>{
 const f=fixture();f.world.paused=true;await run(f,args=>assert.rejects(driveMixedPostRanking(args),/remain healthy/));assert.deepEqual(f.world.keys,[]);
 f.world.paused=false;delete f.peers[0].capture;await run(f,args=>assert.rejects(driveMixedPostRanking(args),/Every peer needs rendered capture/));assert.deepEqual(f.world.keys,[]);
});
test('mixed return retains checkpoint/RNG disagreement as a failure',async()=>{
 const f=fixture();f.world.badCheckpoint=true;await run(f,args=>assert.rejects(driveMixedPostRanking(args),/Mixed input\/checkpoint\/RNG disagreement/));
});
test('stale menu readiness is not accepted after returning',async()=>{
 const f=fixture();f.world.ignoreIntro=true;let now=0;await run(f,async args=>{
  const previous=Date.now;Date.now=()=>now+=100;
  try{await assert.rejects(driveMixedPostRanking({...args,timeoutMs:1000}),/fresh interactive menu loop/);}finally{Date.now=previous;}
 });assert.equal(f.world.captures.length,0);
});
test('unresponsive mixed ranking exit stops after four ordinary Back/Skip cycles',async()=>{
 const f=fixture();f.world.ignoreSkip=true;await run(f,args=>assert.rejects(driveMixedPostRanking(args),/Four ordinary Back\/Skip attempts/));
 assert.equal(f.world.keys.filter(row=>row.slot===0&&row.down&&row.key==='Enter').length,4);
});
