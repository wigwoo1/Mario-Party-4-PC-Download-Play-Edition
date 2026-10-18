// Exercise the retained diagnostic transport controls explicitly.
const manualURL=value=>{const url=new URL(value);url.searchParams.set('manual','1');return url.href;};
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const assert=require('node:assert/strict');
(async()=>{
 const browser=await chromium.launch({channel:process.env.PLAYWRIGHT_CHANNEL||'msedge',headless:true,args:['--mute-audio']});
 try{
  const errors=[],pages=[];
  for(let i=0;i<2;++i){const context=await browser.newContext(),page=await context.newPage();
   page.on('pageerror',error=>errors.push(error.message));pages.push(page);
   await page.goto(manualURL(process.env.PARTYBOARD_TEST_URL||'http://127.0.0.1:8814'));}
  const [host,guest]=pages;
  // Tiny synthetic transfer fixture only; no game/runtime preparation claim.
  await host.locator('#file').setInputFiles({name:'join-music-fixture.bin',mimeType:'application/octet-stream',buffer:Buffer.alloc(65536,37)});
  await host.locator('#host').click();
  try { await host.waitForFunction(()=>document.querySelector('#local').value.length>0); }
  catch(error){
   console.error('Host invitation diagnostics:',JSON.stringify({errors,page:await host.evaluate(()=>({text:document.body.innerText,hostDisabled:document.querySelector('#host').disabled,invitationLength:document.querySelector('#local').value.length}))}));
   throw error;
  }
  await guest.locator('#remote').fill(await host.locator('#local').inputValue());
  await guest.locator('#join').click();
  await guest.waitForFunction(()=>document.querySelector('#local').value.length>0);
  await host.locator('#remote').fill(await guest.locator('#local').inputValue());
  await host.locator('#answer').click();
  await guest.locator('#accept').waitFor({state:'visible'});
  await guest.waitForFunction(()=>!document.querySelector('#accept').disabled);
  await guest.locator('#accept').click();
  const playing=track=>{const audio=document.querySelector('#lobby-audio');return audio.currentSrc.endsWith(track)&&!audio.paused&&audio.readyState>=2&&audio.error===null;};
  // No Play music click or autoplay-policy launch flag is allowed.
  for(const [index,track] of ['wifi-menu.opus','currently-spectating.opus','wifi-menu.opus'].entries()){
   if(index)await guest.evaluate(()=>{const audio=document.querySelector('#lobby-audio');if(!Number.isFinite(audio.duration)||audio.duration<=1)throw Error('Decoded duration required');audio.currentTime=audio.duration-0.2;});
   await guest.waitForFunction(playing,track,{timeout:30000});
   const time=await guest.evaluate(()=>document.querySelector('#lobby-audio').currentTime);
   await guest.waitForFunction(time=>document.querySelector('#lobby-audio').currentTime>time+0.5,time,{timeout:10000});
  }
  await guest.locator('#stop').click();
  await guest.waitForFunction(()=>{const audio=document.querySelector('#lobby-audio');return audio.paused&&!audio.hasAttribute('src');});
  assert.deepEqual(errors,[]);
  console.log('PASS: actual production Join automatically starts both Opus tracks and wraps with advancing clocks; stop releases audio. Synthetic asset only; no audible or game acceptance.');
 }finally{await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
