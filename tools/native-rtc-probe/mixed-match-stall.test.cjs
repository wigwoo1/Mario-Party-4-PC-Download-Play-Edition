'use strict';
const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),os=require('node:os');
const {driveMixedFullMatch}=require('./drive-mixed-full-match.cjs');
test('main-loop stall preserves four observed diagnostic snapshots and releases held inputs',async()=>{
 const outputDirectory=fs.mkdtempSync(path.join(os.tmpdir(),'partyboard-stall-'));
 const originalNow=Date.now,originalTimeout=global.setTimeout;
 let now=0,released=0;
 Date.now=()=>now+=10000;
 global.setTimeout=callback=>{queueMicrotask(callback);return 0;};
 const peers=Array.from({length:4},(_,index)=>{
  let count=0;
  return {slot:index,observe:async()=>({frame:100,progress:{turn:1,maxTurn:20,board:0,player:0,overlay:count++===0?89:44,event:0,mapView:false},observations:[],inputDiagnostics:{peer:index,pending:'waiting'},checkpointComponents:[index,2,3,4]}),keyboard:{press:async()=>{},down:async()=>{},up:async()=>{}},releaseAll:async()=>{released++;}};
 });
 try{
  await assert.rejects(driveMixedFullMatch({peers,outputDirectory,inventory:{overlays:[{name:'w01dll',overlay:89},{name:'mstory3dll',overlay:78},{name:'instdll',overlay:1},{name:'bootdll',overlay:2},{name:'m436dll',overlay:44}]}}),/stopped advancing in main gameplay loop/);
  const saved=JSON.parse(fs.readFileSync(path.join(outputDirectory,'mixed-full-match-stalled-state.json')));
  assert.equal(saved.peers.length,4);
  saved.peers.forEach((p,index)=>{assert.equal(p.state.frame,100);assert.equal(p.state.inputDiagnostics.peer,index);assert.deepEqual(p.state.checkpointComponents,[index,2,3,4]);});
  assert.equal(released,4);
 }finally{Date.now=originalNow;global.setTimeout=originalTimeout;}
});

