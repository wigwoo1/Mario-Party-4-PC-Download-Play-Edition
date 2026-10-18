'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {createFullMatchProgress}=require('../full-match-progress.cjs');
const {circularKeyboardStick}=require('../circular-keyboard-stick.cjs');
const {sweepKeyboardStick}=require('../sweep-keyboard-stick.cjs');
const {variedKeyboardStick}=require('../varied-keyboard-stick.cjs');
const {turnSetupAction,setupActionObserved,retryObservedSetupAction,setupMenuOwnerIndex}=require('../party-turn-setup.cjs');
const {m427AlternatingStrokeKey}=require('../drive-production-full-match.cjs');
const {pressMixedMarioMedleyInput,pressMixedM432ObstacleInput}=require('./mixed-minigame-inputs.cjs');
function matchingRecords(peers){
 const maps=peers.map(records=>new Map(records.map(record=>[String(record.frame),{...record,frame:String(record.frame)}])));
 const common=[...maps[0].keys()].filter(frame=>maps.every(map=>map.has(frame)));
 for(const frame of common)for(const map of maps)assert.deepEqual(map.get(frame),maps[0].get(frame),'Mixed input/checkpoint/RNG disagreement at '+frame);
 return common.map(frame=>maps[0].get(frame));
}
function createExtendedObservationGate(interval=600000){
 let previousKey=null,nextCaptureAt=0;
 return (states,now)=>{
  const key=JSON.stringify(states.map(state=>state.progress));
  if(key!==previousKey){previousKey=key;nextCaptureAt=now+interval;return false;}
  if(now<nextCaptureAt)return false;
  nextCaptureAt=now+interval;return true;
 };
}
async function driveMixedFullMatch({peers,inventory,outputDirectory,board=0,maxTurn=20,onVerifiedTurn,onSetupVerified}){
 assert(Number.isInteger(board)&&board>=0&&board<=5,'Main board index must be 0 through 5');
 assert(Number.isInteger(maxTurn)&&maxTurn>=5&&maxTurn<=50&&maxTurn%5===0,'Party length must be 5 through 50 turns in five-turn steps');
 assert.equal(peers.length,4);fs.mkdirSync(outputDirectory,{recursive:true});
 assert.deepEqual(peers.map(peer=>peer.slot).sort((a,b)=>a-b),[0,1,2,3],'One actual lobby owner per controller port required');
 const boardName='w'+String(board+1).padStart(2,'0')+'dll';
 const boardOverlay=inventory.overlays.find(item=>item.name.toLowerCase()===boardName)?.overlay;
 const awards=inventory.overlays.find(item=>item.name.toLowerCase()==='mstory3dll')?.overlay;
 const instructions=inventory.overlays.find(item=>item.name.toLowerCase()==='instdll')?.overlay;
 const bootOverlay=inventory.overlays.find(item=>item.name.toLowerCase()==='bootdll')?.overlay;
 assert([boardOverlay,awards,instructions,bootOverlay].every(Number.isInteger));
 const deadline=Date.now()+21600000,sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));
 let nextProgressReport=0,progressStage='ordinary-party-setup',previousProgress=null;
 const inputFrameSpans=[];
 const extendedObservationDue=createExtendedObservationGate();
 const snapshots=async()=>{
  const observed=await Promise.all(peers.map(async peer=>({
   ...await peer.observe(),input:{localPadSlots:Array.from({length:4},(_,slot)=>slot===peer.slot?0:null)}
  })));
  if(Date.now()>=nextProgressReport){
   nextProgressReport=Date.now()+30000;
   const monotonicMs=performance.now(),elapsedMs=previousProgress?monotonicMs-previousProgress.monotonicMs:null;
   const progress={stage:progressStage,observedAt:new Date().toISOString(),monotonicMs,
    timing:{elapsedMs,framesPerSecond:elapsedMs>0?observed.map((state,index)=>(state.frame-previousProgress.frames[index])*1000/elapsedMs):null},
    peers:observed.map(state=>({frame:state.frame,progress:state.progress}))};
   previousProgress={monotonicMs,frames:observed.map(state=>state.frame)};
   fs.appendFileSync(path.join(outputDirectory,'mixed-full-match-timing.jsonl'),JSON.stringify(progress)+'\n');
   fs.writeFileSync(path.join(outputDirectory,'mixed-full-match-latest-state.json'),JSON.stringify(progress,null,2)+'\n');
   fs.writeFileSync(path.join(outputDirectory,'mixed-full-match-latest-diagnostics.json'),JSON.stringify({
    observedAt:progress.observedAt,peers:observed,inputFrameSpans:[...inputFrameSpans]
   },null,2)+'\n');
   process.stdout.write(JSON.stringify(progress)+'\n');
   if(progressStage==='full-match'&&extendedObservationDue(observed,Date.now())){
    const tag='mixed-extended-frame-'+observed[0].frame;
    const captures=await Promise.allSettled(peers.map((peer,index)=>peer.capture?
     Promise.resolve().then(()=>peer.capture(path.join(outputDirectory,tag+'-peer-'+index+'.png'))):Promise.resolve(null)));
    fs.writeFileSync(path.join(outputDirectory,tag+'.json'),JSON.stringify({
     ...progress,captures:captures.map((result,index)=>({peer:index,available:typeof peers[index].capture==='function',
      captured:typeof peers[index].capture==='function'&&result.status==='fulfilled',error:result.status==='rejected'?String(result.reason):null}))
    },null,2)+'\n');
   }
  }
  return observed;
 };
 const waitFrames=async(peer,count)=>{
  const readFrame=peer.observeFrame?()=>peer.observeFrame():async()=>(await peer.observe()).frame;
  const start=await readFrame(),startedAt=Date.now(),end=startedAt+120000;
  let lastObservationAt=startedAt,maxObservationGapMs=0;
  let observedFrame=start;
  while((observedFrame=await readFrame())<start+count){
   const observationAt=Date.now();
   maxObservationGapMs=Math.max(maxObservationGapMs,observationAt-lastObservationAt);
   lastObservationAt=observationAt;
   if(Date.now()>=end){
    // Preserve actual peer states before cleanup closes the transports.
    const captured=await Promise.allSettled(peers.map(candidate=>candidate.observe()));
    fs.writeFileSync(path.join(outputDirectory,'mixed-full-match-stalled-state.json'),JSON.stringify({
     observedAt:new Date().toISOString(),startFrame:start,requestedFrames:count,
     timing:{startedAt:new Date(startedAt).toISOString(),elapsedMs:Date.now()-startedAt,
      maxObservationGapMs,timeoutMs:120000,lastObservedFrame:observedFrame},
     peers:captured.map(result=>result.status==='fulfilled'?{state:result.value}:{error:String(result.reason)})
    },null,2)+'\n');
    assert.fail('Mixed simulation stopped advancing');
   }
   // Short stick steps must not include a fixed three-frame polling delay.
   // Full diagnostics are still captured by snapshots and on failure.
   await sleep(count<=2?1:50);
  }
  inputFrameSpans.push({peer:peers.indexOf(peer),startFrame:start,endFrame:observedFrame,requestedFrames:count,observedFrames:observedFrame-start});
  if(inputFrameSpans.length>256)inputFrameSpans.shift();
 };
 let states,partySetupTelemetrySeen=false,partyTurnsSelected=false;
 const setupActions=[];
 const settleSetupInput=()=>Promise.all(peers.map(peer=>waitFrames(peer,30)));
 const observedSetupAction=async decision=>retryObservedSetupAction(decision,{
  press:(owner,key)=>peers[owner].keyboard.press(key,{delay:120}),
  waitForChange:async expected=>{
   const until=Date.now()+5000;
   while(Date.now()<until){
    const current=await snapshots();
    if(setupActionObserved(expected,current))return true;
    await sleep(20);
   }
   return false;
  },
  readStates:snapshots,waitFrames:settleSetupInput,maxAttempts:3,
  onAttempt:entry=>{
   setupActions.push({at:new Date().toISOString(),...entry});
   fs.writeFileSync(path.join(outputDirectory,'mixed-party-setup-actions.json'),JSON.stringify(setupActions,null,2)+'\n');
  },
 });
 try{
  // Traverse the ordinary boot and Party setup using each assigned player's A.
  const setupDeadline=Date.now()+900000;
  let nextSetupConfirmAt=0,boardNavigationPending=false,setupDisagreementSince=0;
  do{
   states=await snapshots();
   assert(Date.now()<setupDeadline,'Ordinary mixed Party setup timed out');
   if(states.every(state=>state.progress.overlay===boardOverlay&&state.progress.turn===1)){
    assert(maxTurn===20||partyTurnsSelected,'Requested non-default turn length was not selected in the actual party setup menu');
    assert(!partySetupTelemetrySeen||partyTurnsSelected,'Observed party setup menu did not confirm the requested turn length');
    assert(states.every(state=>state.progress.board===board&&state.progress.maxTurn===maxTurn),'Selected board or turn count differs from requested match');break;
   }
   const setupValues=states.map(state=>state.progress);
   const setupVisible=setupValues.every(progress=>progress.partySetupRow>=0&&progress.partySetupTurns>=0);
   if(!setupVisible&&setupValues.some(progress=>progress.partySetupRow>=0)){
    await sleep(20);continue;
   }
   const setupAgrees=setupValues.every(progress=>progress.partySetupRow===setupValues[0].partySetupRow&&
    progress.partySetupTurns===setupValues[0].partySetupTurns&&progress.partySetupPort===setupValues[0].partySetupPort);
   if(setupVisible&&!setupAgrees){
    if(!setupDisagreementSince)setupDisagreementSince=Date.now();
    assert(Date.now()-setupDisagreementSince<5000,'Party turn selector remained different across peers');
    await sleep(20);continue;
   }
   setupDisagreementSince=0;
   if(partyTurnsSelected&&setupVisible){
    assert.equal(setupValues[0].partySetupTurns,maxTurn,'Selected Party length changed before starting the board');
    const port=setupValues[0].partySetupPort,row=setupValues[0].partySetupRow;
    assert(Number.isInteger(port)&&port>=0&&port<4&&row>=0&&row<=4,'Valid Party setup owner and row required');
    const owner=setupMenuOwnerIndex(states,port);
    if(row<4){
     await observedSetupAction({owner,key:'z',kind:'cursor',selected:false,row,turns:maxTurn,port});
    }else if(Date.now()>=nextSetupConfirmAt){
     // Last-row A opens the settings confirmation; the following A accepts it.
     // The row remains four while that prompt is open, so use a frame window.
     await peers[owner].keyboard.press('z',{delay:120});
     await settleSetupInput();nextSetupConfirmAt=Date.now()+400;
    }
    await sleep(50);continue;
   }
   const setupAction=turnSetupAction(states,maxTurn);
   if(setupAction){
    partySetupTelemetrySeen=true;
    if(setupAction.key){await observedSetupAction(setupAction);continue;}
    // Stick auto-repeat and cross-peer frame skew can briefly show the target
    // while a prior key is still settling. Confirm the same value again before A.
    await settleSetupInput();
    const settled=await snapshots();
    if(!settled.every(state=>state.progress.partySetupRow===setupAction.row&&
     state.progress.partySetupTurns===maxTurn&&state.progress.partySetupPort===setupAction.port))continue;
    if(!partyTurnsSelected){partyTurnsSelected=true;console.log('Actual party setup selected '+setupAction.turns+' turns on controller port '+setupAction.port+'.');}
    await observedSetupAction({...setupAction,key:'z',kind:'cursor',selected:false});continue;
   }
   assert(!states.some(state=>state.progress.turn===1&&state.progress.overlay!==boardOverlay&&/^w\d+dll$/i.test(inventory.overlays.find(item=>item.overlay===state.progress.overlay)?.name||'')),'Ordinary setup entered a different board');
   assert(Date.now()<setupDeadline,'Ordinary mixed Party setup timed out');
   // Peers can cross the correct board boundary on adjacent observations.
   // Wait for all four instead of treating the first arrival as a wrong board
   // or sending another setup confirmation during that transition.
   if(states.some(state=>state.progress.overlay===boardOverlay&&state.progress.turn===1)){
    await sleep(20);continue;
   }
   // The title's Press Start gate accepts START specifically (bootDll/main.c).
   // Use the actual overlay to select its ordinary input, then A for setup.
   if(states.every(state=>state.progress.menuBoard>=0)){
    const selected=states[0].progress.menuBoard,port=states[0].progress.menuPort;
    assert(states.every(state=>state.progress.menuBoard===selected&&state.progress.menuPort===port),'Board selection menu disagrees across peers');
    assert(Number.isInteger(port)&&port>=0&&port<4,'Board menu input owner required');
    const order=[1,2,0,3,4,5];
    assert(order.includes(selected),'Unknown board cursor');
    boardNavigationPending=selected!==board;
    await peers[port].keyboard.press(selected===board?'z':order.indexOf(selected)<order.indexOf(board)?'d':'a',{delay:100});
   }
   else if(states.some(state=>state.progress.menuBoard>=0)){
    // Wait for all peers to observe an input-ready menu before sending A.
   }
   else if(states.every(state=>state.progress.overlay===bootOverlay))await peers[0].keyboard.press('Enter',{delay:250});
   else if(!boardNavigationPending&&Date.now()>=nextSetupConfirmAt){
    await Promise.all(peers.map(peer=>peer.keyboard.press('z')));
    nextSetupConfirmAt=Date.now()+1000;
   }
   // Observe menu readiness between confirmation pulses instead of missing
   // the transition during a one-second sleep and confirming the default.
   await sleep(50);
  }while(true);
  const setupEvidence={scope:'actual-mixed-party-setup',board,maxTurn,
   peers:states.map(state=>({frame:state.frame,progress:state.progress})),actions:setupActions};
  fs.writeFileSync(path.join(outputDirectory,'mixed-party-setup-verified.json'),JSON.stringify(setupEvidence,null,2)+'\n');
  if(onSetupVerified)await onSetupVerified(setupEvidence);
  progressStage='full-match';
  const trackers=peers.map(()=>createFullMatchProgress({board,maxTurn,awardOverlay:awards}));
  const verifiedTurns=new Set(),boardEntries=peers.map(()=>null);let direction=0,shot=0,riverRidersStroke=0;
  let lastFrames=null,lastAdvanceAt=Date.now();
  while(true){
   assert(Date.now()<deadline,'Mixed full match exceeded six hours');states=await snapshots();
   const frames=states.map(state=>state.frame);
   if(!lastFrames||frames.some((frame,index)=>frame!==lastFrames[index])){
    lastFrames=frames;lastAdvanceAt=Date.now();
   }else if(Date.now()-lastAdvanceAt>=120000){
    fs.writeFileSync(path.join(outputDirectory,'mixed-full-match-stalled-state.json'),JSON.stringify({
     observedAt:new Date().toISOString(),lastAdvanceAt,peers:states.map(state=>({state}))
    },null,2)+'\n');
    assert.fail('Mixed simulation stopped advancing in main gameplay loop');
   }
   const complete=states.map((state,index)=>trackers[index].observe(state.progress));
   if(states.every(state=>state.progress.turn===states[0].progress.turn)&&!verifiedTurns.has(states[0].progress.turn)){
    const records=matchingRecords(states.map(state=>state.observations));
    if(records.length>=30){
     const turn=states[0].progress.turn;verifiedTurns.add(turn);
     fs.writeFileSync(path.join(outputDirectory,'mixed-full-match-turn-'+turn+'.json'),JSON.stringify({turn,peers:states,matchingPolls:records.length},null,2)+'\n');
     if(onVerifiedTurn)await onVerifiedTurn(turn);
     // Capture actual browser canvases alongside each turn checkpoint. A
     // diagnostic capture failure must not stop an otherwise advancing match.
     const captures=await Promise.allSettled(peers.map((peer,index)=>peer.capture?
      Promise.resolve().then(()=>peer.capture(path.join(outputDirectory,'mixed-turn-'+turn+'-peer-'+index+'.png'))):Promise.resolve(null)));
     fs.writeFileSync(path.join(outputDirectory,'mixed-turn-'+turn+'-captures.json'),JSON.stringify(captures.map((result,index)=>({
      peer:index,available:typeof peers[index].capture==='function',captured:typeof peers[index].capture==='function'&&result.status==='fulfilled',
      error:result.status==='rejected'?String(result.reason):null
     })),null,2)+'\n');
    }
   }
   if(complete.every(Boolean)){
    const rankingFrames=states.map(state=>state.frame),settleDeadline=Date.now()+120000;
    do{
     states=await snapshots();
     assert(states.every(state=>state.progress.overlay===awards&&state.progress.event===1),'Every mixed peer must retain final rankings');
     if(states.every((state,index)=>state.frame>=rankingFrames[index]+600))break;
     assert(Date.now()<settleDeadline,'Mixed final rankings failed to settle');await sleep(100);
    }while(true);
    const common=matchingRecords(states.map(state=>state.observations));
    assert(common.length>=30,'Matching mixed final ranking polls required');
    fs.writeFileSync(path.join(outputDirectory,'mixed-full-match-ranking-inputs.json'),JSON.stringify({rankingFrames,settlePolls:600,peers:states,matchingPolls:common.length},null,2)+'\n');
    break;
   }
   const same=states.every(state=>state.progress.overlay===states[0].progress.overlay);
   const scene=same?inventory.overlays.find(item=>item.overlay===states[0].progress.overlay)?.name.toLowerCase():'';
   if(states.every(state=>state.progress.mapView)){
    const port=states[0].progress.mapViewPort;
    assert(Number.isInteger(port)&&port>=0&&port<4&&states.every(state=>state.progress.mapViewPort===port),'Actual assigned map-view port required');
    // This harness assigns peers in the same global-port order: host, native
    // guest1, then browser guest2/guest3. Their local physical lane is zero.
    await peers[port].keyboard.press('x',{delay:250});
   }
   else if(same&&states[0].progress.overlay===instructions)await peers[0].keyboard.press('Enter',{delay:250});
   else if(scene==='m443dll')await Promise.all(peers.map(async peer=>{
    await peer.keyboard.down('e');try{await peer.keyboard.press('z');await sleep(880);}finally{await peer.keyboard.up('e');}
   }));
   else if(scene==='m444dll'){
    const pull=[112,80,64,96][shot++%4];await Promise.all(peers.map(async peer=>{
     await peer.keyboard.down('s');try{await waitFrames(peer,pull);await peer.keyboard.down('z');try{await waitFrames(peer,4);}finally{await peer.keyboard.up('z');}}finally{await peer.keyboard.up('s');}
    }));
   }else if(scene==='m428dll')await Promise.all(peers.map(peer=>peer.keyboard.press('w',{delay:1000})));
   else if(scene==='m405dll')await Promise.all(peers.map(peer=>pressMixedMarioMedleyInput(peer,waitFrames)));
   else if(scene==='m427dll'){
    const key=m427AlternatingStrokeKey(riverRidersStroke++);
    await Promise.all(peers.map(peer=>peer.keyboard.press(key,{delay:120})));
   }else if(scene==='m432dll'){
    const key=['w','a','s','d'][direction++%4];
    await Promise.all(peers.map(peer=>pressMixedM432ObstacleInput(peer,waitFrames,key)));
   }
   else{
    await Promise.all(peers.map((peer,index)=>complete[index]?Promise.resolve():peer.keyboard.press('z')));
    if(/^w\d+dll$/.test(scene))await Promise.all(peers.map((peer,index)=>{
     const key=[states[index].progress.turn,states[index].progress.player].join('-');
     if(boardEntries[index]?.key!==key)boardEntries[index]={key,frame:states[index].frame,sweeps:0};
     const scratching=states[index].lotteryFields?.[0]===1;
     const stick=scratching?variedKeyboardStick:states[index].frame-boardEntries[index].frame>=7200&&boardEntries[index].sweeps++%2===0?sweepKeyboardStick:circularKeyboardStick;
     return stick({hold:(key,down)=>peer.keyboard[down?'down':'up'](key),waitFrames:count=>waitFrames(peer,count),
      shouldContinue:async()=>{
       const observation=await (peer.observeLottery?peer.observeLottery():peer.observe());
       return (observation.fields||observation.lotteryFields)?.[0]===1;
      }});
    }));
    else await Promise.all(peers.map((peer,index)=>complete[index]?Promise.resolve():peer.keyboard.press(['w','d','s','a'][direction%4],{delay:250})));
    direction++;
   }
   await sleep(1000);
  }
  const evidence={scope:'actual-mixed-full-match-progress',peers:trackers.map(tracker=>tracker.snapshot()),verifiedTurns:[...verifiedTurns],renderedRankingsVerified:false};
  fs.writeFileSync(path.join(outputDirectory,'mixed-full-match-progress.json'),JSON.stringify(evidence,null,2)+'\n');
  assert.equal(verifiedTurns.size,maxTurn,'Every configured turn requires matching mixed-platform records');
  return evidence;
 }catch(error){
  // Native rejection can end the match before waitFrames reaches its deadline.
  // Retain surviving browser diagnostics before teardown closes their channels.
  const captured=await Promise.allSettled(peers.map(peer=>peer.observe()));
  fs.writeFileSync(path.join(outputDirectory,'mixed-full-match-failure-state.json'),JSON.stringify({
   observedAt:new Date().toISOString(),error:String(error),inputFrameSpans:[...inputFrameSpans],
   peers:captured.map(result=>result.status==='fulfilled'?{state:result.value}:{error:String(result.reason)})
  },null,2)+'\n');
  throw error;
 }finally{await Promise.allSettled(peers.map(peer=>peer.releaseAll()));}
}
module.exports={driveMixedFullMatch,matchingRecords,createExtendedObservationGate};
