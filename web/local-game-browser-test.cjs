const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const {spawn}=require('node:child_process');
const fs=require('node:fs');
const path=require('node:path'),assert=require('node:assert/strict');
const {summarizeFrameTiming}=require('../tools/browser-frame-timing.cjs');
if(!process.env.PARTYBOARD_LOCAL_CISO||!process.argv[2])throw Error('Explicit local game file and compiled runtime required.');
const outputDirectory=path.resolve(process.env.PARTYBOARD_TEST_OUTPUT_DIRECTORY||path.join(__dirname,'../../../outputs/validation'));
fs.mkdirSync(outputDirectory,{recursive:true});
const frameTimeout=Number(process.env.PARTYBOARD_TEST_FRAME_TIMEOUT_MS||60000);
if(!Number.isInteger(frameTimeout)||frameTimeout<1000||frameTimeout>180000)throw Error('Invalid frame diagnostic timeout.');
const profileFrames=Number(process.env.PARTYBOARD_TEST_PROFILE_FRAMES||0);
if(!Number.isInteger(profileFrames)||(profileFrames!==0&&(profileFrames<240||profileFrames>5000)))throw Error('Profile frame window must be240–5000 or disabled.');
(async()=>{
  const join=process.env.PARTYBOARD_TEST_MENU_ACTION==='join';
  let browser;const messages=[],errors=[];
  const server=spawn(process.execPath,[path.join(__dirname,'serve.cjs')],{env:{...process.env,
    PARTYBOARD_PORT:'8796',PARTYBOARD_TEST_PORT:'8796',PARTYBOARD_LISTEN_ADDRESS:'127.0.0.1',PARTYBOARD_RUNTIME_PACKAGE:'',PARTYBOARD_RUNTIME_DIRECTORY:path.resolve(process.argv[2])},stdio:['ignore','pipe','pipe']});
  try {
    await new Promise((resolve,reject)=>{server.stdout.once('data',resolve);server.once('error',reject);server.once('exit',code=>reject(Error('Server exited '+code)));});
    browser=await chromium.launch({channel:process.env.PARTYBOARD_TEST_BROWSER_CHANNEL||'msedge',headless:true,args:['--mute-audio']});
      const page=await browser.newPage();page.on('pageerror',e=>errors.push(e.message));
    page.on('console',m=>{messages.push(m.text());if(m.text().startsWith('Online Multiplayer'))console.log(m.text());});
      await page.addInitScript(()=>{
        globalThis.testFrameStageDiagnostics=true;globalThis.testAsyncifyRewindDiagnostics=true;globalThis.partyBoardPageErrors=[];
        addEventListener('error',event=>globalThis.partyBoardPageErrors.push(event.message));
        addEventListener('unhandledrejection',event=>globalThis.partyBoardPageErrors.push(String(event.reason)));
        const ranges=new WeakMap(),stats={reads:0,bytes:0,pending:0,last:undefined,lastError:undefined};
        const slice=Blob.prototype.slice,arrayBuffer=Blob.prototype.arrayBuffer;
        Blob.prototype.slice=function(start=0,end=this.size,type){const result=slice.call(this,start,end,type);ranges.set(result,{start,end,size:result.size});return result;};
        Blob.prototype.arrayBuffer=function(...args){const range=ranges.get(this)||{size:this.size};stats.pending++;
          stats.last={...range,startedAt:Date.now()};
          return arrayBuffer.apply(this,args).then(value=>{stats.reads++;stats.bytes+=value.byteLength;return value;},error=>{stats.lastError=String(error);throw error;})
            .finally(()=>{stats.pending--;stats.last.finishedAt=Date.now();});};
        globalThis.partyBoardBlobReadDiagnostic=()=>({...stats,last:stats.last&&{...stats.last}});
      });
    await page.goto('http://127.0.0.1:8796/');await page.evaluate(async()=>{globalThis.partyBoardApp=await import('/app.mjs');});
    assert.equal(await page.locator('#speaker-layout').isEnabled(),true,'Speaker layout should be selectable before runtime preparation.');
    await page.locator('#speaker-layout').selectOption('6');
    assert.equal(await page.evaluate(()=>localStorage.getItem('partyboard.audioChannels')),'6');
    await page.locator('#file').setInputFiles(process.env.PARTYBOARD_LOCAL_CISO);
    await page.locator('#play-local').click();
    const progress=setInterval(async()=>{
      try{console.log('Launcher diagnostic:',JSON.stringify(await page.evaluate(()=>({status:document.querySelector('#status')?.textContent,
         visibility:document.visibilityState,state:globalThis.partyBoardApp?.localGameState?.(),blobRead:globalThis.partyBoardBlobReadDiagnostic?.(),stage:globalThis.partyBoardActiveFrameStage,
         frameStageTiming:globalThis.partyBoardFrameStageTiming,pacingTiming:globalThis.partyBoardFramePacingTiming,pageErrors:globalThis.partyBoardPageErrors}))));}catch{}
    },5000);
    try {
    await page.waitForFunction(()=>document.querySelector('#status').textContent.startsWith('Local game running.')||
      document.querySelector('#status').textContent.startsWith('Local game unavailable:'),{},{timeout:180000});
    assert.equal(await page.locator('#status').textContent(),
      'Local game running. Online Multiplayer hands off to the Host/Join peer lobby.',JSON.stringify(messages.slice(-30)));
    assert.match(await page.locator('#speaker-status').textContent(),/source: 6 channels/,
      'The selected channel layout must be applied before the audio stream opens.');
    assert.equal(await page.locator('#speaker-layout').inputValue(),'6','Preselected 5.1 preference must survive runtime preparation.');
    const waitFrame=frame=>page.waitForFunction(frame=>partyBoardApp.localGameState()?.frame>=frame,frame,{timeout:frameTimeout});
    if(profileFrames){
      await waitFrame(960);
      const read=()=>page.evaluate(()=>({state:partyBoardApp.localGameState(),timing:globalThis.partyBoardFrameStageTiming,
        pacing:globalThis.partyBoardFramePacingTiming,renderer:globalThis.partyBoardRendererStageTiming,
        submission:globalThis.partyBoardRendererSubmissionTiming,staging:globalThis.partyBoardRendererStagingTiming,
        visibility:document.visibilityState,observedAt:Date.now()}));
      const before=await read();assert.ok(before.timing,'Compiled browser frame timing counters required.');
      await waitFrame(before.state.frame+profileFrames);
      const after=await read(),summary=summarizeFrameTiming(before.timing,after.timing);
      await page.locator('#stop-game').click();
      await page.waitForFunction(()=>document.querySelector('#status').textContent==='Local game stopped.');
      assert.deepEqual(errors,[]);
      const profile={scope:'Actual production browser local-play boot/idle frame window; one runtime, muted. Not a full scene, GPU attribution or multiplayer performance acceptance.',
        requestedFrames:profileFrames,before,after,summary,cleanStop:true,errors,
        logicalRetraceRateVerified:false,expectedSceneCadenceVerified:false};
      fs.writeFileSync(path.join(outputDirectory,'browser-local-frame-profile.json'),JSON.stringify(profile,null,2)+'\n',{flag:'wx'});
      console.log(JSON.stringify({frameProfile:summary,cleanStop:true}));return;
    }
    await waitFrame(950);await page.locator('#canvas').screenshot({path:path.resolve(outputDirectory,'browser-launch-before-start.png')});
    await page.locator('#canvas').click();
    const enterDeadline=Date.now()+120000;
    while(!messages.some(message=>message.includes('Start New OVL 74 '))) {
      if(Date.now()>enterDeadline)throw Error('Actual boot did not reach mode selection.');
      await page.keyboard.press('Enter',{delay:120});await page.waitForTimeout(1000);
    }
    await page.locator('#canvas').screenshot({path:path.resolve(outputDirectory,'browser-launch-before-confirm.png')});
    const confirmDeadline=Date.now()+60000;
    while(!messages.some(message=>message.includes('Online Multiplayer entry: available'))) {
      if(Date.now()>confirmDeadline)throw Error('Actual mode selection did not open.');
      await page.keyboard.press('z',{delay:30});await page.waitForTimeout(500);
    }
    const waitMessage=async marker=>{
      const deadline=Date.now()+60000;
      while(!messages.some(message=>message.includes(marker))) {
        if(Date.now()>=deadline)throw Error('Game marker timed out: '+marker);
        await page.waitForTimeout(200);
      }
    };
    await waitMessage('Online Multiplayer entry: available');
    const selectorFrame=await page.evaluate(()=>partyBoardApp.localGameState().frame);
    await waitFrame(selectorFrame+30);
    assert.equal(await page.evaluate(()=>partyBoardApp.localGameState().progress.overlay),74,'Online entry must remain in the actual mode selector before confirmation');
    await page.keyboard.press('s',{delay:400});
    const selectionFrame=await page.evaluate(()=>partyBoardApp.localGameState().frame);
    await waitFrame(selectionFrame+10);
    await page.keyboard.press('z',{delay:30});
    await waitMessage('Online Multiplayer menu: opened');
    const menuFrame=await page.evaluate(async()=>(await import('./app.mjs')).localGameState().frame);
    await waitFrame(menuFrame+10);
    await page.locator('#canvas').screenshot({path:path.resolve(outputDirectory,'browser-production-menu-glyphs.png')});
    if(process.env.PARTYBOARD_TEST_WINDOW_GLYPHS==='1') {
      for(const bank of [2,3])assert.ok(messages.some(message=>message.includes('Browser window glyph installed: bank '+bank)));
    }
    if(join)await page.keyboard.press('s',{delay:120});
    await page.keyboard.press('z',{delay:300});
    if(join) {
      await page.waitForFunction(()=>document.querySelector('#status').textContent==='Paste the host invitation, then choose Join.',{},{timeout:45000});
      assert.ok(messages.some(m=>m.includes('Join requested; frontend accepted')));
      assert.equal(await page.evaluate(()=>document.activeElement.id),'remote');
      assert.equal(await page.locator('#local').inputValue(),'');
    } else {
    await page.waitForFunction(()=>document.querySelector('#local').value.length>0,{},{timeout:45000});
    assert.ok(messages.some(m=>m.includes('Host requested; frontend accepted')),JSON.stringify(messages.slice(-20)));
    assert.equal(await page.evaluate(async()=>(await import('./app.mjs')).localGameState()),null);
    assert.equal(await page.locator('#game-view').isVisible(),false);
    assert.equal(await page.locator('#answer').isDisabled(),false);
    const invitation=JSON.parse(Buffer.from(await page.locator('#local').inputValue(),'base64').toString());
    assert.equal(invitation.description.type,'offer');
    assert.equal(invitation.playerId,'guest1');
    }
    assert.equal(await page.evaluate(async()=>(await import('./app.mjs')).localGameState()),null);
    assert.equal(await page.locator('#game-view').isVisible(),false);
    if(process.env.PARTYBOARD_CAPTURE_PIPELINES==='1'){
      const captured=await page.evaluate(async()=>{
        const seed=(await import('/app.mjs')).lastCapturedPipelineSeed();
        if(!seed)throw Error('No captured pipeline seed.');
        const bytes=new Uint8Array(await seed.blob.arrayBuffer());
        return {sha256:seed.sha256,scope:seed.scope,bytes:Array.from(bytes)};
      });
      const target=path.resolve(outputDirectory,'browser-observed-pipeline-cache.db');
      fs.writeFileSync(target,Buffer.from(captured.bytes));
      console.log(JSON.stringify({pipelineCapture:{path:target,sha256:captured.sha256,size:captured.bytes.length,scope:captured.scope}}));
    }
    assert.deepEqual(errors,[]);
    await page.screenshot({path:path.resolve(outputDirectory,'browser-game-'+(join?'join':'host')+'-handoff.png'),fullPage:true});
    await page.locator('#stop').click();
    console.log('Actual standalone game menu → cleanup → '+(join?'Join invitation entry':'real Host invitation')+' PASS. No online gameplay claim.');
    }catch(error){
      console.log('Game messages:',JSON.stringify(messages.slice(-100)));
      console.log('Game state:',await page.evaluate(async()=>(await import('./app.mjs')).localGameState()));
      await page.screenshot({path:path.resolve(outputDirectory,'browser-launch-handoff-failure.png'),fullPage:true});
      throw error;
    }finally{clearInterval(progress);}
  }finally{await browser?.close();server.kill();}
})().catch(error=>{console.error(error);process.exitCode=1;});
