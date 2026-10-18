'use strict';
const assert=require('node:assert/strict'),path=require('node:path'),fs=require('node:fs');
const {createFullMatchProgress}=require('./full-match-progress.cjs');
const {circularKeyboardStick}=require('./circular-keyboard-stick.cjs');
const {sweepKeyboardStick}=require('./sweep-keyboard-stick.cjs');
const DEFAULT_FULL_MATCH_TIMEOUT_MS=6*60*60*1000;
function fullMatchDeadline(startedAt,timeoutMs=DEFAULT_FULL_MATCH_TIMEOUT_MS){
 assert.ok(Number.isSafeInteger(startedAt)&&startedAt>=0,'Full-match start time must be a nonnegative safe integer.');
 assert.ok(Number.isSafeInteger(timeoutMs)&&timeoutMs>0,'Full-match timeout must be a positive safe integer in milliseconds.');
 const deadline=startedAt+timeoutMs;
 assert.ok(Number.isSafeInteger(deadline),'Full-match deadline must remain a safe integer.');
 return deadline;
}
function fullMatchTimeoutFromEnvironment(environment){
 const value=environment.PARTYBOARD_TEST_GAME_FULL_MATCH_TIMEOUT_MS;
 if(value===undefined)return undefined;
 const timeoutMs=Number(value);
 assert.ok(Number.isSafeInteger(timeoutMs)&&timeoutMs>0,'PARTYBOARD_TEST_GAME_FULL_MATCH_TIMEOUT_MS must be a positive safe integer in milliseconds.');
 return timeoutMs;
}
function fullMatchHarnessTimeout(timeoutMs=DEFAULT_FULL_MATCH_TIMEOUT_MS){
 assert.ok(Number.isSafeInteger(timeoutMs)&&timeoutMs>0,'Full-match timeout must be a positive safe integer in milliseconds.');
 const harnessTimeout=timeoutMs+10*60*1000;
 assert.ok(Number.isSafeInteger(harnessTimeout),'Full-match harness timeout must remain a safe integer.');
 return harnessTimeout;
}
async function boundedPeerOperation(index,kind,operation){
 let timer;
 try{
  return await Promise.race([
   operation(),
   new Promise((_,reject)=>{timer=setTimeout(()=>reject(new Error('Peer '+index+' '+kind+' did not respond within 120 seconds.')),120000);})
  ]);
 }finally{clearTimeout(timer);}
}
function readPeerObservation(page,index,kind,evaluate){
 return boundedPeerOperation(index,kind+' observation',()=>page.evaluate(evaluate));
}
// Test plans use WASD direction names; inject the current default arrow-key stick.
const movementKey={w:'ArrowUp',a:'ArrowLeft',s:'ArrowDown',d:'ArrowRight'};
function pressKey(page,index,key,options){
 key=movementKey[key]??key;
 return boundedPeerOperation(index,'keyboard '+key,()=>page.keyboard.press(key,options));
}
function holdKey(page,index,key,down){
 key=movementKey[key]??key;
 return boundedPeerOperation(index,'keyboard '+key+(down?' down':' up'),()=>down?page.keyboard.down(key):page.keyboard.up(key));
}
function waitTwoGameFrames(frame){
 const current=partyBoardApp.lobbyGameState();
 return current?.input.state==='Running'&&BigInt(current.input.frame)>=BigInt(frame)+2n;
}
async function pressMarioMedleyInput(page,index){
 // Mario Medley cycles between an A+B chord and alternating X/Y or A/B
 // prompts. A one-button default cannot satisfy every prompt. Press all four
 // face buttons together briefly; each prompt checks only its required mask.
 // Releasing after two emulated frames lets the next call create fresh edges.
 const keys=['z','x','c','v'];
 const state=await readGameState(page,index);
 await Promise.all(keys.map(key=>holdKey(page,index,key,true)));
 try{
  await boundedPeerOperation(index,'Mario Medley button chord frame',()=>page.waitForFunction(
   waitTwoGameFrames,
   String(state.frame),{timeout:120000}));
 }finally{
  await Promise.all(keys.map(key=>holdKey(page,index,key,false)));
 }
}
async function pressM432ObstacleInput(page,index,movementKey='w'){
 // m432's player code maps obstacle classes to B, X, or Y button edges:
 // the 0x100 class accepts B (0x200), 0x200 accepts X (0x400), and
 // 0x400 accepts Y (0x800). A generic A tap can therefore wait through
 // an obstacle without triggering the matching response. Briefly chord
 // B/X/Y while steering, then release to provide fresh edges next pass.
 assert.ok(['w','a','s','d'].includes(movementKey),'m432 steering key must be a WASD direction.');
 const keys=['x','c','v'];
 const state=await readGameState(page,index);
 await Promise.all([...keys,movementKey].map(key=>holdKey(page,index,key,true)));
 try{
  await boundedPeerOperation(index,'m432 obstacle-response frame',()=>page.waitForFunction(
   waitTwoGameFrames,
   String(state.frame),{timeout:120000}));
 }finally{
  await Promise.all([...keys,movementKey].map(key=>holdKey(page,index,key,false)));
 }
}
function readGameState(page,index){
 return readPeerObservation(page,index,'game-state',()=>partyBoardApp.lobbyGameState());
}
function readInputObservations(page,index){
 return readPeerObservation(page,index,'final input',()=>partyBoardApp.lobbyInputObservations());
}
function rankingCaptureTargetFrame(frame,minimumAdvance=60){
 assert.match(String(frame),/^\d+$/,'Ranking capture frame must be a nonnegative integer.');
 assert.ok(Number.isSafeInteger(minimumAdvance)&&minimumAdvance>0,'Ranking capture frame advance must be a positive safe integer.');
 return (BigInt(frame)+BigInt(minimumAdvance)).toString();
}
function rankingCaptureFrameReached({frame,overlay}){
 const state=partyBoardApp.lobbyGameState();
 return state?.input.state==='Running'&&state.progress.overlay===overlay&&state.progress.event===1&&
  BigInt(state.input.frame)>=BigInt(frame);
}
function m427AlternatingStrokeKey(strokeIndex){
 assert.ok(Number.isSafeInteger(strokeIndex)&&strokeIndex>=0,'Paddle stroke index must be a nonnegative safe integer.');
 return strokeIndex%2===0?'z':'x';
}
function mapViewOwnerIndex(states,port){
 const owners=states.flatMap((state,index)=>{
  const binding=state.input.localPadSlots?.[port];
  return Number.isInteger(binding)&&binding>=0&&binding<4?[index]:[];
 });
 assert.equal(owners.length,1,'Map-view controller port must have exactly one local owner');
 return owners[0];
}
async function driveProductionFullMatch({pages,inventory,outputDirectory,maxTurn=20,timeoutMs=DEFAULT_FULL_MATCH_TIMEOUT_MS}){
 assert.ok(Number.isInteger(maxTurn)&&maxTurn>=5&&maxTurn<=50&&maxTurn%5===0,'Full-match length must be 5 through 50 turns in five-turn steps.');
 const awards=inventory.overlays.find(scene=>scene.name.toLowerCase()==='mstory3dll');
 const instructions=inventory.overlays.find(scene=>scene.name.toLowerCase()==='instdll');
 assert.ok(awards&&instructions,'Authoritative USA awards/instructions overlays required.');
 const initial=await Promise.all(pages.map(readGameState));
 assert.ok(initial.every(state=>state.progress?.turn===1),'Full match must begin on ordinary turn one.');
 assert.ok(initial.every(state=>state.progress.maxTurn===maxTurn),'Full match must use the requested configured turn count.');
 const trackers=initial.map(state=>createFullMatchProgress({board:state.progress.board,maxTurn,awardOverlay:awards.overlay}));
 const started=Date.now(),deadline=fullMatchDeadline(started,timeoutMs),lastFrames=initial.map(state=>state.frame),lastProgress=pages.map(()=>started);
 let direction=0,pinballShot=0,riverRidersStroke=0,previousTurns='',final;
 const turnChecks=[];
 const sceneCaptures=[],sceneEntries=pages.map(()=>null),capturedScenes=pages.map(()=>new Set());
 const boardDecisionEntries=pages.map(()=>null);
 // A new run has no scene evidence yet. Do not expose a previous run's ledger
 // while the current match is still approaching its first minigame.
 fs.writeFileSync(path.join(outputDirectory,'browser-full-match-scene-captures.json'),'[]\n');
 while(true){
  const states=await Promise.all(pages.map(readGameState));
  const complete=states.map((state,index)=>{
   assert.equal(state?.input.state,'Running','Every actual full-match runtime must remain synchronized.');
   if(state.frame>lastFrames[index]){lastFrames[index]=state.frame;lastProgress[index]=Date.now();}
   assert.ok(Date.now()-lastProgress[index]<120000,'Actual full-match simulation stopped progressing.');
   return trackers[index].observe(state.progress);
  });
  // Preserve actual scene pixels for later visual review. Captures do not
  // establish rendering correctness or complete shader coverage by themselves.
  for(let index=0;index<pages.length;index++){
   const state=states[index];
   const scene=inventory.overlays.find(item=>item.overlay===state.progress.overlay);
   const isBoard=/^w\d+dll$/i.test(scene?.name||'');
   const isResult=scene?.name?.toLowerCase()==='resultdll';
   const key=[state.progress.turn,state.progress.overlay,state.progress.event,...(isBoard?[state.progress.player]:[])].join('-');
   if(sceneEntries[index]?.key!==key)sceneEntries[index]={key,frame:state.frame};
   if(!scene||(!isBoard&&!isResult&&!/^m\d+dll$/i.test(scene.name)))continue;
   // Result overlays can leave before 900 frames when ordinary A input
   // acknowledges Next. Retain intermediate pixels for prompt/glyph review.
   const captureAge=(isBoard?[1800,6000]:isResult?[120,240,360,600,900]:[120,900,6000,18000]).find(age=>state.frame-sceneEntries[index].frame>=age&&!capturedScenes[index].has(key+'-'+age));
   if(captureAge===undefined)continue;
   const filename='browser-full-match-scene-peer'+index+'-'+key+'-'+captureAge+'.png';
   await pages[index].locator('#canvas').screenshot({path:path.join(outputDirectory,filename),timeout:120000});
   const after=await readGameState(pages[index],index);
   const retainedScene=after?.progress?.turn===state.progress.turn&&after.progress.overlay===state.progress.overlay&&after.progress.event===state.progress.event&&(!isBoard||after.progress.player===state.progress.player);
   sceneCaptures.push({peer:index,scene:scene.name,scope:isBoard?'board-decision-observation':isResult?'minigame-result-observation':'minigame-observation',filename,captureAge,before:state.progress,after:after?.progress,
    beforeFrame:state.frame,afterFrame:after?.frame,retainedScene,renderingVerified:false,shaderCoverageVerified:false});
   if(retainedScene)capturedScenes[index].add(key+'-'+captureAge);
   fs.writeFileSync(path.join(outputDirectory,'browser-full-match-scene-captures.json'),JSON.stringify(sceneCaptures,null,2)+'\n');
  }
  const turns=states.map(state=>state.progress.turn).join(',');
  if(turns!==previousTurns){
   const observations=await Promise.all(pages.map(readInputObservations));
   const common=observations[0].filter(record=>observations.every(records=>records.some(other=>other.frame===record.frame))).slice(-30);
   assert.equal(common.length,30,'Each ordinary turn transition requires recent common input/checkpoint records.');
   for(const record of common)for(const records of observations)assert.deepEqual(records.find(other=>other.frame===record.frame),record);
   // Retain each peer's actual records so a later acceptance review can repeat
   // the comparison instead of relying on the driver's matching-count claim.
   const commonFrames=new Set(common.map(record=>record.frame));
   fs.writeFileSync(path.join(outputDirectory,'browser-full-match-turn-inputs-'+turns.replaceAll(',','-')+'.json'),
    JSON.stringify({turns:states.map(state=>state.progress.turn),peers:observations.map(records=>records.filter(record=>commonFrames.has(record.frame)))},null,2)+'\n');
   turnChecks.push({turns:states.map(state=>state.progress.turn),firstPoll:common[0].frame,lastPoll:common.at(-1).frame,matchingPolls:common.length});
   fs.writeFileSync(path.join(outputDirectory,'browser-full-match-turn-checkpoints.json'),JSON.stringify(turnChecks,null,2)+'\n');
   console.log('Actual full-match turns: '+turns+'; 30 common checkpoint/input records match');previousTurns=turns;
  }
  if(complete.every(Boolean)){final=states;break;}
   assert.ok(Date.now()<deadline,'Ordinary full match exceeded its configured '+Math.ceil(timeoutMs/60000)+'-minute limit before final awards and rankings.');
  if(states.every(state=>state.progress.mapView)) {
   // ExecMapView requires exactly B down (0x200); A only accelerates scrolling.
   // Use the normal mapped Return key on the active player's owned page.
   const port=states[0].progress.mapViewPort;
   assert.ok(Number.isInteger(port)&&port>=0&&port<4&&states.every(state=>state.progress.mapViewPort===port),'Map view must report its actual controller port');
   const owner=mapViewOwnerIndex(states,port);
   await pressKey(pages[owner],owner,'x',{delay:250});
  }
  else if(states.every(state=>state.progress.overlay===instructions.overlay))await pressKey(pages[0],0,'Enter',{delay:250});
  else if(states.every(state=>inventory.overlays.find(scene=>scene.overlay===state.progress.overlay)?.name.toLowerCase()==='m443dll')){
   // Original racing code reads the analog R trigger for throttle and A down
   // for an upshift when engine speed permits it. Default keyboard E maps R.
   await Promise.all(pages.map(async (page,index)=>{
    await holdKey(page,index,'e',true);
    try{await pressKey(page,index,'z',{delay:120});await page.waitForTimeout(880);}
    finally{await holdKey(page,index,'e',false);}
   }));
  }
  else if(states.every(state=>inventory.overlays.find(scene=>scene.overlay===state.progress.overlay)?.name.toLowerCase()==='m444dll')){
   // Original power grows by 0.45 per simulated down-stick frame, capped50.
   // A wall-clock hold may cover only a few frames under four-engine load.
   // Vary ordinary shot strength in frames, launching while still pulling.
   const pullFrames=[112,80,64,96][pinballShot++%4];
   await Promise.all(pages.map(async (page,index)=>{
    await holdKey(page,index,'s',true);
    try{
     const initial=await readGameState(page,index);
     await page.waitForFunction(({frame,pullFrames})=>partyBoardApp.lobbyGameState()?.frame>=frame+pullFrames,
        {frame:initial.frame,pullFrames},{timeout:120000});
     await holdKey(page,index,'z',true);
     try{
      const launch=await readGameState(page,index);
      await page.waitForFunction(frame=>partyBoardApp.lobbyGameState()?.frame>=frame+4,launch.frame,{timeout:120000});
     }finally{await holdKey(page,index,'z',false);}
    }
    finally{await holdKey(page,index,'s',false);}
   }));
  }
  else if(states.every(state=>inventory.overlays.find(scene=>scene.overlay===state.progress.overlay)?.name.toLowerCase()==='m428dll')){
   // Original m428 player code climbs from positive left-stick Y. Cycling
   // through down cancels that progress; A instead changes the grab state.
   // Use ordinary controller input toward the goal, without editing game state.
   await Promise.all(pages.map((page,index)=>pressKey(page,index,'w',{delay:1000})));
  }
  else if(states.every(state=>inventory.overlays.find(scene=>scene.overlay===state.progress.overlay)?.name.toLowerCase()==='m405dll')){
   await Promise.all(pages.map((page,index)=>pressMarioMedleyInput(page,index)));
  }
  else if(states.every(state=>inventory.overlays.find(scene=>scene.overlay===state.progress.overlay)?.name.toLowerCase()==='m427dll')){
   // This canoe minigame maps A and B down edges to its two paddle strokes;
   // alternate them so test inputs work both sides of the boat.
   const key=m427AlternatingStrokeKey(riverRidersStroke++);
   await Promise.all(pages.map((page,index)=>pressKey(page,index,key,{delay:120})));
  }
  else if(states.every(state=>inventory.overlays.find(scene=>scene.overlay===state.progress.overlay)?.name.toLowerCase()==='m432dll')){
   const key=['w','a','s','d'][direction++%4];
   await Promise.all(pages.map((page,index)=>pressM432ObstacleInput(page,index,key)));
  }
  else{
   await Promise.all(pages.map((page,index)=>complete[index]?Promise.resolve():pressKey(page,index,'z',{delay:120})));
   const boardEvent=states.every(state=>/^w\d+dll$/i.test(inventory.overlays.find(scene=>scene.overlay===state.progress.overlay)?.name||''));
   if(boardEvent)await Promise.all(pages.map((page,index)=>{
    const key=[states[index].progress.turn,states[index].progress.overlay,states[index].progress.player].join('-');
    if(boardDecisionEntries[index]?.key!==key)boardDecisionEntries[index]={key,frame:states[index].frame,sweeps:0};
    // A prolonged ordinary board interaction may require area coverage
    // (scratch ticket), while rotation remains necessary for the lottery drum.
    const stick=states[index].frame-boardDecisionEntries[index].frame>=7200&&boardDecisionEntries[index].sweeps++%2===0?sweepKeyboardStick:circularKeyboardStick;
    return stick({
    hold:(key,down)=>holdKey(page,index,key,down),
    waitFrames:async count=>{
     const state=await readGameState(page,index);
     await boundedPeerOperation(index,'circular stick frames',()=>page.waitForFunction(
      ({frame,count})=>partyBoardApp.lobbyGameState()?.frame>=frame+count,
      {frame:state.frame,count},{timeout:120000}));
    },
   });}));
   else {await Promise.all(pages.map((page,index)=>complete[index]?Promise.resolve():pressKey(page,index,['w','d','s','a'][direction%4],{delay:250})));direction++;}
  }
  await pages[0].waitForTimeout(1000);
 }
 const frame=final.reduce((latest,state)=>BigInt(state.input.frame)>latest?BigInt(state.input.frame):latest,0n);
 // result.c starts its title at x=1800 and moves it to -175 at five
 // units per frame (395 frames), after the opening wipe and 60-frame hold.
 // Allow the subsequent rotation to settle before capturing the standings.
 const rankingSettlePolls=600;
 await Promise.all(pages.map(page=>page.waitForFunction(({frame,overlay,settlePolls})=>{const state=partyBoardApp.lobbyGameState();return state?.input.state==='Running'&&
  state.progress.overlay===overlay&&state.progress.event===1&&BigInt(state.input.frame)>=BigInt(frame)+BigInt(settlePolls);},{frame:String(frame),overlay:awards.overlay,settlePolls:rankingSettlePolls},{timeout:120000})));
  const observations=await Promise.all(pages.map(readInputObservations));
  const common=observations[0].filter(record=>BigInt(record.frame)>=frame&&observations.every(records=>records.some(other=>other.frame===record.frame)));
 fs.writeFileSync(path.join(outputDirectory,'browser-full-match-ranking-inputs.json'),
  JSON.stringify({rankingStartFrame:String(frame),peers:observations},null,2)+'\n');
  assert.ok(common.length>=30,'Fresh matching final-ranking polls required.');
 for(const record of common)for(const records of observations)assert.deepEqual(records.find(other=>other.frame===record.frame),record);
 for(let index=0;index<pages.length;index++){
  const page=pages[index];
  await page.bringToFront();
  // Background WebGPU canvases can be captured before the compositor has
  // presented their latest frame. Read a fresh baseline after foregrounding,
  // then require additional game frames before capturing the ranking screen.
  const foregroundState=await readGameState(page,index);
  assert.equal(foregroundState?.input?.state,'Running','Ranking capture peer must remain in the running match.');
  assert.equal(foregroundState.progress?.overlay,awards.overlay,'Ranking capture peer must remain on the awards overlay.');
  assert.equal(foregroundState.progress?.event,1,'Ranking capture peer must have reached the rankings event.');
  const captureFrame=rankingCaptureTargetFrame(foregroundState.input.frame);
  await boundedPeerOperation(index,'ranking render settling',()=>page.waitForFunction(
   rankingCaptureFrameReached,{frame:captureFrame,overlay:awards.overlay},{timeout:120000}));
  await page.locator('#canvas').screenshot({path:path.join(outputDirectory,'browser-full-match-rankings-peer'+index+'.png'),timeout:120000});
  // Keep a viewport capture as an independent compositor-level view. Some
  // WebGPU canvas screenshots have been incomplete even after the game-frame
  // settle gate, so retain both sources for final cross-peer review.
  await page.screenshot({path:path.join(outputDirectory,'browser-full-match-rankings-viewport-peer'+index+'.png'),timeout:120000});
 }
 const evidence={scope:'actual-ordinary-full-match-progress',elapsedMs:Date.now()-started,matchingFinalPolls:common.length,rankingSettlePolls,
  peers:trackers.map(tracker=>tracker.snapshot()),turnChecks,renderedRankingsVerified:false};
 fs.writeFileSync(path.join(outputDirectory,'browser-full-match-progress.json'),JSON.stringify(evidence,null,2)+'\n');
 console.log('Actual full configured match reached final rankings with '+common.length+' matching polls. Captured rankings still require visual inspection.');
 return evidence;
}
module.exports={DEFAULT_FULL_MATCH_TIMEOUT_MS,driveProductionFullMatch,fullMatchDeadline,fullMatchHarnessTimeout,fullMatchTimeoutFromEnvironment,mapViewOwnerIndex,m427AlternatingStrokeKey,pressM432ObstacleInput,pressMarioMedleyInput,rankingCaptureFrameReached,rankingCaptureTargetFrame,waitTwoGameFrames};
