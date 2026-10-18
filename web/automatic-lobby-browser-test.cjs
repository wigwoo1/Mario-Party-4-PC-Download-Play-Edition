const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),{spawn}=require('node:child_process');
(async()=>{
 const output=path.resolve(process.env.PARTYBOARD_TEST_OUTPUT_DIRECTORY||'work/automatic-lobby-validation');fs.mkdirSync(output,{recursive:true});
 assert.ok(process.env.PARTYBOARD_RUNTIME_PACKAGE&&process.env.PARTYBOARD_LOCAL_CISO,'Measured runtime package and private local game required');
 const port=Number(process.env.PARTYBOARD_TEST_PORT||8951),base='http://127.0.0.1:'+port+'/',errors=[],warnings=[],snapshots=[];
 let browser;const pages=[];
 const server=spawn(process.execPath,[path.join(__dirname,'serve.cjs')],{env:{...process.env,PARTYBOARD_PORT:String(port)},stdio:['ignore','pipe','pipe']});
 const deadline=setTimeout(()=>{server.kill();void browser?.close();},360000);
 try{
  await new Promise((resolve,reject)=>{server.stdout.once('data',resolve);server.once('error',reject);server.once('exit',code=>reject(Error('Server exited: '+code)));});
  browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio','--disable-features=WebRtcHideLocalIpsWithMdns']});
  for(let i=0;i<2;i++){
   const context=await browser.newContext();await context.addInitScript(()=>{const Original=RTCPeerConnection;globalThis.testConnections=[];globalThis.RTCPeerConnection=class extends Original{constructor(...args){super(...args);testConnections.push(this);}};});
   const page=await context.newPage();pages.push(page);
   page.on('pageerror',e=>errors.push({peer:i,message:e.message}));page.on('console',m=>{if(['error','warning'].includes(m.type())){warnings.push({peer:i,text:m.text()});if(warnings.length>1000)warnings.shift();}});
   await page.goto(base);await page.evaluate(async()=>{globalThis.app=await import('/app.mjs');});
   for(const id of ['host','join','prepare-game','ready-game','answer'])assert.equal(await page.locator('#'+id).isVisible(),false,'No manual setup control: '+id);
  }
  await pages[0].locator('#file').setInputFiles(process.env.PARTYBOARD_LOCAL_CISO);
  await pages[0].locator('#host-game').click();
  await pages[0].waitForFunction(()=>document.querySelector('#lobby-link').value.includes('#room='),null,{timeout:30000});
  const link=await pages[0].locator('#lobby-link').inputValue();
  await pages[1].goto(link);await pages[1].evaluate(async()=>{globalThis.app=await import('/app.mjs');});
  assert.equal(await pages[1].locator('#join-link').inputValue(),link);
  await pages[1].locator('#join-game').click();
  const until=Date.now()+270000;let ready=false,last=0;
  while(Date.now()<until){
   const state=await Promise.all(pages.map(p=>p.evaluate(()=>({status:document.querySelector('#status').textContent,ready:app.lobbyReadyState(),runtime:app.lobbyRuntimeState()}))));
   if(Date.now()-last>15000){snapshots.push(state);console.log(state.map((s,i)=>'Peer '+i+': '+s.status).join('\n'));last=Date.now();}
   if(state.some(s=>/failed:|Lobby setup:|Connection failed|preparation failed/i.test(s.status)))throw Error(JSON.stringify(state));
   if(state.every(s=>s.ready?.canStart)){ready=true;break;}
   await new Promise(r=>setTimeout(r,500));
  }
  assert.ok(ready,'Both peers automatically ready');
  const reports=await Promise.all(pages.map(p=>p.evaluate(()=>app.lobbyPreparedReports())));
  assert.deepEqual(reports[0].host,reports[0].guest1);assert.deepEqual(reports[1].guest1,reports[0].host);
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
  assert.deepEqual(errors,[]);
  fs.writeFileSync(path.join(output,'acceptance.json'),JSON.stringify({status:'passed',scope:'two isolated Edge contexts on one PC using real P2P assets, production runtime and UI-only automatic join/preparation/start',reports,running,recovery,errors,warnings,at:new Date().toISOString()},null,2));
  console.log('PASS automatic Host/link Join, asset transfer, shader preparation, readiness and one-click synchronized start; both peers ran at least 120 frames.');
 }catch(e){const finalStatus=await Promise.all(pages.map(p=>p.locator('#status').textContent().catch(()=> 'unavailable')));fs.writeFileSync(path.join(output,'failure.json'),JSON.stringify({message:e.stack,finalStatus,errors,warnings,snapshots},null,2));throw e;}
 finally{
  clearTimeout(deadline);for(const page of pages)try{await page.locator('#stop').click({timeout:5000});}catch{}
  await browser?.close();server.kill();
 }
})().catch(e=>{console.error(e);process.exitCode=1;});
