'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
async function pressMixedRankingAction(peers,key,waitFrames){
 const results=await Promise.allSettled(peers.map(async peer=>{
  await peer.keyboard.down(key);
  try{await waitFrames(peer,3);}finally{await peer.keyboard.up(key);}
 }));
 const failed=results.find(result=>result.status==='rejected');if(failed)throw failed.reason;
}
async function driveMixedPostRanking({peers,inventory,outputDirectory,waitFrames,matchingRecords,timeoutMs=300000,pause=ms=>new Promise(resolve=>setTimeout(resolve,ms))}){
 assert.ok(Number.isInteger(timeoutMs)&&timeoutMs>=1000&&timeoutMs<=600000,'Bounded return observation required');
 assert.deepEqual(peers.map(peer=>peer.slot),[0,1,2,3],'Actual four-port ownership required');
 assert.ok(peers.every(peer=>typeof peer.capture==='function'&&typeof peer.modeEntryCount==='function'),'Every peer needs rendered capture and fresh-menu receipt observation');
 assert.equal(typeof waitFrames,'function');assert.equal(typeof matchingRecords,'function');
 const find=name=>inventory.overlays.find(row=>row.name.toLowerCase()===name)?.overlay,awards=find('mstory3dll'),mode=find('modeseldll');
 assert.ok(Number.isInteger(awards)&&Number.isInteger(mode),'Authoritative ranking/menu overlays required');
 const read=async()=>{const states=await Promise.all(peers.map(peer=>peer.observe()));assert.ok(states.every(state=>Number.isSafeInteger(state.frame)&&!state.error&&(!state.input||state.input.state==='Running')),'Post-ranking peers must remain healthy');return states;};
 const initial=await read();assert.ok(initial.every(state=>state.progress.overlay===awards&&state.progress.event===1),'Every peer must start on final rankings');
 const receipts=await Promise.all(peers.map(peer=>peer.modeEntryCount()));assert.ok(receipts.every(Number.isSafeInteger),'Menu receipt counters required');
 const history=[],inputs=[],captures=[],started=Date.now();let attempts=0,lastInput=-Infinity;
 const capture=async id=>{
  const before=await read();
  const files=await Promise.all(peers.map((peer,index)=>peer.capture(path.join(outputDirectory,'mixed-post-ranking-'+id+'-peer'+index+'.png'))));
  const after=await read(),common=matchingRecords(after.map(state=>state.observations));assert.ok(common.length>=30,'Matching sampled menu input/checkpoint/RNG records required');
  captures.push({id,before:before.map(s=>({frame:s.frame,progress:s.progress})),after:after.map(s=>({frame:s.frame,progress:s.progress})),matchingRecords:common.length,files});
 };
 while(Date.now()-started<timeoutMs){
  const states=await read();history.push(states.map(state=>({frame:state.frame,progress:state.progress})));
  fs.writeFileSync(path.join(outputDirectory,'mixed-post-ranking-observation.json'),JSON.stringify({history,inputs},null,2)+'\n');
  if(states.every(state=>state.progress.overlay===mode)){
   await Promise.all(peers.map(peer=>waitFrames(peer,420)));
   assert.ok((await read()).every(state=>state.progress.overlay===mode),'Returned mode-selection overlay must remain stable');
   const fresh=async()=>{const counts=await Promise.all(peers.map(peer=>peer.modeEntryCount()));return counts.every((count,index)=>Number.isSafeInteger(count)&&count>receipts[index]);};
   if(!await fresh()){
    inputs.push({key:'z',peer:0,purpose:'confirm normal envelope introduction'});
    await pressMixedRankingAction([peers[0]],'z',waitFrames);await Promise.all(peers.map(peer=>waitFrames(peer,600)));
   }
   while(!await fresh()){
    assert.ok(Date.now()-started<timeoutMs,'Every peer must report a fresh interactive menu loop');
    assert.ok((await read()).every(state=>state.progress.overlay===mode),'Mode selection changed before its fresh interactive receipt');await pause(100);
   }
   for(const [key,id]of [[null,'selection'],['d','right'],['a','return']]){
    if(key){inputs.push({key,peer:0,purpose:'normal main-stick menu navigation'});await pressMixedRankingAction([peers[0]],key,waitFrames);await Promise.all(peers.map(peer=>waitFrames(peer,90)));}
    assert.ok((await read()).every(state=>state.progress.overlay===mode),'Navigation must remain in mode selection');await capture(id);
   }
   const result={status:'execution passed; visual review required',scope:'Ordinary Back then Skip leaves rankings/details; every peer reports fresh mode selection and receives normal host main-stick navigation',initial:initial.map(s=>({frame:s.frame,progress:s.progress})),inputs,captures,modeEntryBaselines:receipts,finalModeEntryCounts:await Promise.all(peers.map(peer=>peer.modeEntryCount())),interactiveMenuExecutionCompleted:true,renderedMenuReviewed:false,noCardScreenVisuallyVerified:false,elapsedMs:Date.now()-started};
   fs.writeFileSync(path.join(outputDirectory,'mixed-post-ranking-return.json'),JSON.stringify(result,null,2)+'\n',{flag:'wx'});return result;
  }
  if(states.every(state=>state.progress.overlay===awards)&&states[0].frame>=lastInput+180){
   assert.ok(attempts++<4,'Four ordinary Back/Skip attempts did not leave mixed rankings');
   inputs.push({key:'x',purpose:'leave detailed results if open',frames:states.map(s=>s.frame)});await pressMixedRankingAction(peers,'x',waitFrames);
   await Promise.all(peers.map(peer=>waitFrames(peer,90)));
   const beforeSkip=await read();
   if(beforeSkip.every(state=>state.progress.overlay===awards)){
    lastInput=beforeSkip[0].frame;inputs.push({key:'Enter',purpose:'skip final standings',frames:beforeSkip.map(s=>s.frame)});await pressMixedRankingAction(peers,'Enter',waitFrames);
   }
  }
  await pause(100);
 }
 throw Error('Mixed post-ranking return did not complete within its bounded lifetime');
}
module.exports={driveMixedPostRanking,pressMixedRankingAction};
