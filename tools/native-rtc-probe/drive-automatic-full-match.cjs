'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const {createNativeMatchKeyboard}=require('./native-match-keyboard.cjs');
const {createBoundMatchKeyboard,browserMatchMapping}=require('./bound-match-keyboard.cjs');
const {driveMixedFullMatch}=require('./drive-mixed-full-match.cjs');
const {createNativePngCapture}=require('./native-png-capture.cjs');
function assignedSlot(owner){assert(Number.isInteger(owner)&&owner>=1&&owner<=4,'One assigned controller owner required');return owner-1;}
async function driveAutomaticFullMatch({pages,nativeHost,nativeObserver,send,outputDirectory,maxTurn=5,nativeCaptureRequest,python,nativeModeEntryCount}){
 assert.equal(typeof nativeModeEntryCount,'function','Native fresh-menu receipt counter required');
 const captureNative=createNativePngCapture({request:nativeCaptureRequest,python});
 const profile=(await import('../../web/control-bindings.mjs')).defaultControlBindings();
 const mapping=browserMatchMapping(profile),keyboard=createNativeMatchKeyboard(send);
 const browserPeers=[],listeners=[];
 try{
 for(const page of pages){
  const plan=await page.evaluate(()=>app.lobbyInputPlan());
  const slot=assignedSlot(plan.owner);
  await page.locator('#canvas').focus();
  let modeEntries=0;const listener=message=>{if(message.text().includes('Online Multiplayer entry: available'))modeEntries++;};page.on('console',listener);listeners.push({page,listener});
  browserPeers.push({slot,keyboard:createBoundMatchKeyboard(page.keyboard,mapping),
   releaseAll:async()=>{for(const key of new Set(Object.values(mapping)))await page.keyboard.up(key);},
   modeEntryCount:()=>modeEntries,
   capture:async filename=>{await page.locator('#canvas').screenshot({path:filename,timeout:10000});return {path:filename,sha256:crypto.createHash('sha256').update(fs.readFileSync(filename)).digest('hex')};},
   observeFrame:()=>page.evaluate(()=>app.lobbyGameState().frame),
   observeLottery:()=>page.evaluate(()=>({frame:app.lobbyGameState().frame,fields:app.lobbyLotteryFields()})),
   observe:()=>page.evaluate(()=>{const game=app.lobbyGameState();if(!game)throw Error('Actual automatic lobby game required');if(game.input.stopReason!=='None')throw Error('Automatic game input stopped: '+game.input.stopReason);return {frame:game.frame,progress:game.progress,input:game.input,observations:app.lobbyInputObservations(),lotteryFields:app.lobbyLotteryFields()};})});
 }
 const occupied=new Set(browserPeers.map(p=>p.slot)),missing=[0,1,2,3].filter(slot=>!occupied.has(slot));assert.equal(missing.length,1);
 const nativePeer={slot:missing[0],keyboard,releaseAll:()=>keyboard.releaseAll(),observe:()=>nativeObserver.observe(),capture:captureNative,modeEntryCount:nativeModeEntryCount};
 assert.equal(nativePeer.slot===0,nativeHost,'Native role must match assigned menu owner');
 const peers=[nativePeer,...browserPeers].sort((a,b)=>a.slot-b.slot);
 const inventory=JSON.parse(fs.readFileSync(path.resolve(__dirname,'../../../../outputs/validation/browser-usa-scene-inventory.json'),'utf8'));
 return await driveMixedFullMatch({peers,inventory,outputDirectory,maxTurn,board:0,verifyReturn:true});
 }finally{for(const {page,listener}of listeners)page.off('console',listener);}
}
module.exports={assignedSlot,driveAutomaticFullMatch};
