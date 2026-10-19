'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
// Ordinary input only; no scene selection, state injection or save overrides.
async function driveBrowserPostRanking({pages,inventory,outputDirectory,timeoutMs=180000}){
 const find=name=>inventory.overlays.find(row=>row.name.toLowerCase()===name)?.overlay;
 const awards=find('mstory3dll'),returns=[find('mentdll'),find('modeseldll')].filter(Number.isInteger);
 assert.ok(Number.isInteger(awards)&&returns.length===2,'Authoritative result and menu overlays required');
 const read=()=>Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState())));
 const initial=await read();
 assert.ok(initial.every(state=>state?.input.state==='Running'&&state.progress.overlay===awards&&state.progress.event===1),'Every peer must begin on normal final rankings');
 const history=[],inputs=[],start=Date.now();let lastInputFrame=-Infinity;
 while(Date.now()-start<timeoutMs){
  const states=await read();assert.ok(states.every(state=>state?.input.state==='Running'),'Post-ranking peers must remain running');
  history.push(states.map(state=>({frame:state.frame,progress:state.progress})));
  fs.writeFileSync(path.join(outputDirectory,'browser-post-ranking-observation.json'),JSON.stringify({history,inputs},null,2)+'\n');
  if(states.every(state=>returns.includes(state.progress.overlay))){
   assert.ok(states.every(state=>state.progress.overlay===states[0].progress.overlay),'All peers must return to the same menu');
   const frame=states[0].frame;
   await Promise.all(pages.map(page=>page.waitForFunction(frame=>partyBoardApp.lobbyGameState()?.frame>=frame+180,frame,{timeout:120000})));
   const settled=await read();assert.ok(settled.every(state=>state.progress.overlay===states[0].progress.overlay),'Returned menu must remain stable');
   for(const [index,page] of pages.entries())await page.locator('#canvas').screenshot({path:path.join(outputDirectory,'browser-post-ranking-menu-peer'+index+'.png')});
   const result={status:'execution passed; visual inspection required',scope:'Normal Start/Skip inputs leave final standings and reach a stable menu; no game state injection',initial:initial.map(state=>({frame:state.frame,progress:state.progress})),settled:settled.map(state=>({frame:state.frame,progress:state.progress})),inputs,elapsedMs:Date.now()-start,renderedMenuReviewed:false,noCardScreenVisuallyVerified:false};
   fs.writeFileSync(path.join(outputDirectory,'browser-post-ranking-return.json'),JSON.stringify(result,null,2)+'\n',{flag:'wx'});return result;
  }
  if(states.every(state=>state.progress.overlay===awards)&&states[0].frame>=lastInputFrame+180){
   assert.ok(inputs.length<4,'Four ordinary Skip attempts did not leave rankings');
   inputs.push({key:'Enter',frames:states.map(state=>state.frame)});lastInputFrame=states[0].frame;
   await Promise.all(pages.map(page=>page.keyboard.press('Enter',{delay:120})));
  }
  await new Promise(resolve=>setTimeout(resolve,500));
 }
 throw Error('Normal post-ranking return did not reach a known menu within its bounded lifetime');
}
module.exports={driveBrowserPostRanking};
