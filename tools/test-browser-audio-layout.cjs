const fs=require('node:fs'),path=require('node:path'),http=require('node:http'),assert=require('node:assert/strict');
const {createHash}=require('node:crypto');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const directory=path.resolve(process.argv[2]);
const disc=process.argv[3] ? path.resolve(process.argv[3]) : null;
const layoutOnly=process.env.PARTYBOARD_TEST_AUDIO_LAYOUT_ONLY==='1';
if(layoutOnly&&!disc)throw Error('Prepared layout-only validation requires the original disc');
const server=http.createServer((req,res)=>{
  const files={'/contract.mjs':path.join(directory,'contract.mjs'),'/contract.wasm':path.join(directory,'contract.wasm'),'/audio-layout.mjs':path.resolve(__dirname,'../web/audio-layout.mjs'),'/async-blob-fs.mjs':path.resolve(__dirname,'../web/async-blob-fs.mjs')};
  if(req.url==='/disc.ciso' && disc) {res.setHeader('Content-Type','application/octet-stream');fs.createReadStream(disc).on('error',()=>res.destroy()).pipe(res);return;}
  if(req.url==='/') {res.setHeader('Content-Type','text/html');res.end('<canvas id="canvas" width="640" height="480"></canvas><select id="layout"><option value="2">Stereo</option><option value="6">5.1 Surround</option></select><p id="status"></p>');return;}
  if(!files[req.url]) {res.writeHead(404);res.end();return;}
  res.setHeader('Content-Type',req.url.endsWith('.wasm')?'application/wasm':'text/javascript');
  fs.createReadStream(files[req.url]).on('error',error=>{console.error('Audio validation file read failed:',req.url,error.code);res.destroy();}).pipe(res);
});
(async()=>{
  let browser,timer;
  try {
    await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
    browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});
    const page=await browser.newPage();
    page.on('requestfailed',request=>console.error('Audio validation request failed:',new URL(request.url()).pathname,request.failure()?.errorText));
    page.on('pageerror',error=>console.error('Audio validation page error:',error.message));
    await page.goto('http://127.0.0.1:'+server.address().port);
    await page.click('#canvas');
    const result=await Promise.race([page.evaluate(async({prepared,layoutOnly})=>{
      const {default:create}=await import('/contract.mjs');
      const logs=[];
      const module=await create({noInitialRun:true,canvas:document.getElementById('canvas'),print:line=>logs.push(line),printErr:line=>logs.push(line)});
      const rejected=[module._PartyBoardBrowserGetAudioChannels(),...[2,6,9].map(value=>module._PartyBoardBrowserSetAudioChannels(value))];
      if(module._PartyBoardBrowserAudioSourceChannels()!==0 || module._PartyBoardBrowserAudioDeviceChannels()!==0)
        throw Error('Unprepared runtime must report unavailable audio output');
      const {audioLayout}=await import('/audio-layout.mjs');
      const select=document.getElementById('layout'),label=document.getElementById('status');
      const control=audioLayout(select,label,localStorage);control.bind(module);
      if(prepared) {
        const response=await fetch('/disc.ciso');if(!response.ok)throw Error('Disc fetch failed');
        const blob=await response.blob();
        const {mountAsyncBlob}=await import('/async-blob-fs.mjs');mountAsyncBlob(module,blob);
        const setup=await module.ccall('PartyBoardBrowserPreparePlatform','number',[],[],{async:true});
        if(setup!==0)throw Error('Actual platform preparation failed: '+setup);
        if(module._PartyBoardBrowserAudioSourceChannels()!==0 || module._PartyBoardBrowserAudioDeviceChannels()!==0)
          throw Error('Unopened prepared runtime must report unavailable audio output');
        control.bind(module);select.value='6';select.dispatchEvent(new Event('change'));
        const actual=module._PartyBoardBrowserGetAudioChannels();
        const invalid=module._PartyBoardBrowserSetAudioChannels(9);
        if(layoutOnly) {
          if(module._PartyBoardBrowserGetAudioChannels()!==6)throw Error('Invalid layout mutated prepared preference');
          select.value='2';select.dispatchEvent(new Event('change'));
          if(module._PartyBoardBrowserGetAudioChannels()!==2)throw Error('Prepared stereo selector failed');
          select.value='6';select.dispatchEvent(new Event('change'));
          return {rejected,actual,invalid,retained:module._PartyBoardBrowserGetAudioChannels(),disabled:select.disabled,saved:localStorage.getItem('partyboard.audioChannels')};
        }
        const handle=await module.ccall('HuAudSStreamPlay','number',['number'],[1],{async:true});
        const source=module._PartyBoardBrowserAudioSourceChannels(),device=module._PartyBoardBrowserAudioDeviceChannels();
        if(handle<0 || source!==6 || device<=0)throw Error('Original music stream did not open selected source format: '+JSON.stringify({handle,source,device,logs:logs.slice(-12)}));
        select.value='2';select.dispatchEvent(new Event('change'));
        if(module._PartyBoardBrowserGetAudioChannels()!==2 || module._PartyBoardBrowserAudioSourceChannels()!==6)
          throw Error('Layout change must preserve active stream and apply to next stream');
        const queued=module._PartyBoardTestAudioTick();
        if(queued!==533*6*2)throw Error('Original first tick did not queue 533 six-channel frames: '+queued);
        const clockBefore=module.SDL3.audioContext.currentTime;
        await new Promise(resolve=>setTimeout(resolve,500));
        const clockAfter=module.SDL3.audioContext.currentTime,state=module.SDL3.audioContext.state;
        const afterDrain=module._PartyBoardTestAudioTick();
        if(afterDrain<533*6*2 || afterDrain>=queued+533*6*2 || clockAfter-clockBefore<0.1 || state!=='running')
          throw Error('Original PCM consumer did not advance: '+JSON.stringify({queued,afterDrain,clockBefore,clockAfter,state}));
        const sustainedStart=module.SDL3.audioContext.currentTime;
        let maximumQueue=afterDrain;
        for(let tick=0;tick<120;++tick) {
          await new Promise(resolve=>setTimeout(resolve,1000/60));
          const bytes=module._PartyBoardTestAudioTick();
          if(bytes<0 || bytes>32000/4*6*2)throw Error('Sustained original stream queue invalid: '+bytes);
          maximumQueue=Math.max(maximumQueue,bytes);
        }
        const sustainedEnd=module.SDL3.audioContext.currentTime;
        if(sustainedEnd-sustainedStart<1.5 || module.SDL3.audioContext.state!=='running')
          throw Error('Sustained original audio clock stalled: '+JSON.stringify({sustainedStart,sustainedEnd}));
        const destinationChannels=module.SDL3.audioContext.destination.channelCount;
        const destinationMaxChannels=module.SDL3.audioContext.destination.maxChannelCount;
        let completionTicks=0,closed=false;
        for(;completionTicks<180;++completionTicks) {
          await new Promise(resolve=>setTimeout(resolve,1000/60));
          const bytes=module._PartyBoardTestAudioTick();
          if(bytes===-1) {closed=true;break;}
          if(bytes<0 || bytes>32000/4*6*2)throw Error('Finite music completion queue invalid: '+bytes);
        }
        if(!closed || module._PartyBoardBrowserAudioSourceChannels()!==0 || module._PartyBoardBrowserAudioDeviceChannels()!==0)
          throw Error('Finite original music output did not close after completion');
        const stereoHandle=await module.ccall('HuAudSStreamPlay','number',['number'],[1],{async:true});
        const stereoSource=module._PartyBoardBrowserAudioSourceChannels();
        const stereoQueued=module._PartyBoardTestAudioTick();
        if(stereoHandle<=handle || stereoSource!==2 || stereoQueued!==533*2*2)
          throw Error('Next original stream did not use selected stereo format: '+JSON.stringify({stereoHandle,stereoSource,stereoQueued}));
        select.value='6';select.dispatchEvent(new Event('change'));
        if(module._PartyBoardBrowserAudioSourceChannels()!==2)throw Error('Second layout change mutated active stereo stream');
        console.info('Original PDT stream opened: source='+source+', device='+device+'; no audible acceptance');
        return {rejected,actual,invalid,retained:module._PartyBoardBrowserGetAudioChannels(),disabled:select.disabled,saved:localStorage.getItem('partyboard.audioChannels'),playback:{handle,source,device,queued,afterDrain,clockBefore,clockAfter,state,sustainedTicks:120,sustainedStart,sustainedEnd,maximumQueue,destinationChannels,destinationMaxChannels,completionTicks,closed,stereoHandle,stereoSource,stereoQueued}};
      }
      return {rejected,disabled:select.disabled,saved:localStorage.getItem('partyboard.audioChannels')};
    },{prepared:!!disc,layoutOnly}),new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error('Audio layout browser observation timed out')),disc?120000:30000);})]);
    clearTimeout(timer);
    const playback=result.playback;
    if(disc) {
      if(!layoutOnly)console.log('Actual original PDT output: '+JSON.stringify(result.playback)+'; no audible or speaker mapping acceptance.');
      delete result.playback;
    }
    assert.deepEqual(result,disc ? {rejected:[0,-1,-1,-1],actual:6,invalid:-1,retained:6,disabled:false,saved:'6'} : {rejected:[0,-1,-1,-1],disabled:true,saved:null});
    if(disc) {
      await page.reload();
      const restored=await page.evaluate(async()=>{
        const {audioLayout}=await import('/audio-layout.mjs');
        const select=document.getElementById('layout');
        audioLayout(select,document.getElementById('status'),localStorage);
        return {value:select.value,disabled:select.disabled,saved:localStorage.getItem('partyboard.audioChannels')};
      });
      assert.deepEqual(restored,{value:'6',disabled:true,saved:'6'});
      console.log('PASS: browser reload restores six-channel preference and waits for runtime preparation.');
    }
    console.log(disc ? 'PASS: actual disc-prepared Wasm audio layout changes through browser selector; invalid value preserves preference. No playback/device acceptance.' : 'PASS: actual Wasm audio API rejects unprepared access; browser selector stays disabled without saving.');
    if(process.argv[4]) {
      const digest=file=>createHash('sha256').update(fs.readFileSync(file)).digest('hex');
      const sources=['src/port/audio.c','src/port/pcm_output.h','src/port/settings.cpp','src/port/wasm_platform.cpp','web/audio-layout.mjs','tools/test-browser-audio-layout.cjs'];
      const report={timestamp:new Date().toISOString(),scope:layoutOnly?'Actual disc-prepared runtime audio selector, invalid value and reload preference only':disc?'Actual original PDT stream 1; isolated disc-prepared Edge runtime, selector and finite PCM queue lifecycle':'Actual isolated Wasm unprepared API rejection only',
        runtime:{javascript:digest(path.join(directory,'contract.mjs')),wasm:digest(path.join(directory,'contract.wasm'))},
        sources:Object.fromEntries(sources.map(file=>[file,digest(path.resolve(__dirname,'..',file))])),
        assertions:{unpreparedRejection:true,preparedSelection:!!disc,invalidSelectionPreservesState:!!disc,pageReloadPreference:!!disc,nextStreamStereoSwitch:!!disc&&!layoutOnly,finiteMusicQueueLifecycle:!!disc&&!layoutOnly},
        playback:playback||null,acceptance:{audiblePlayback:false,physicalSurround:false,allGameAudio:false,fullGameMultiplayer:false}};
      fs.writeFileSync(path.resolve(process.argv[4]),JSON.stringify(report,null,2)+'\n');
    }
  }finally {clearTimeout(timer);await browser?.close();server.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
