'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto'),{spawn}=require('node:child_process'),{pathToFileURL}=require('node:url');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const {fingerprint}=require('../tools/stage-local-package.cjs');
const sha=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
(async()=>{
 const runtime=path.resolve(process.env.PARTYBOARD_NATIVE_RUNTIME||'C:/Users/wigwo/Documents/Codex/m4bn-current');
 const disc=process.env.PARTYBOARD_LOCAL_CISO||'D:/Desktop/Games/Mario Party 4/Mario Party 4 (USA) (Rev 1).ciso';
 const output=path.resolve(process.env.PARTYBOARD_TEST_OUTPUT_DIRECTORY||'outputs/validation/normal-startup-'+Date.now());
 assert.equal(fs.existsSync(output),false,'Preserve previous startup evidence');fs.mkdirSync(output,{recursive:true});
 const descriptor=JSON.parse(fs.readFileSync(path.join(runtime,'native-session-package.json')));
 assert.equal(fingerprint(runtime,descriptor),descriptor.configuration.buildId);
 const {fingerprintRuntimeFiles}=await import(pathToFileURL(path.join(__dirname,'runtime-build.mjs')));
 const local=await fingerprintRuntimeFiles(['contract.mjs','contract.wasm'].map(name=>({name,blob:new Blob([fs.readFileSync(path.join(runtime,'browser-runtime',name))])})));
 const env={...process.env,PARTYBOARD_PORT:'8956',PARTYBOARD_LISTEN_ADDRESS:'127.0.0.1',PARTYBOARD_RUNTIME_PACKAGE:path.join(runtime,'native-session-package.json')};
 for(const name of ['PARTYBOARD_RUNTIME_DIRECTORY','PARTYBOARD_TEST_RUNTIME_DIRECTORY','PARTYBOARD_TLS_CERTIFICATE','PARTYBOARD_TLS_KEY','PARTYBOARD_PUBLIC_ORIGIN'])delete env[name];
 let server,context,page,stage='server';const messages=[],errors=[],inputs=[],screenshots=[];
 const watchdog=setTimeout(()=>{void context?.close();server?.kill();},300000);
 const state=()=>page.evaluate(()=>app.localGameState());
 const capture=async name=>{const file=path.join(output,name+'.png');await page.locator('#canvas').screenshot({path:file});screenshots.push({name:name+'.png',sha256:sha(file),state:await state()});};
 const waitFrame=async frame=>page.waitForFunction(target=>{const state=app.localGameState();if(document.querySelector('#status').textContent.startsWith('Local game failed:'))throw Error(document.querySelector('#status').textContent);return state?.frame>=target;},frame,{timeout:90000});
 try{
  server=spawn(process.execPath,[path.join(__dirname,'serve.cjs')],{env,windowsHide:true,stdio:['ignore','pipe','pipe']});
  await new Promise((resolve,reject)=>{server.stdout.once('data',resolve);server.once('error',reject);server.once('exit',code=>reject(Error('Server exited '+code)));});
  stage='browser';context=await chromium.launchPersistentContext(path.join(output,'owned-browser-profile'),{channel:'msedge',headless:true,args:['--mute-audio'],viewport:{width:1100,height:850}});
  page=await context.newPage();page.on('pageerror',error=>errors.push(error.message));
  page.on('console',message=>{const row={at:new Date().toISOString(),type:message.type(),text:message.text(),inputCount:inputs.length};messages.push(row);fs.appendFileSync(path.join(output,'console.jsonl'),JSON.stringify(row)+'\n');});
  await page.addInitScript(()=>localStorage.setItem('partyboard.gameAudio',JSON.stringify({volume:0,muted:true})));
  await page.goto('http://127.0.0.1:8956/');await page.evaluate(async()=>{globalThis.app=await import('/app.mjs');});
  assert.equal(await page.evaluate(()=>localStorage.length),1,'Only the test mute preference should exist before startup');
  stage='normal local launch';await page.locator('#file').setInputFiles(disc);await page.locator('#play-local').click();
  await page.waitForFunction(()=>app.localGameState()?.frame>=60||/^Local game (unavailable|failed)/.test(document.querySelector('#status').textContent),null,{timeout:180000});
  assert.match(await page.locator('#status').textContent(),/^Local game running/);assert.equal((await state()).buildId,local.buildId);
  await waitFrame(1300);await capture('title-before-start');
  stage='one Start input';await page.locator('#canvas').focus();inputs.push({key:'Enter',before:await state()});await page.keyboard.press('Enter',{delay:80});
  const bypass='Session profile: all rewards unlocked; memory card bypassed';
  const deadline=Date.now()+60000;while(!messages.some(row=>row.text.includes(bypass))){if(Date.now()>deadline)throw Error('Normal mode selection did not report the memory-card bypass after one Start input');await new Promise(resolve=>setTimeout(resolve,100));}
  const receipt=messages.find(row=>row.text.includes(bypass));assert.equal(receipt.inputCount,1,'No confirm/navigation input may dismiss a card prompt before the bypass receipt');
  // The envelope/card animation has 192 explicit simulation waits, followed by
  // motion/window animation. The first preserved run shows the prompt by300
  // frames. Wait420 simulation frames before its single confirm, not wall time.
  stage='normal mode menu';const entered=await state();assert.equal(entered.progress.overlay,74);await waitFrame(entered.frame+420);await capture('mode-intro-before-confirm');
  inputs.push({key:'z',before:await state()});await page.keyboard.press('z',{delay:80});
  const menuDeadline=Date.now()+60000;while(!messages.some(row=>row.text.includes('Online Multiplayer entry: available'))){if(Date.now()>menuDeadline)throw Error('Mode-selection confirm did not reach the actual selection loop');await new Promise(resolve=>setTimeout(resolve,100));}
  await waitFrame((await state()).frame+45);await capture('mode-selection');
  for(const [key,name]of [['ArrowRight','mode-right'],['ArrowLeft','mode-return']]){
   inputs.push({key,before:await state()});await page.keyboard.press(key,{delay:80});await waitFrame((await state()).frame+45);await capture(name);
  }
  assert.equal((await state()).progress.overlay,74);assert.deepEqual(errors,[]);
  stage='clean stop';await page.locator('#stop-game').click();await page.waitForFunction(()=>document.querySelector('#status').textContent==='Local game stopped.',null,{timeout:15000});assert.equal(await state(),null);
  const report={status:'execution-passed-visual-review-required',scope:'Fresh ordinary Edge profile; production Play local game/normal boot, one Start before memory-card bypass, mode menu and clean stop; no native or later-game save-screen claim',buildId:descriptor.configuration.buildId,localBuildId:local.buildId,muted:true,inputs,bypassReceipt:receipt,screenshots,errors,cleanStop:true,productionFilesUnmodified:true,sourceHashes:Object.fromEntries(['app.mjs','runtime-asset.mjs'].map(name=>[name,sha(path.join(__dirname,name))])),at:new Date().toISOString()};
  fs.writeFileSync(path.join(output,'execution.json'),JSON.stringify(report,null,2));console.log('PASS execution: normal browser boot reached the memory-card bypass after exactly one Start, opened the mode menu and stopped cleanly. Review captured images.');
 }catch(error){if(page){try{await capture('failure');}catch{}}fs.writeFileSync(path.join(output,'failure.json'),JSON.stringify({stage,error:error.stack,errors,inputs,screenshots},null,2));throw error;}
 finally{clearTimeout(watchdog);await context?.close();server?.kill();}
})().catch(error=>{console.error(error);process.exitCode=1;});
