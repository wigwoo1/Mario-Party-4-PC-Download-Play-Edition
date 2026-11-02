'use strict';
// Bounded local test controller: normal keyboard input only. The reviewer
// inspects captured screens before choosing the next command; no game-state writes.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {pressPostRankingAction}=require('./drive-browser-post-ranking.cjs');
async function inspectFreePlayMenu(pages,directory){
 const keys=new Set(['z','x','Enter','ArrowLeft','ArrowRight','ArrowUp','ArrowDown']);
 const timeout=Number(process.env.PARTYBOARD_TEST_MENU_REVIEW_TIMEOUT_MS||900000);
 assert.ok(Number.isInteger(timeout)&&timeout>=60000&&timeout<=1800000,'Bounded menu review timeout required');
 const seen=new Set(),deadline=Date.now()+timeout;
 async function snapshot(id){
  const states=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState())));
  for(const state of states){assert.equal(state.phase,'Running');assert.equal(state.input.state,'Running');assert.equal(state.input.stopReason,'None');}
  const records=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyInputObservations())));
  const common=records[0].filter(record=>records.every(peer=>peer.some(other=>other.frame===record.frame)));
  assert.ok(common.length>=30);
  for(const record of common)for(const peer of records)assert.deepEqual(peer.find(other=>other.frame===record.frame),record);
  const audio=await Promise.all(pages.map(page=>page.evaluate(()=>globalThis.testGameAudioMeters
    ?{meters:testGameAudioMeters.map(m=>m.snapshot()),errors:testGameAudioMeterErrors}:null)));
  for(const probe of audio)if(probe){assert.deepEqual(probe.errors,[]);assert.ok(probe.meters.every(m=>m.nonFinite===0));}
  for(let i=0;i<pages.length;i++)await pages[i].locator('#canvas').screenshot({path:path.join(directory,id+'-peer'+i+'.png')});
  fs.writeFileSync(path.join(directory,id+'-state.json'),JSON.stringify({states,matchingRecords:common.length,audio},null,2));
  console.log('Free Play inspection receipt '+id+' frames '+states.map(s=>s.frame).join('/')+' matching '+common.length);
 }
 async function wait(frames){
  const starts=await Promise.all(pages.map(page=>page.evaluate(()=>partyBoardApp.lobbyGameState().frame)));
  for(let i=0;i<pages.length;i++)await pages[i].waitForFunction(target=>{
   const s=partyBoardApp.lobbyGameState();if(s?.phase!=='Running'||s.input.state!=='Running')throw Error('Menu game stopped');return s.frame>=target;
  },starts[i]+frames,{timeout:30000});
 }
 await snapshot('menu-inspection-ready');
 while(Date.now()<deadline){
  const file=path.join(directory,'menu-command.json');
  if(!fs.existsSync(file)){await new Promise(resolve=>setTimeout(resolve,200));continue;}
  const command=JSON.parse(fs.readFileSync(file,'utf8'));
  assert.match(command.id,/^[a-zA-Z0-9_-]{1,60}$/);
  if(seen.has(command.id)){await new Promise(resolve=>setTimeout(resolve,200));continue;}
  assert.ok(['key','wait','capture','finish'].includes(command.action));
  if(command.action==='key'){
   assert.ok(keys.has(command.key));assert.ok(Number.isInteger(command.peer)&&command.peer>=0&&command.peer<pages.length);
   await pressPostRankingAction([pages[command.peer]],command.key);await wait(90);
  }else if(command.action==='wait'){
   assert.ok(Number.isInteger(command.frames)&&command.frames>0&&command.frames<=600);await wait(command.frames);
  }
  await snapshot(command.id);seen.add(command.id);
  if(command.action==='finish')return;
 }
 throw Error('Free Play inspection exceeded its bounded review window');
}
module.exports={inspectFreePlayMenu};
