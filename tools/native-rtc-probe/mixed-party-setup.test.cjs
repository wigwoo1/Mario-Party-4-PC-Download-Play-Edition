'use strict';
const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),os=require('node:os'),path=require('node:path');
const {driveMixedFullMatch}=require('./drive-mixed-full-match.cjs');
const inventory={overlays:[{name:'w01dll',overlay:89},{name:'mstory3dll',overlay:78},{name:'instdll',overlay:1},{name:'bootdll',overlay:2}]};
for(const target of [5,10])for(const transient of [false,true])test('mixed setup retains '+target+' turns with delayed menu telemetry'+(transient?' and a transient target value':''),async()=>{
 const directory=fs.mkdtempSync(path.join(os.tmpdir(),'partyboard-mixed-setup-'));
 const originalNow=Date.now,originalTimer=global.setTimeout;let frame=0,now=0,row=0,turns=20,pending,readyAt=0,board=false,prompt=false,drifted=false,driftAt,releaseCount=0;
 const actions=[];
 const tick=()=>{
  if(pending&&frame>=pending.at){pending.effect();pending=undefined;readyAt=frame+30;}
  if(driftAt!==undefined&&frame>=driftAt){turns=target+5;driftAt=undefined;}
 };
 Date.now=()=>now;
 global.setTimeout=(callback,ms)=>{now+=ms;frame+=Math.max(1,Math.ceil(ms/16));tick();queueMicrotask(callback);return 0;};
 const progress=()=>({overlay:board?89:70,turn:board?1:0,maxTurn:board?turns:0,board:0,partySetupRow:board?-1:row,partySetupTurns:board?-1:turns,partySetupPort:board?-1:2});
 // Deliberately shuffle owners: controller port two belongs to array entry zero.
 const peers=[2,0,3,1].map(slot=>({slot,
  observe:async()=>{tick();return {frame,progress:progress(),observations:[]};},observeFrame:async()=>{tick();return frame;},
  keyboard:{press:async(key,{delay}={})=>{
   assert.equal(slot,2,'Only the actual menu controller may receive a setup key');
   assert.ok(!pending&&frame>=readyAt,'No setup key may overtake an animation or key-reset window');actions.push({key,frame,row,turns});
   const beforeRow=row,beforeTurns=turns;
   pending={at:frame+10,effect:()=>{
    if(key==='z'){if(beforeRow<4)row=beforeRow+1;else if(!prompt)prompt=true;else board=true;}
    else if(key==='x')row=beforeRow-1;
    else {assert.equal(beforeRow,1);const delta=key==='a'?-5:5;turns=((beforeTurns-5+delta+50)%50)+5;
     if(transient&&!drifted&&turns===target){drifted=true;driftAt=frame+20;}}
   }};
   await new Promise(resolve=>setTimeout(resolve,delay));
  }},releaseAll:async()=>{releaseCount++;}}));
 let verified;
 try{
  await assert.rejects(driveMixedFullMatch({peers,inventory,outputDirectory:directory,maxTurn:target,onSetupVerified:e=>{verified=e;throw Error('focused setup complete');}}),/focused setup complete/);
  assert.ok(verified);assert.equal(verified.maxTurn,target);assert.ok(verified.peers.every(p=>p.progress.turn===1&&p.progress.maxTurn===target&&p.progress.board===0));
  assert.equal(turns,target);assert.equal(releaseCount,4);assert.ok(actions.length>=7);if(transient)assert.equal(drifted,true);
  const confirmed=actions.findIndex(a=>a.key==='z'&&a.row===1&&a.turns===target);assert.ok(confirmed>=0);
  assert.ok(actions.slice(confirmed+1).every(a=>a.key==='z'),'No direction may change the selected length after confirmation');
 }finally{
  Date.now=originalNow;global.setTimeout=originalTimer;
  const relative=path.relative(os.tmpdir(),directory);assert.ok(relative&&!relative.startsWith('..')&&!path.isAbsolute(relative));fs.rmSync(directory,{recursive:true,force:true});
 }
});
