'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto'),{spawn}=require('node:child_process');
const {fingerprint}=require('./stage-local-package.cjs');
const {requestNativeFrame}=require('./native-rtc-probe/native-frame-capture.cjs');
const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms)),sha=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
(async()=>{
 const runtime=path.resolve(process.env.PARTYBOARD_NATIVE_RUNTIME||'C:/Users/wigwo/Documents/Codex/m4bn-current');
 const disc=process.env.PARTYBOARD_LOCAL_CISO||'D:/Desktop/Games/Mario Party 4/Mario Party 4 (USA) (Rev 1).ciso';
 const output=path.resolve(process.env.PARTYBOARD_TEST_OUTPUT_DIRECTORY||'outputs/validation/normal-native-startup-'+Date.now());
 assert.equal(fs.existsSync(output),false,'Preserve previous startup evidence');fs.mkdirSync(output,{recursive:true});
 const descriptor=JSON.parse(fs.readFileSync(path.join(runtime,'native-session-package.json')));assert.equal(fingerprint(runtime,descriptor),descriptor.configuration.buildId);
 const profile=path.join(output,'owned-profile');fs.mkdirSync(profile);
 fs.writeFileSync(path.join(profile,'config.json'),JSON.stringify({'backend.skipPreLaunchUI':true,'backend.skipBootSequence':false,'backend.checkForUpdates':false,'backend.enableCrashReporting':false,'audio.muted':true,'audio.masterVolume':0,'audio.mainMusicVolume':0,'audio.menuSounds':false}));
 const samples=3400,stimulus=Buffer.alloc(16+samples*68);Buffer.from('PBTRACE').copy(stimulus);stimulus[7]=3;stimulus.writeUInt32LE(1,8);
 const actions=[{first:1500,count:6,button:0x1000,name:'Start'},{first:2500,count:6,button:0x0100,name:'Confirm'},{first:2850,count:6,button:0,stickX:72,name:'Main-stick right'},{first:3100,count:6,button:0,stickX:-72,name:'Main-stick left'}];
 for(let index=0;index<samples;index++){
  const offset=16+index*68;stimulus.writeBigUInt64LE(BigInt(index),offset);
  for(let slot=1;slot<4;slot++)stimulus[offset+20+slot*11+10]=255;
  const action=actions.find(action=>index>=action.first&&index<action.first+action.count);if(action){stimulus.writeUInt16LE(action.button,offset+20);stimulus.writeInt8(action.stickX||0,offset+22);}
 }
 const script=path.join(output,'startup-inputs.pbtrace'),recording=path.join(output,'actual-inputs.pbtrace');fs.writeFileSync(script,stimulus,{flag:'wx'});
 const captureRequest=path.join(output,'capture.request'),args=['--keyboard','--user-dir',profile,'--disc',disc,'--record-input',recording,'--input-script',script,'--simulation-seed','1'];
 const env={...process.env,SDL_AUDIODRIVER:'dummy',PARTYBOARD_FRAME_CAPTURE_REQUEST:captureRequest};
 for(const name of ['PARTYBOARD_AUTOMATIC_LOBBY_TEST','PARTYBOARD_NATIVE_FULL_MATCH','PARTYBOARD_NATIVE_HOST_RETURN_ACCEPTANCE','PARTYBOARD_NATIVE_JOIN_RETURN_ACCEPTANCE','PARTYBOARD_NATIVE_DIAGNOSTIC_FINISH_ACK','PARTYBOARD_PROBE_DESCRIPTOR_SEED'])delete env[name];
 const child=spawn(path.join(runtime,'partyboard.exe'),args,{cwd:runtime,env,windowsHide:true,stdio:['ignore','pipe','pipe']});
 let exit,error,log='',stage='native startup';const captures=[];
 child.on('exit',code=>{exit=code;});child.on('error',value=>{error=value;});
 for(const stream of [child.stdout,child.stderr])stream.on('data',bytes=>{log+=bytes;fs.appendFileSync(path.join(output,'native.log'),bytes);});
 const started=Date.now(),watchdog=setTimeout(()=>child.kill(),180000);
 const accepted=()=>fs.existsSync(recording)?Math.max(0,Math.floor((fs.statSync(recording).size-16)/68)):0;
 const wait=async check=>{while(!check()){if(error)throw error;if(exit!==undefined)throw Error('Native exited before '+stage+': '+exit);if(Date.now()-started>175000)throw Error('Native startup observation timed out at '+stage);await pause(50);}};
 const capture=async(name,target,before)=>{
  stage=name;await wait(()=>accepted()>=target);const sampleBefore=accepted();
  const result=await requestNativeFrame(captureRequest,10000);const sampleAfter=accepted();
  if(before!==undefined)assert.ok(sampleAfter<before,'Capture must finish before the next scripted action');
  const file=path.join(output,name+'.ppm');assert.ok(!fs.existsSync(file));
  for(let attempt=0;;attempt++){try{fs.renameSync(result.path,file);break;}catch(error){if(attempt>=20)throw error;await pause(10);}}
  captures.push({...result,path:file,sha256:sha(file),sampleBefore,sampleAfter});console.log('Captured '+name+' near input sample '+sampleAfter);
 };
 try{
  await capture('title-before-start',1200,1500);
  await capture('mode-intro-before-confirm',2100,2500);
  await capture('mode-selection',2650,2850);
  await capture('mode-right',2920,3100);
  await capture('mode-return',3180);
  stage='bounded script completion';await wait(()=>exit!==undefined);assert.equal(exit,0);
  assert.ok(log.includes('Session profile: all rewards unlocked; memory card bypassed'));assert.ok(log.includes('Online Multiplayer entry: available in mode selector'));
  assert.ok(log.includes('Input script: complete (3400 samples); new recording finished'));assert.ok(log.includes('Input trace: closed after 3400 samples'));
  const actual=fs.readFileSync(recording);assert.equal(actual.length,stimulus.length);assert.equal(actual.subarray(0,8).toString('hex'),stimulus.subarray(0,8).toString('hex'));
  for(let index=0;index<samples;index++){const offset=16+index*68;assert.equal(actual.readBigUInt64LE(offset),BigInt(index));assert.deepEqual(actual.subarray(offset+20,offset+64),stimulus.subarray(offset+20,offset+64));}
  const profileFiles=[];const walk=directory=>{for(const item of fs.readdirSync(directory,{withFileTypes:true})){const name=path.join(directory,item.name);assert.equal(item.isSymbolicLink(),false);if(item.isDirectory())walk(name);else profileFiles.push(path.relative(profile,name).replaceAll('\\','/'));}};walk(profile);
  assert.ok(!profileFiles.some(name=>/\.(?:raw|gci|gcs|sav|memcard)$/i.test(name)),'No memory-card/save image should be created');
  assert.equal(fingerprint(runtime,descriptor),descriptor.configuration.buildId,'Installed runtime changed during startup test');
  fs.writeFileSync(path.join(output,'execution.json'),JSON.stringify({status:'execution-passed-visual-review-required',scope:'Production native game_main title/menu route with existing skip-launcher preference, fresh owned profile and bounded seeded logical controller stimulus; not default launcher, physical input or multiplayer proof',buildId:descriptor.configuration.buildId,muted:true,nativeExit:exit,seed:1,samples,actions,captures,profileFiles,stimulusSha256:sha(script),actualTraceSha256:sha(recording),runtimeUnchanged:true,at:new Date().toISOString()},null,2));
  console.log('PASS execution: native title/menu/card-bypass route and bounded trace cleanup. Review actual resolved images.');
 }catch(error){fs.writeFileSync(path.join(output,'failure.json'),JSON.stringify({stage,error:error.stack,nativeExit:exit,accepted:accepted(),captures,logTail:log.slice(-3000)},null,2));throw error;}
 finally{clearTimeout(watchdog);if(exit===undefined)child.kill();}
})().catch(error=>{console.error(error);process.exitCode=1;});
