'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {matchingRecords}=require('./drive-mixed-full-match.cjs');
// Diagnostic controller only. Uses normal game menus and input; never writes
// game memory, skips a checkpoint, or reports a Party match as completed.
async function driveFocusedFreePlay({peers,inventory,outputDirectory}){
 fs.mkdirSync(outputDirectory,{recursive:true});
 const target=inventory.overlays.find(item=>item.name.toLowerCase()==='m443dll')?.overlay;
 assert.equal(target,51);assert.deepEqual(peers.map(peer=>peer.slot).sort(),[0,1,2,3]);
 const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms)),deadline=Date.now()+20*60*1000;
 let lastSavedInput=-1;
 const snapshot=async()=>{
  assert(Date.now()<deadline,'Focused Free Play diagnostic exceeded 20 minutes');
  const states=await Promise.all(peers.map(peer=>peer.observe()));
  assert(!states.some(state=>state.inputDiagnostics?.queue?.stopReason==='Divergence'),
   'Focused minigame input queue stopped on actual gameplay divergence');
  const records=matchingRecords(states.map(state=>state.observations));
  const fresh=records.filter(record=>Number(record.frame)>lastSavedInput);
  if(fresh.length){
   fs.appendFileSync(path.join(outputDirectory,'focused-completed-inputs.jsonl'),fresh.map(record=>JSON.stringify(record)).join('\n')+'\n');
   lastSavedInput=Number(fresh.at(-1).frame);
  }
  fs.writeFileSync(path.join(outputDirectory,'focused-latest.json'),JSON.stringify({at:new Date().toISOString(),states,matchingRecords:records.length},null,2));
  return states;
 };
 const settle=async(count=90)=>{const start=(await snapshot()).map(state=>state.frame);while(true){const states=await snapshot();if(states.every((state,i)=>state.frame>=start[i]+count))return;await sleep(100);}};
 const capture=async name=>{await Promise.all(peers.map((peer,i)=>peer.capture?.(path.join(outputDirectory,name+'-peer-'+i+'.png'))));};
 const press=async(key,all=false)=>{await Promise.all((all?peers:[peers.find(peer=>peer.slot===0)]).map(peer=>peer.keyboard.press(key,{delay:120})));await settle();};
 let lastCommand,auto=false,awardFrames=null;
 try{
  while((await snapshot()).some(state=>state.frame<1000))await sleep(100);
  await press('Enter');await settle(210);
  for(const key of ['z','d','d','z'])await press(key);
  await settle(210);
  for(const key of ['z','z','z','z','z','z','z','z','z'])await press(key);
  await capture('focused-after-menu-prelude');
  console.log(JSON.stringify({stage:'focused-freeplay-controls',directory:outputDirectory,targetOverlay:target,
   instructions:'Owned command.json accepts press/capture/auto-race. Select the target using normal menu controls; no game state writes.'}));
  if(process.env.PARTYBOARD_NATIVE_FOCUSED_AUTOMENU==='1'){
   // Replay the observed four-human Free Play path with animation settles.
   await press('z');await settle(210);await press('z');await press('z');
   await settle(210);await press('z');await press('z',true);await settle(210);
   for(let index=0;index<14;++index)await press('s');
   await capture('focused-selected-speedwagons');
   await press('z');await settle(240);await press('Enter');
   while((await snapshot()).some(state=>state.progress.overlay!==target))await sleep(100);
   await capture('focused-entered-speedwagons');auto=true;
  }
  while(true){
   let states=await snapshot();
   const commandFile=path.join(outputDirectory,'focused-command.json');
   if(fs.existsSync(commandFile)){
    const command=JSON.parse(fs.readFileSync(commandFile,'utf8'));
    if(command.id!==lastCommand){
     assert.match(command.id,/^[A-Za-z0-9_-]{1,64}$/);lastCommand=command.id;
     if(command.action==='press'){
      assert.ok(['Enter','z','x','w','a','s','d','e','q'].includes(command.key));
      if(command.all)await press(command.key,true);
      else{
       assert(Number.isInteger(command.slot??0)&&(command.slot??0)>=0&&(command.slot??0)<4);
       await peers.find(peer=>peer.slot===(command.slot??0)).keyboard.press(command.key,{delay:120});await settle();
      }
     }else if(command.action==='auto-race'){
      assert(states.every(state=>state.progress.overlay===target),'Enter the actual target minigame before enabling race inputs');auto=true;
     }else assert.equal(command.action,'capture');
     await capture('focused-command-'+command.id);
     fs.appendFileSync(path.join(outputDirectory,'focused-commands.jsonl'),JSON.stringify({...command,at:new Date().toISOString()})+'\n');
    }
   }
   if(auto){
    await Promise.all(peers.map(async peer=>{await peer.keyboard.down('e');try{await peer.keyboard.press('z');await sleep(880);}finally{await peer.keyboard.up('e');}}));
    states=await snapshot();
    const scores=states.map(state=>[155,205,255,305].map(index=>state.globalCheckpointFields[index]));
    if(!awardFrames&&states.every(state=>state.progress.overlay===target)&&scores[0].some(score=>score>0)&&scores.every(score=>JSON.stringify(score)===JSON.stringify(scores[0]))){
     awardFrames=states.map(state=>state.frame);
     fs.writeFileSync(path.join(outputDirectory,'focused-coin-award.json'),JSON.stringify({scope:'actual-m443-coin-award',states,scores},null,2));
    }
    if(awardFrames&&states.every((state,i)=>state.frame>=awardFrames[i]+120)){
     const records=matchingRecords(states.map(state=>state.observations));assert(records.length>=60);
     const evidence={scope:'actual-mixed-native-browser-focused-freeplay',targetOverlay:target,normalMenuInputOnly:true,
      coinAwardVerified:true,matchingFinalRecords:records.length,partyMatchCompleted:false,states};
     fs.writeFileSync(path.join(outputDirectory,'focused-acceptance.json'),JSON.stringify(evidence,null,2));return evidence;
    }
   }
   await sleep(200);
  }
 }catch(error){
  const states=await Promise.allSettled(peers.map(peer=>peer.observe()));
  fs.writeFileSync(path.join(outputDirectory,'focused-failure.json'),JSON.stringify({error:String(error),at:new Date().toISOString(),states},null,2));throw error;
 }finally{await Promise.allSettled(peers.map(peer=>peer.releaseAll()));}
}
module.exports={driveFocusedFreePlay};
