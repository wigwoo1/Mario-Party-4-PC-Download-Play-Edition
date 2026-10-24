const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),{spawn}=require('node:child_process');
async function verifyLobbyPlaylist(page,peer){
 const playing=track=>page.waitForFunction(name=>{const audio=document.querySelector('#lobby-audio');return audio?.currentSrc.endsWith(name)&&!audio.paused&&audio.readyState>=2&&audio.currentTime>0&&audio.error===null;},track,{timeout:30000});
 await playing('wifi-menu.opus');
 const tracks=[{name:'wifi-menu.opus',duration:await page.locator('#lobby-audio').evaluate(audio=>audio.duration)}];
 let time=await page.locator('#lobby-audio').evaluate(audio=>audio.currentTime);
 await page.waitForFunction(value=>document.querySelector('#lobby-audio').currentTime>value+0.5,time,{timeout:10000});
 for(const name of ['currently-spectating.opus','wifi-menu.opus']){
  await page.waitForFunction(()=>{const audio=document.querySelector('#lobby-audio');return Number.isFinite(audio.duration)&&audio.duration>1;},{},{timeout:30000});
  await page.locator('#lobby-audio').evaluate(audio=>{if(!Number.isFinite(audio.duration)||audio.duration<=1)throw Error('Decoded track duration required.');audio.currentTime=audio.duration-0.2;});
  await playing(name);time=await page.locator('#lobby-audio').evaluate(audio=>audio.currentTime);
  await page.waitForFunction(value=>document.querySelector('#lobby-audio').currentTime>value+0.5,time,{timeout:10000});
  tracks.push({name,duration:await page.locator('#lobby-audio').evaluate(audio=>audio.duration)});
 }
 return {peer,tracks,advancing:true,wrapped:true};
}
(async()=>{
 const output=path.resolve(process.env.PARTYBOARD_TEST_OUTPUT_DIRECTORY||'work/automatic-lobby-validation');fs.mkdirSync(output,{recursive:true});
 assert.ok(process.env.PARTYBOARD_RUNTIME_PACKAGE&&process.env.PARTYBOARD_LOCAL_CISO,'Measured runtime package and private local game required');
 const count=Number(process.env.PARTYBOARD_AUTOMATIC_BROWSER_PEERS||2);assert.ok(count===2||count===4,'Two or four actual browser peers required');
 const preparationTimeout=Number(process.env.PARTYBOARD_TEST_PREPARATION_TIMEOUT_MS||(count===4?900000:270000));
 assert.ok(Number.isSafeInteger(preparationTimeout)&&preparationTimeout>=30000&&preparationTimeout<=900000,'Bounded browser preparation timeout required');
 const port=Number(process.env.PARTYBOARD_TEST_PORT||8951),base='http://127.0.0.1:'+port+'/',errors=[],warnings=[],snapshots=[];
 let browser;const pages=[],lobbyMusic=[];
 const server=spawn(process.execPath,[path.join(__dirname,'serve.cjs')],{env:{...process.env,PARTYBOARD_PORT:String(port)},stdio:['ignore','pipe','pipe']});
 const deadline=setTimeout(()=>{server.kill();void browser?.close();},preparationTimeout+90000);
 try{
  await new Promise((resolve,reject)=>{server.stdout.once('data',resolve);server.once('error',reject);server.once('exit',code=>reject(Error('Server exited: '+code)));});
  browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio','--disable-features=WebRtcHideLocalIpsWithMdns']});
  for(let i=0;i<count;i++){
   const context=await browser.newContext();await context.addInitScript(()=>{const Original=RTCPeerConnection;globalThis.testConnections=[];globalThis.RTCPeerConnection=class extends Original{constructor(...args){super(...args);testConnections.push(this);}};});
   const page=await context.newPage();pages.push(page);
   page.on('pageerror',e=>errors.push({peer:i,message:e.message}));page.on('console',m=>{if(['error','warning'].includes(m.type())){warnings.push({peer:i,text:m.text()});if(warnings.length>1000)warnings.shift();}});
   await page.goto(base);await page.evaluate(async()=>{globalThis.app=await import('/app.mjs');});
   for(const id of ['host','join','prepare-game','ready-game','answer'])assert.equal(await page.locator('#'+id).isVisible(),false,'No manual setup control: '+id);
  }
  await pages[0].locator('#file').setInputFiles(process.env.PARTYBOARD_LOCAL_CISO);
  await pages[0].locator('#host-game').click();
  if(process.env.PARTYBOARD_TEST_LOBBY_MUSIC==='1')lobbyMusic.push(await verifyLobbyPlaylist(pages[0],'host'));
  await pages[0].waitForFunction(()=>document.querySelector('#lobby-link').value.includes('#room='),null,{timeout:30000});
  const link=await pages[0].locator('#lobby-link').inputValue();
  for(let i=1;i<count;i++){
   await pages[i].goto(link);await pages[i].evaluate(async()=>{globalThis.app=await import('/app.mjs');});
   assert.equal(await pages[i].locator('#join-link').inputValue(),link);
   await pages[i].locator('#join-game').click();
   if(process.env.PARTYBOARD_TEST_LOBBY_MUSIC==='1')lobbyMusic.push(await verifyLobbyPlaylist(pages[i],'guest'+i));
  }
  const until=Date.now()+preparationTimeout;let ready=false,last=0;
  while(Date.now()<until){
   const state=await Promise.all(pages.map(p=>p.evaluate(()=>({status:document.querySelector('#status').textContent,ready:app.lobbyReadyState(),runtime:app.lobbyRuntimeState()}))));
   if(Date.now()-last>15000){snapshots.push(state);console.log(state.map((s,i)=>'Peer '+i+': '+s.status).join('\n'));last=Date.now();}
   if(state.some(s=>/failed:|Lobby setup:|Connection failed|preparation failed/i.test(s.status)))throw Error(JSON.stringify(state));
   if(state.every(s=>s.ready?.canStart&&s.ready.players.length===count)){ready=true;break;}
   await new Promise(r=>setTimeout(r,500));
  }
  assert.ok(ready,'All requested peers automatically ready');
  const reports=await Promise.all(pages.map(p=>p.evaluate(()=>app.lobbyPreparedReports())));
  const descriptor=JSON.parse(fs.readFileSync(process.env.PARTYBOARD_RUNTIME_PACKAGE,'utf8'));
  const seed=JSON.parse(fs.readFileSync(path.join(path.dirname(process.env.PARTYBOARD_RUNTIME_PACKAGE),'initial_pipeline_cache.json'),'utf8'));
  assert.equal(reports[0].host?.buildId,descriptor.configuration.buildId,'Actual prepared runtime must match the measured bundle');
  assert.equal(reports[0].host?.pipelineSeed,seed.sha256,'Actual prepared runtime must use the configured shader seed');
  assert.equal(reports[0].host?.frame,0,'Automatic start must agree at frame zero');
  const inputPlans=await Promise.all(pages.map(p=>p.evaluate(()=>app.lobbyInputPlan())));
  fs.writeFileSync(path.join(output,'pre-start-receipts.json'),JSON.stringify({reports,inputPlans,expectedBuildId:descriptor.configuration.buildId,expectedPipelineSeed:seed.sha256},null,2));
  assert.equal(inputPlans[0]?.peer,'host');
  assert.equal(new Set(inputPlans.map(plan=>plan?.peer)).size,count,'Each browser must have its own assigned peer');
  assert.equal(new Set(inputPlans.map(plan=>plan?.owner)).size,count,'Each browser must own a separate logical controller');
  assert.equal(Object.keys(reports[0]).length,count,'Host must retain each actual prepared receipt');
  for(let i=1;i<count;i++){
   const peer=inputPlans[i].peer;
   assert.deepEqual(inputPlans[i].bindings,inputPlans[0].bindings,'All browsers must agree on port assignments');
   assert.deepEqual(reports[0][peer],reports[0].host);
   assert.deepEqual(reports[i][peer],reports[0].host);
   // Guests retain their own receipt; the host checks every receipt before the
   // start barrier sends the shared identity proposal for guest acknowledgment.
  }
  await pages[0].locator('#automatic-start').click();
  for(const p of pages)await p.waitForFunction(()=>app.lobbyGameState()?.frame>=120,null,{timeout:60000});
  const running=await Promise.all(pages.map(p=>p.evaluate(()=>app.lobbyGameState())));let recovery;
  if(process.env.PARTYBOARD_TEST_AUTOMATIC_RECOVERY==='1'){
   const configurations=await Promise.all(pages.map(p=>p.evaluate(()=>app.lobbySessionConfiguration())));
   await pages[0].evaluate(()=>{const connection=testConnections.find(p=>p.connectionState==='connected');if(!connection)throw Error('Actual connected transport required');connection.close();});
   for(const p of pages)await p.waitForFunction(()=>app.lobbyGameState()?.input.state==='Paused',null,{timeout:15000});
   const paused=await Promise.all(pages.map(p=>p.evaluate(()=>app.lobbyGameState())));console.log('Actual game transport closed. Waiting for automatic verified recovery.');
   for(const p of pages)for(const id of ['recovery-host','recovery-join','recovery-remote','recovery-copy'])assert.equal(await p.locator('#'+id).isVisible(),false,'No manual recovery control: '+id);
   for(let i=0;i<pages.length;i++)await pages[i].waitForFunction(frame=>{const s=app.lobbyGameState();return s?.input.state==='Running'&&BigInt(s.input.frame)>BigInt(frame)+90n;},paused[i].input.frame,{timeout:55000});
   const observations=await Promise.all(pages.map(p=>p.evaluate(()=>app.lobbyInputObservations())));
   const common=observations[0].filter(row=>BigInt(row.frame)>=BigInt(paused[0].input.frame)&&observations.every(rows=>rows.some(other=>other.frame===row.frame)));
   assert.ok(common.length>=30);for(const row of common)for(const rows of observations)assert.deepEqual(rows.find(other=>other.frame===row.frame),row);
   for(let i=0;i<pages.length;i++)assert.deepEqual(await pages[i].evaluate(()=>app.lobbySessionConfiguration()),configurations[i]);
   recovery={paused,matchedRecoveredPolls:common.length,running:await Promise.all(pages.map(p=>p.evaluate(()=>app.lobbyGameState())))};
   console.log('PASS automatic signed replacement and agreed resume within the original recovery deadline; matching completed polls: '+common.length);
  }
  const lobbyMusicStopped=[];
  if(process.env.PARTYBOARD_TEST_LOBBY_MUSIC==='1'){
   assert.equal(lobbyMusic.length,count,'Every lobby participant must complete playback and wrap checks.');
   for(const [index,page] of pages.entries()){
    await page.locator('#stop').click();
    await page.waitForFunction(()=>{const audio=document.querySelector('#lobby-audio');return audio.paused&&!audio.hasAttribute('src');},{},{timeout:10000});
    lobbyMusicStopped.push({peer:index===0?'host':'guest'+index,stopped:true});
   }
  }
  assert.deepEqual(errors,[]);
  if(process.env.PARTYBOARD_TEST_LOBBY_MUSIC==='1')fs.writeFileSync(path.join(output,'lobby-music-acceptance.json'),JSON.stringify({status:'passed',muted:true,syntheticTransfers:false,lobbyMusic,stopped:lobbyMusicStopped,at:new Date().toISOString()},null,2));
  fs.writeFileSync(path.join(output,'acceptance.json'),JSON.stringify({status:'passed',peerCount:count,scope:count+' isolated Edge contexts on one PC using real P2P assets, production runtime and UI-only automatic join/preparation/start',reports,inputPlans,running,recovery,lobbyMusic,lobbyMusicStopped,errors,warnings,at:new Date().toISOString()},null,2));
  console.log('PASS automatic Host/link Join, asset transfer, shader preparation, readiness and one-click synchronized start; all '+count+' peers ran at least 120 frames.');
 }catch(e){const finalStatus=await Promise.all(pages.map(p=>p.locator('#status').textContent().catch(()=> 'unavailable')));fs.writeFileSync(path.join(output,'failure.json'),JSON.stringify({message:e.stack,finalStatus,errors,warnings,snapshots,lobbyMusic},null,2));throw e;}
 finally{
  clearTimeout(deadline);for(const page of pages)try{await page.locator('#stop').click({timeout:5000});}catch{}
  await browser?.close();server.kill();
 }
})().catch(e=>{console.error(e);process.exitCode=1;});
