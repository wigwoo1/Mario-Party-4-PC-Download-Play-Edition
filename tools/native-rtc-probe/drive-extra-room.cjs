'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto'),{promisify}=require('node:util'),{execFile}=require('node:child_process');
const {matchingRecords}=require('./drive-mixed-full-match.cjs');
const {requestNativeFrame}=require('./native-frame-capture.cjs');
const run=promisify(execFile),sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms)),sha=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
async function driveExtraRoom({page,outputDirectory,nativeCaptureRequest,observeNative,normalize,budgetMs,requiredOverlay=67}){
 assert.ok([65,67].includes(requiredOverlay),'Select the actual Mushroom Medic or Bob-omb X-ing overlay');
 const python=process.env.PARTYBOARD_TEST_PYTHON;assert.ok(python&&fs.existsSync(python),'Explicit test Python/Pillow runtime required');
 const directory=path.join(outputDirectory,'extra-room');fs.mkdirSync(directory);const started=Date.now(),overlays=new Set(),captures=[];
 let firstSaved,lastSaved=-1,archived=0,lastReport=0,lastCommand;
 const snapshot=async()=>{
  assert.ok(Date.now()-started<budgetMs,'Extra Room observation budget exceeded');
  const [native,browser]=await Promise.all([observeNative(),page.evaluate(()=>({game:app.lobbyGameState(),records:app.lobbyInputObservations(),plan:app.lobbyInputPlan()}))]);
  assert.ok(browser.game,'Running production browser session required');assert.equal(browser.plan.owner,1,'Browser Host must own menu port0');
  assert.equal(browser.game.input.stopReason,'None','Actual input queue stopped');assert.ok(!native.error,'Native session error: '+native.error);
  const shared=matchingRecords([native.records.map(normalize),browser.records.map(normalize)]).sort((a,b)=>Number(a.frame)-Number(b.frame));
  const fresh=shared.filter(row=>Number(row.frame)>lastSaved);
  for(const row of fresh){const frame=Number(row.frame);if(lastSaved>=0)assert.equal(frame,lastSaved+1,'Completed-poll archive has an observation gap');firstSaved??=frame;lastSaved=frame;archived++;}
  if(fresh.length)fs.appendFileSync(path.join(directory,'completed-inputs.jsonl'),fresh.map(row=>JSON.stringify(row)).join('\n')+'\n');
  const overlay=browser.game.progress.overlay;overlays.add(overlay);
  const state={at:new Date().toISOString(),game:browser.game,nativeAccepted:native.accepted,matchingRecords:shared.length,archived,firstSaved,lastSaved,visitedOverlays:[...overlays]};
  fs.writeFileSync(path.join(directory,'latest.json'),JSON.stringify(state,null,2));
  if(Date.now()-lastReport>15000){lastReport=Date.now();console.log(JSON.stringify({stage:'mixed-extra-room',frame:state.game.frame,overlay,archived,matchingRecords:shared.length}));}
  return state;
 };
 const settle=async frames=>{const first=(await snapshot()).game.frame;while((await snapshot()).game.frame<first+frames)await sleep(100);};
 const press=async(key,hold=80,frames=180)=>{await page.locator('#canvas').focus();await page.keyboard.press(key,{delay:hold});await settle(frames);};
 const capture=async id=>{
  assert.match(id,/^[a-zA-Z0-9_-]{1,64}$/);const browserFile=path.join(directory,id+'-browser.png'),nativeFile=path.join(directory,id+'-native.ppm'),nativePng=path.join(directory,id+'-native.png');
  const before=await snapshot();assert.ok(!fs.existsSync(browserFile)&&!fs.existsSync(nativeFile)&&!fs.existsSync(nativePng));
  const [,native]=await Promise.all([page.locator('#canvas').screenshot({path:browserFile,timeout:15000}),requestNativeFrame(nativeCaptureRequest,15000)]);
  for(let attempt=0;;attempt++){try{fs.renameSync(native.path,nativeFile);break;}catch(error){if(attempt>=20)throw error;await sleep(10);}}
  await run(python,['-c','from PIL import Image; import sys; image=Image.open(sys.argv[1]); assert image.format=="PPM"; target=open(sys.argv[2],"xb"); image.save(target,format="PNG"); target.close(); assert Image.open(sys.argv[2]).convert("RGB").tobytes()==image.convert("RGB").tobytes()',nativeFile,nativePng],{windowsHide:true,timeout:15000});
  const item={id,before,after:await snapshot(),browser:{path:browserFile,sha256:sha(browserFile)},native:{path:nativePng,sha256:sha(nativePng),rawPath:nativeFile,rawSha256:sha(nativeFile),pixelsUnchanged:true}};
  captures.push(item);fs.writeFileSync(path.join(directory,'captures.json'),JSON.stringify(captures,null,2));return item;
 };
 try{
  assert.equal(await page.locator('#canvas').isVisible(),true,'Production game canvas should be visible after Start');
  while((await snapshot()).game.frame<1300)await sleep(100);
  await capture('title');await press('Enter',80,480);assert.equal((await snapshot()).game.progress.overlay,74);await capture('mode-intro');
  await press('z');for(let index=0;index<5;index++)await press('ArrowRight',80,90);await capture('extra-card');
  await press('z',80,1000);assert.equal((await snapshot()).game.progress.overlay,75,'Normal menus must enter Extra Room');await capture('extra-open');
  console.log(JSON.stringify({stage:'mixed-extra-room-controls',directory,instructions:'extra-room/command.json accepts press/capture/finish; only normal browser Host controls are used.'}));
  while(true){
   await snapshot();const file=path.join(directory,'command.json');
   if(fs.existsSync(file)){
    assert.ok(fs.statSync(file).size<=4096);const command=JSON.parse(fs.readFileSync(file));
    if(command.id!==lastCommand){
     assert.match(command.id,/^[a-zA-Z0-9_-]{1,64}$/);lastCommand=command.id;
     if(command.action==='press'){
      assert.ok(['Enter','z','x','c','v','ArrowUp','ArrowDown','ArrowLeft','ArrowRight','q','e','r'].includes(command.key));
      const hold=command.hold??80,frames=command.frames??180;assert.ok(Number.isInteger(hold)&&hold>=20&&hold<=2000);assert.ok(Number.isInteger(frames)&&frames>=1&&frames<=1800);
      await press(command.key,hold,frames);
     }else assert.ok(['capture','finish'].includes(command.action));
     const result=await capture(command.id);fs.appendFileSync(path.join(directory,'commands.jsonl'),JSON.stringify({...command,result})+'\n');
     fs.writeFileSync(path.join(directory,'command-complete.json'),JSON.stringify({id:command.id,result},null,2));
     if(command.action==='finish'){
      assert.ok(overlays.has(requiredOverlay),'Selected actual Extra Room game overlay must have run');assert.equal(result.after.game.progress.overlay,75,'Round must return to Extra Room');
      assert.equal(command.roundResultsRankingAndReturnReviewed,true,'Actual paired round/results/ranking/return images require visual review');
      const final=await snapshot();assert.ok(final.matchingRecords>=30);const report={status:'execution-passed-visual-review-required',scope:'Actual browser Host/native Join same-PC Extra Room gameplay; all observed overlapping completed inputs/checkpoints/RNG agree; no four-player/WAN claim',requiredOverlay,visitedOverlays:[...overlays],firstSaved,lastSaved,archived,final,captures,visualReviewDeclared:true};
      fs.writeFileSync(path.join(directory,'execution.json'),JSON.stringify(report,null,2));return {evidence:path.join(directory,'execution.json'),visitedOverlays:report.visitedOverlays,firstSaved,lastSaved,archived,matchingFinalRecords:final.matchingRecords};
     }
    }
   }await sleep(100);
  }
 }catch(error){fs.writeFileSync(path.join(directory,'failure.json'),JSON.stringify({error:error.stack,archived,firstSaved,lastSaved,visitedOverlays:[...overlays],captures},null,2));throw error;}
}
module.exports={driveExtraRoom};
