'use strict';
const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),os=require('node:os');
const {driveBrowserPostRanking,pressPostRankingAction}=require('./drive-browser-post-ranking.cjs');
const inventory={overlays:[{name:'mstory3dll',overlay:78},{name:'mentdll',overlay:70},{name:'modeseldll',overlay:74}]};
function fixture(phase){
 const world={phase,keys:[],focuses:0,waits:[],running:true,ignoreSkip:false};
 const pages=Array.from({length:4},(_,index)=>{
  let frame=1000,focused=false;
  return {on(){},off(){},evaluate:async fn=>{
   const state={frame:frame+=100,input:{state:world.running?'Running':'Paused'},progress:{overlay:world.phase==='menu'?74:78,event:1}};
   const previous=globalThis.partyBoardApp;
   globalThis.partyBoardApp={lobbyGameState:()=>state};
   try{return fn();}finally{globalThis.partyBoardApp=previous;}
  },
   locator:()=>({focus:async()=>{focused=true;world.focuses++;},screenshot:async()=>{}}),
   waitForFunction:async(fn,arg)=>{world.waits.push({index,arg});frame=Math.max(frame,typeof arg==='number'?arg+100:arg.frame+arg.settle);if(world.waitError)throw world.waitError;},
   keyboard:{down:async key=>{assert.ok(focused,'Actual game canvas must receive the key');world.keys.push({index,key,down:true});},up:async key=>{
    world.keys.push({index,key,down:false});
    // Original result.c: Start is ignored in details; B returns to summary.
    if(key==='x'&&world.phase==='details')world.phase='summary';
    if(key==='Enter'&&world.phase==='summary'&&!world.ignoreSkip)world.phase='menu';
   }} };
 });return {world,pages};
}
function output(){return fs.mkdtempSync(path.join(os.tmpdir(),'partyboard-post-ranking-'));}
test('normal Back then Skip returns all peers from the observed detailed-results state',async()=>{
 const {world,pages}=fixture('details');
 await pages[0].locator('#canvas').focus();await pages[0].keyboard.down('Enter');await pages[0].keyboard.up('Enter');
 assert.equal(world.phase,'details','Skip alone reproduces the unresponsive detailed view');world.keys=[];
 const result=await driveBrowserPostRanking({pages,inventory,outputDirectory:output()});
 assert.equal(world.phase,'menu');assert.deepEqual(result.inputs.map(row=>row.key),['x','Enter']);
 assert.ok(result.settled.every(state=>state.progress.overlay===74));
 assert.equal(world.keys.filter(row=>row.down&&row.key==='x').length,4);
 assert.equal(world.keys.filter(row=>row.down&&row.key==='Enter').length,4);
 assert.equal(result.renderedMenuReviewed,false,'A driver fixture cannot establish rendered gameplay');
});
test('Back is harmless on the rankings summary and preserves the normal Skip path',async()=>{
 const {world,pages}=fixture('summary');
 const result=await driveBrowserPostRanking({pages,inventory,outputDirectory:output()});
 assert.equal(world.phase,'menu');assert.equal(result.inputs.length,2);assert.ok(world.focuses>=8);
 assert.ok(world.waits.some(row=>typeof row.arg==='number'),'Keys wait on observed game frames');
});
test('a frame observation failure releases every held key and remains a failure',async()=>{
 const {world,pages}=fixture('summary');world.waitError=Error('engine stopped responding');
 await assert.rejects(pressPostRankingAction(pages,'Enter'),/engine stopped responding/);
 await new Promise(resolve=>setImmediate(resolve));
 assert.equal(world.keys.filter(row=>row.down).length,4);assert.equal(world.keys.filter(row=>!row.down).length,4);
});
test('post-ranking validation rejects paused peers rather than pressing through a failure',async()=>{
 const {world,pages}=fixture('summary');world.running=false;
 await assert.rejects(driveBrowserPostRanking({pages,inventory,outputDirectory:output()}),/Every peer must begin/);
 assert.deepEqual(world.keys,[]);
});
test('an unresponsive return path stops after four Back/Skip cycles',async()=>{
 const {world,pages}=fixture('summary');world.ignoreSkip=true;
 await assert.rejects(driveBrowserPostRanking({pages,inventory,outputDirectory:output(),timeoutMs:10000}),/Four ordinary Back\/Skip attempts/);
 assert.equal(world.keys.filter(row=>row.index===0&&row.down&&row.key==='Enter').length,4);
 assert.equal(world.keys.filter(row=>row.index===0&&row.down&&row.key==='x').length,4);
});
