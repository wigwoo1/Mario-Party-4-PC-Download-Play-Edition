'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
async function pressPostRankingAction(pages,key){
 // Use emulated frames: a short wall-clock tap can miss a busy online poll.
 await Promise.all(pages.map(async page=>{
  await page.locator('#canvas').focus();
  await page.keyboard.down(key);
  try{
   const frame=await page.evaluate(()=>partyBoardApp.lobbyGameState().frame);
   await page.waitForFunction(before=>partyBoardApp.lobbyGameState()?.frame>=before+3,frame,{timeout:120000});
  }finally{await page.keyboard.up(key);}
 }));
}
// Ordinary input only; no scene selection, state injection or save overrides.
async function driveBrowserPostRanking({pages,inventory,outputDirectory,timeoutMs=180000,verifyInteractiveMenu=false}){
 const find=name=>inventory.overlays.find(row=>row.name.toLowerCase()===name)?.overlay;
 const awards=find('mstory3dll'),returns=[find('mentdll'),find('modeseldll')].filter(Number.isInteger);
 assert.ok(Number.isInteger(awards)&&returns.length===2,'Authoritative result and menu overlays required');
 const read=()=>Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState())));
 const initial=await read();
 assert.ok(initial.every(state=>state?.input.state==='Running'&&state.progress.overlay===awards&&state.progress.event===1),'Every peer must begin on normal final rankings');
 const history=[],inputs=[],start=Date.now();let lastInputFrame=-Infinity,skipAttempts=0;
 const receipts=pages.map(()=>[]),handlers=pages.map((page,index)=>{
  const handler=message=>{if(message.text().includes('Online Multiplayer entry: available'))receipts[index].push(message.text());};
  page.on('console',handler);return handler;
 });
 try{
 while(Date.now()-start<timeoutMs){
  const states=await read();assert.ok(states.every(state=>state?.input.state==='Running'),'Post-ranking peers must remain running');
  history.push(states.map(state=>({frame:state.frame,progress:state.progress})));
  fs.writeFileSync(path.join(outputDirectory,'browser-post-ranking-observation.json'),JSON.stringify({history,inputs},null,2)+'\n');
  if(states.every(state=>returns.includes(state.progress.overlay))){
   assert.ok(states.every(state=>state.progress.overlay===states[0].progress.overlay),'All peers must return to the same menu');
   const frame=states[0].frame;
   const settle=verifyInteractiveMenu?420:180;
   await Promise.all(pages.map(page=>page.waitForFunction(({frame,settle})=>partyBoardApp.lobbyGameState()?.frame>=frame+settle,{frame,settle},{timeout:120000})));
   const settled=await read();assert.ok(settled.every(state=>state.progress.overlay===states[0].progress.overlay),'Returned menu must remain stable');
   for(const [index,page] of pages.entries())await page.locator('#canvas').screenshot({path:path.join(outputDirectory,'browser-post-ranking-menu-peer'+index+'.png')});
   if(verifyInteractiveMenu){
    assert.equal(states[0].progress.overlay,find('modeseldll'),'Interactive return check requires normal mode selection');
    await pages[0].locator('#canvas').focus();
    if(!receipts.every(rows=>rows.length)){
     inputs.push({key:'z',purpose:'confirm normal envelope introduction',frames:settled.map(state=>state.frame)});
     await pages[0].keyboard.press('z',{delay:80});
    }
    const menuDeadline=Date.now()+60000;
    while(!receipts.every(rows=>rows.length)){
     assert.ok(Date.now()<menuDeadline,'Every peer must report the fresh interactive mode-selection loop');
     assert.ok((await read()).every(state=>state.progress.overlay===find('modeseldll')),'Mode selection changed before its interactive receipt');
     await new Promise(resolve=>setTimeout(resolve,100));
    }
    for(const [key,label]of [[null,'selection'],['ArrowRight','right'],['ArrowLeft','return']]){
     const before=await read();
     if(key){inputs.push({key,purpose:'normal mode navigation',frames:before.map(state=>state.frame)});await pages[0].keyboard.press(key,{delay:80});}
     await Promise.all(pages.map((page,index)=>page.waitForFunction(frame=>partyBoardApp.lobbyGameState()?.frame>=frame+45,before[index].frame,{timeout:120000})));
     assert.ok((await read()).every(state=>state.progress.overlay===find('modeseldll')),'Navigation must remain in mode selection');
     for(const [index,page]of pages.entries())await page.locator('#canvas').screenshot({path:path.join(outputDirectory,'browser-post-ranking-'+label+'-peer'+index+'.png')});
    }
   }
   const result={status:'execution passed; visual inspection required',scope:'Normal B/Back and Start/Skip inputs leave final standings or detailed results and reach a known menu overlay; interactive navigation requires separate image review',initial:initial.map(state=>({frame:state.frame,progress:state.progress})),settled:settled.map(state=>({frame:state.frame,progress:state.progress})),inputs,interactiveMenuExecutionCompleted:verifyInteractiveMenu,modeSelectionReceipts:receipts,elapsedMs:Date.now()-start,renderedMenuReviewed:false,noCardScreenVisuallyVerified:false};
   fs.writeFileSync(path.join(outputDirectory,'browser-post-ranking-return.json'),JSON.stringify(result,null,2)+'\n',{flag:'wx'});return result;
  }
  if(states.every(state=>state.progress.overlay===awards)&&states[0].frame>=lastInputFrame+180){
   assert.ok(skipAttempts++<4,'Four ordinary Back/Skip attempts did not leave rankings');
   // result.c: detailed results only accepts B to return; Start is ignored.
   // B is harmless on the summary. Normalize either observed screen before
   // Skip, allowing its wipe to finish without changing game state directly.
   inputs.push({key:'x',purpose:'leave detailed results if open',frames:states.map(state=>state.frame)});
   await pressPostRankingAction(pages,'x');
   const backed=await read();
   await Promise.all(pages.map((page,index)=>page.waitForFunction(frame=>partyBoardApp.lobbyGameState()?.frame>=frame+90,backed[index].frame,{timeout:120000})));
   const beforeSkip=await read();
   assert.ok(beforeSkip.every(state=>state?.input.state==='Running'),'Back must retain running input');
   if(beforeSkip.every(state=>state.progress.overlay===awards)){
    inputs.push({key:'Enter',purpose:'skip final standings',frames:beforeSkip.map(state=>state.frame)});lastInputFrame=beforeSkip[0].frame;
    await pressPostRankingAction(pages,'Enter');
   }
  }
  await new Promise(resolve=>setTimeout(resolve,500));
 }
 throw Error('Normal post-ranking return did not reach a known menu within its bounded lifetime');
 }finally{for(const [index,page]of pages.entries())page.off('console',handlers[index]);}
}
module.exports={driveBrowserPostRanking,pressPostRankingAction};
