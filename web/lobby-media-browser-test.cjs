const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const {spawn}=require('node:child_process');const fs=require('node:fs');const assert=require('node:assert/strict');
(async()=>{
 const server=spawn(process.execPath,[require('node:path').join(__dirname,'serve.cjs')],{env:{...process.env,PARTYBOARD_TEST_PORT:'8793'},stdio:['ignore','pipe','inherit']});let browser,page;
 try{
  await new Promise(resolve=>server.stdout.once('data',resolve));
  const url='http://127.0.0.1:8793/audio/wifi-menu.opus',bytes=fs.readFileSync(require('node:path').join(__dirname,'audio/wifi-menu.opus'));
  const head=await fetch(url,{method:'HEAD'});assert.equal(head.status,200);assert.equal(Number(head.headers.get('content-length')),bytes.length);assert.equal(head.headers.get('accept-ranges'),'bytes');
  for(const [range,start,end] of [['bytes=0-31',0,31],['bytes=-32',bytes.length-32,bytes.length-1],['bytes='+String(bytes.length-32)+'-',bytes.length-32,bytes.length-1]]){
   const response=await fetch(url,{headers:{Range:range}});assert.equal(response.status,206);assert.equal(response.headers.get('content-range'),`bytes ${start}-${end}/${bytes.length}`);assert.deepEqual(Buffer.from(await response.arrayBuffer()),bytes.subarray(start,end+1));
  }
  for(const range of ['bytes=-0','bytes=999999999-','bytes=32-0'])assert.equal((await fetch(url,{headers:{Range:range}})).status,416);
  assert.equal((await fetch('http://127.0.0.1:8793/game.ciso')).status,404);
  browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});page=await browser.newPage();await page.goto('http://127.0.0.1:8793');
  await page.evaluate(async()=>{const {lobbyMusic}=await import('/lobby-music.mjs');globalThis.testMusic=lobbyMusic(document.querySelector('#lobby-audio'),document.querySelector('#music-toggle'),document.querySelector('#music-title'),document.querySelector('#music-volume'));document.body.addEventListener('click',()=>testMusic.start(),{once:true});});
  await page.locator('h1').click();
  for(const track of ['wifi-menu.opus','currently-spectating.opus','wifi-menu.opus']){
   await page.waitForFunction(track=>{const audio=document.querySelector('#lobby-audio');return audio.currentSrc.endsWith(track)&&!audio.paused&&audio.currentTime>0&&Number.isFinite(audio.duration)&&audio.duration>1;},track,{timeout:30000});
   console.log('Actual decoded finite-duration track: '+JSON.stringify(await page.locator('#lobby-audio').evaluate(audio=>({src:audio.currentSrc,duration:audio.duration}))));
   const firstTime=await page.locator('#lobby-audio').evaluate(audio=>audio.currentTime);
   await page.waitForFunction(time=>document.querySelector('#lobby-audio').currentTime>time+0.5,firstTime,{timeout:10000});
   if(track==='wifi-menu.opus'&&await page.evaluate(()=>globalThis.wrapped===true))break;
   await page.evaluate(()=>{const audio=document.querySelector('#lobby-audio');if(audio.currentSrc.includes('spectating'))globalThis.wrapped=true;audio.currentTime=audio.duration-0.2;});
  }
  await page.evaluate(()=>testMusic.stop());assert.equal(await page.locator('#lobby-audio').evaluate(audio=>audio.paused&&!audio.hasAttribute('src')),true);
  console.log('PASS real byte-range media responses, Opus metadata, natural ended playlist wrap and cleanup. Audible listening and guest transfer integration remain unverified here.');
 }catch(error){
  if(page)console.error('Actual media failure state: '+JSON.stringify(await page.evaluate(()=>{const audio=document.querySelector('#lobby-audio');return {
   src:audio.currentSrc,time:audio.currentTime,duration:audio.duration,paused:audio.paused,ended:audio.ended,seeking:audio.seeking,
   readyState:audio.readyState,networkState:audio.networkState,error:audio.error?{code:audio.error.code,message:audio.error.message}:null,
   button:document.querySelector('#music-toggle')?.textContent,title:document.querySelector('#music-title')?.textContent};})));
  throw error;
 }finally{await browser?.close();server.kill();}
})().catch(error=>{console.error(error);process.exitCode=1;});

