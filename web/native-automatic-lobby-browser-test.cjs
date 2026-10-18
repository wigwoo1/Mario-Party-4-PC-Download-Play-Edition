const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),{spawn}=require('node:child_process');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const {browserStorageObservation}=require('../tools/native-rtc-probe/browser-storage-observation.cjs');
const browserProfile=process.env.PARTYBOARD_AUTOMATIC_BROWSER_PROFILE||'private';assert.ok(['private','persistent'].includes(browserProfile));
const preparationTimeout=Number(process.env.PARTYBOARD_TEST_PREPARATION_TIMEOUT_MS||300000);assert.ok(Number.isInteger(preparationTimeout)&&preparationTimeout>=30000&&preparationTimeout<=900000);
const pause=ms=>new Promise(r=>setTimeout(r,ms));
const mode=process.env.PARTYBOARD_NATIVE_AUTOMATIC_ROLE||'host',nativeHost=mode==='host';assert.ok(['host','join'].includes(mode));
const runtime=path.resolve(process.env.PARTYBOARD_NATIVE_RUNTIME||'C:/Users/wigwo/Documents/Codex/m4bn-current');
const descriptor=path.resolve(process.env.PARTYBOARD_RUNTIME_PACKAGE||path.join(runtime,'native-session-package.json'));
const disc=process.env.PARTYBOARD_LOCAL_CISO||'D:/Desktop/Games/Mario Party 4/Mario Party 4 (USA) (Rev 1).ciso';
const guestCount=Number(process.env.PARTYBOARD_AUTOMATIC_BROWSER_GUESTS||1);assert.ok([1,3].includes(guestCount)&& (nativeHost||guestCount===1));
const testRejoin=process.env.PARTYBOARD_TEST_AUTOMATIC_REJOIN==='1';assert.ok(!testRejoin||(nativeHost&&guestCount===1));
const output=path.resolve(process.env.PARTYBOARD_TEST_OUTPUT_DIRECTORY||path.join(__dirname,'../../validation/native-automatic-'+mode+'-'+Date.now()+'-'+process.pid));
assert.equal(fs.existsSync(output),false,'Automatic lobby test requires a fresh output directory; preserve existing evidence');
function normalized(row){return {frame:String(row.frame),checkpoint:row.checkpoint,frand:row.frand,rand8:row.rand8,pads:row.pads};}
(async()=>{
 fs.mkdirSync(output,{recursive:true});const profile=path.join(output,'native-profile');fs.mkdirSync(profile,{recursive:true});
 const sourceHashes=Object.fromEntries(['app.mjs','runtime-asset.mjs'].map(name=>[name,require('node:crypto').createHash('sha256').update(fs.readFileSync(path.join(__dirname,name))).digest('hex')]));
 fs.writeFileSync(path.join(output,'test-configuration.json'),JSON.stringify({browserProfile,mode,guestCount,preparationTimeout,sourceHashes,muted:true,at:new Date().toISOString()},null,2));
 fs.writeFileSync(path.join(profile,'config.json'),JSON.stringify({'audio.muted':true,'audio.masterVolume':0,'audio.mainMusicVolume':0,'audio.menuSounds':false}));
 const port=8954,base='http://127.0.0.1:'+port+'/',events=[],errors=[],statuses=[],pages=[],contexts=[];let native,server,browser,page,exit,nativeText='',stopping=false;
 const createContext=async()=>{const args=['--mute-audio','--disable-features=WebRtcHideLocalIpsWithMdns'];const context=browserProfile==='persistent'
  ?await chromium.launchPersistentContext(path.join(output,'browser-profile-'+contexts.length),{channel:'msedge',headless:true,args})
  :await browser.newContext();contexts.push(context);return context;};
 const write=command=>native.stdin.write((typeof command==='string'?command:JSON.stringify(command))+'\n');
 const wait=async(check,limit=60000)=>{const until=Date.now()+limit;while(true){const result=await check();if(result)return result;if(exit!==undefined&&!stopping)throw Error('Native frontend exited '+exit);if(Date.now()>until)throw Error('Automatic native '+mode+' observation timed out');await pause(100);}};
 const waitEvent=async(predicate,limit)=>wait(()=>events.find(predicate),limit);
 const observe=async()=>{const before=events.length;write('observe');await wait(()=>events.slice(before).some(e=>e.type==='automatic-game-observation'));return events.slice(before).find(e=>e.type==='automatic-game-observation');};
 const nativeLog=fs.createWriteStream(path.join(output,'native.log'));const watchdog=setTimeout(()=>{native?.kill();server?.kill();void browser?.close();for(const context of contexts)void context.close();},preparationTimeout+180000);
 try{
  const env={...process.env,PARTYBOARD_PORT:String(port),PARTYBOARD_RUNTIME_PACKAGE:descriptor};for(const name of ['PARTYBOARD_RUNTIME_DIRECTORY','PARTYBOARD_TEST_RUNTIME_DIRECTORY','PARTYBOARD_TLS_CERTIFICATE','PARTYBOARD_TLS_KEY','PARTYBOARD_LISTEN_ADDRESS'])delete env[name];
  server=spawn(process.execPath,[path.join(__dirname,'serve.cjs')],{env,windowsHide:true,stdio:['ignore','pipe','pipe']});await new Promise((r,j)=>{server.stdout.once('data',r);server.once('error',j);server.once('exit',c=>j(Error('Service exited '+c)));});
  if(browserProfile==='private')browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio','--disable-features=WebRtcHideLocalIpsWithMdns']});
  const context=await createContext();await context.addInitScript(()=>{const Original=RTCPeerConnection;globalThis.testConnections=[];globalThis.RTCPeerConnection=class extends Original{constructor(...args){super(...args);testConnections.push(this);}};});
  page=await context.newPage();pages.push(page);page.on('pageerror',e=>errors.push(e.message));page.on('console',m=>{if(['error','warning'].includes(m.type()))fs.appendFileSync(path.join(output,'browser.log'),m.type()+': '+m.text()+'\n');});
  await page.goto(base);await page.evaluate(async()=>{globalThis.app=await import('/app.mjs');});
  const nativeEnv={...process.env,PARTYBOARD_AUTOMATIC_LOBBY_TEST:'1',PARTYBOARD_LOBBY_URL:base,SDL_AUDIODRIVER:'dummy'};
  for(const key of ['PARTYBOARD_NATIVE_HOST_RETURN_ACCEPTANCE','PARTYBOARD_NATIVE_JOIN_RETURN_ACCEPTANCE','PARTYBOARD_NATIVE_FULL_MATCH','PARTYBOARD_NATIVE_DIAGNOSTIC_FINISH_ACK','PARTYBOARD_PROBE_DESCRIPTOR_SEED'])delete nativeEnv[key];
  native=spawn(path.join(runtime,'partyboard.exe'),['--user-dir',profile,'--keyboard','--disc',disc,'--native-session-package',descriptor,nativeHost?'--probe-host-gameplay':'--probe-join-menu'],{cwd:runtime,env:nativeEnv,windowsHide:true,stdio:['pipe','pipe','pipe']});
  native.stderr.on('data',b=>nativeLog.write(b));native.stdout.on('data',b=>{
   nativeLog.write(b);nativeText+=b;let end;while((end=nativeText.indexOf('\n'))>=0){const line=nativeText.slice(0,end);nativeText=nativeText.slice(end+1);const receiptPrefix='Native owned lobby prepared receipt: ';if(line.includes(receiptPrefix)){try{events.push({type:'actual-native-prepared-receipt',receipt:JSON.parse(line.split(receiptPrefix)[1])});}catch{}}for(const prefix of ['Native frontend event: ','Native external Join event: ']){const index=line.indexOf(prefix);if(index>=0){try{events.push(JSON.parse(line.slice(index+prefix.length)));}catch{}}}}
  });native.on('exit',code=>{exit=code;});native.on('error',e=>errors.push(e.message));
  let link;
  if(nativeHost){const opened=await waitEvent(e=>e.type==='automatic-lobby-link',120000);assert.equal(opened.automatic,true);link=opened.link;await page.goto(link);await page.evaluate(async()=>{globalThis.app=await import('/app.mjs');});await page.locator('#join-game').click();}
  else {await waitEvent(e=>e.type==='automatic-join-waiting',60000);await page.locator('#file').setInputFiles(disc);await page.locator('#host-game').click();await page.waitForFunction(()=>document.querySelector('#lobby-link').value.includes('#room='));link=await page.locator('#lobby-link').inputValue();write({type:'join-link',link});}
  if(nativeHost)for(let index=1;index<guestCount;index++){
   const extra=await createContext(),guest=await extra.newPage();pages.push(guest);guest.on('pageerror',e=>errors.push(e.message));
   await guest.goto(link);await guest.evaluate(async()=>{globalThis.app=await import('/app.mjs');});await guest.locator('#join-game').click();
  }
  const waitReady=async()=>{const started=Date.now(),readyDeadline=started+preparationTimeout;let ready=false,last=0;
  while(Date.now()<readyDeadline){
   const peerStatuses=await Promise.all(pages.map(p=>p.locator('#status').textContent())),status=peerStatuses[0],state=await page.evaluate(()=>app.lobbyReadyState());
   if(Date.now()-last>15000){console.log('Browser preparation: '+JSON.stringify(peerStatuses));statuses.push({status,peerStatuses,state});last=Date.now();
    if(process.env.PARTYBOARD_TEST_STORAGE_DIAGNOSTICS==='1')fs.appendFileSync(path.join(output,'storage.jsonl'),JSON.stringify({at:new Date().toISOString(),browserProfile,observations:await Promise.allSettled(pages.map(p=>browserStorageObservation(p)))})+'\n');}
   if(errors.length||peerStatuses.some(text=>/preparation failed|Connection failed|Lobby setup:|does not match/i.test(text)))throw Error('Automatic preparation failed: '+JSON.stringify(peerStatuses)+' '+errors.join(' | '));
   if(exit!==undefined)throw Error('Native frontend exited before readiness: '+exit);
   if(state?.canStart&&state.players.length===guestCount+1&&(await Promise.all(pages.map(p=>p.evaluate(()=>app.lobbyReadyState()?.canStart)))).every(Boolean)){ready=true;break;}await pause(300);
  }assert.ok(ready,'Native and browser must automatically prepare and become ready');return {elapsedMs:Date.now()-started,budgetMs:preparationTimeout};};
  const preparation=await waitReady();let rejoin;
  if(testRejoin){
   const original=await page.evaluate(()=>app.lobbyReadyState().players.find(p=>p.peer!=='host').peer),before=events.length;
   await page.locator('#stop').click();await page.waitForFunction(()=>document.querySelector('#status').textContent.includes('Session stopped'));
   const removed=await wait(()=>events.slice(before).find(e=>e.type==='automatic-lobby-state'&&e.snapshot.players.length===1),30000);
   assert.equal(removed.snapshot.canStart,false);assert.equal(removed.roomError,'');
   await page.goto(link);await page.evaluate(async()=>{globalThis.app=await import('/app.mjs');});await page.locator('#join-game').click();await waitReady();
   const replacement=await page.evaluate(()=>app.lobbyReadyState().players.find(p=>p.peer!=='host').peer);
   assert.notEqual(replacement,original);rejoin={removed:original,replacement,sameLink:true,automaticReady:true};console.log('PASS pre-game guest departure and automatic rejoin with a fresh assigned identity.');
  }
  const reports=await page.evaluate(()=>app.lobbyPreparedReports());
  const actual=await waitEvent(e=>e.type===(nativeHost?'actual-native-prepared-receipt':'automatic-join-prepared'));
  const actualReceipts=await Promise.all(pages.map(p=>p.evaluate(()=>app.lobbyPreparedReports())));let compared=0;
  for(const group of actualReceipts)for(const receipt of Object.values(group)){assert.deepEqual(actual.receipt,receipt);compared++;}assert.ok(compared>=guestCount);
  for(const id of ['remote','prepare-game','ready-game','answer'])assert.equal(await page.locator('#'+id).isVisible(),false);
  if(nativeHost)write('start');else await page.locator('#automatic-start').click();
  for(const guest of pages)await guest.waitForFunction(()=>app.lobbyGameState()?.frame>=120,null,{timeout:60000});let first=await observe();assert.ok(first.accepted>=120);
  const compare=async(since)=>{let matched=[];for(let attempt=0;attempt<10;attempt++){
   const n=await observe(),records=await Promise.all(pages.map(p=>p.evaluate(()=>app.lobbyInputObservations()))),maps=records.map(rows=>new Map(rows.map(row=>[String(row.frame),normalized(row)])));
   matched=n.records.filter(row=>BigInt(row.frame)>=BigInt(since)&&maps.every(map=>map.has(String(row.frame)))).map(normalized);for(const row of matched)for(const map of maps)assert.deepEqual(row,map.get(row.frame));if(matched.length>=30)return {count:matched.length,first:matched[0].frame,last:matched.at(-1).frame,native:n};await pause(100);
  }throw Error('Insufficient matching completed input/checkpoint/RNG polls');};
  const initial=await compare(0);console.log('PASS automatic '+mode+' menu start; matching mixed-runtime polls: '+initial.count);
  let recovery;
  if(process.env.PARTYBOARD_TEST_AUTOMATIC_RECOVERY!=='0'){
   if(nativeHost)await page.evaluate(()=>{const connection=testConnections.find(c=>c.connectionState==='connected');if(!connection)throw Error('Connected game transport required');connection.close();});else write('disconnect');
   await page.waitForFunction(()=>app.lobbyGameState()?.input.state==='Paused',null,{timeout:15000});const paused=await page.evaluate(()=>app.lobbyGameState());console.log('Actual mixed-runtime input connection closed; waiting for automatic recovery.');
   for(const guest of pages)await guest.waitForFunction(frame=>{const s=app.lobbyGameState();return s?.input.state==='Running'&&BigInt(s.input.frame)>=BigInt(frame)+90n;},paused.input.frame,{timeout:55000});
   recovery=await compare(paused.input.frame);console.log('PASS automatic '+mode+' signed recovery; matching recovered polls: '+recovery.count);
  }
  assert.deepEqual(errors,[]);const guestReports=await Promise.all(pages.map(p=>p.evaluate(()=>app.lobbyPreparedReports())));const final=await observe();stopping=true;write('stop');await wait(()=>exit!==undefined,30000);assert.equal(exit,0,'Native frontend and stopped-input cleanup must succeed');
  fs.writeFileSync(path.join(output,'acceptance.json'),JSON.stringify({status:'passed',scope:'one real native '+mode+' frontend with '+guestCount+' actual muted Edge peer(s) on the same PC; automatic room signaling, verified assets/known shaders/readiness, Start UI action, synchronized input and signed recovery',runtimeDescriptor:descriptor,browserProfile,sourceHashes,preparation,muted:true,knownPipelineCount:618,players:guestCount+1,reports,guestReports,nativePreparedReceipt:actual.receipt,actualReceiptsCompared:compared,rejoin,initial:{count:initial.count,first:initial.first,last:initial.last},recovery:recovery?{count:recovery.count,first:recovery.first,last:recovery.last}:null,nativeAccepted:final.accepted,nativeExit:exit,errors,at:new Date().toISOString()},null,2));
 }catch(error){const storage=await Promise.allSettled(pages.map(p=>browserStorageObservation(p,{files:true})));fs.writeFileSync(path.join(output,'failure.json'),JSON.stringify({error:error.stack,errors,statuses,browserProfile,storage,nativeExit:exit,latestEvents:events.filter(e=>e.type!=='automatic-lobby-link').slice(-12)},null,2));throw error;}
 finally{clearTimeout(watchdog);if(native&&exit===undefined){try{write('stop');await pause(500);}catch{}if(exit===undefined)native.kill();}for(const guest of pages)await guest.locator('#stop').click({timeout:3000}).catch(()=>{});await Promise.allSettled(contexts.map(context=>context.close()));await browser?.close();server?.kill();nativeLog.end();}
})().catch(e=>{console.error(e);process.exitCode=1;});
