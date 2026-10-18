'use strict';
const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),os=require('node:os'),path=require('node:path');
const {driveMixedFullMatch}=require('./drive-mixed-full-match.cjs');
const inventory={overlays:[{name:'w01dll',overlay:89},{name:'w02dll',overlay:90},{name:'mstory3dll',overlay:78},{name:'instdll',overlay:1},{name:'bootdll',overlay:2}]};
for(const wrong of [false,true])test(wrong?'rejects an actual wrong board':'waits for staggered arrival on the requested board',async()=>{
 const outputDirectory=fs.mkdtempSync(path.join(os.tmpdir(),'partyboard-board-transition-'));
 let presses=0;
 const peers=Array.from({length:4},(_,slot)=>{let polls=0;return {slot,observe:async()=>{
  if(++polls>2)throw Error('after-setup-sentinel');
  return {frame:polls,progress:{overlay:wrong?90:polls===1&&slot===3?70:89,turn:1,maxTurn:20,board:wrong?1:0,partySetupRow:-1,partySetupTurns:-1},observations:[]};
 },keyboard:{press:async()=>{presses++;}},releaseAll:async()=>{}};});
 try{await assert.rejects(driveMixedFullMatch({peers,inventory,outputDirectory}),wrong?/Ordinary setup entered a different board/:/after-setup-sentinel/);assert.equal(presses,0);}
 finally{fs.rmSync(outputDirectory,{recursive:true,force:true});}
});
