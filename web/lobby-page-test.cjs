// Exercise the retained diagnostic transport controls explicitly.
const manualURL=value=>{const url=new URL(value);url.searchParams.set('manual','1');return url.href;};
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const assert=require('node:assert/strict');
const fs=require('node:fs');
(async()=>{
 const browser=await chromium.launch({channel:process.env.PARTYBOARD_TEST_BROWSER_CHANNEL||'msedge',headless:true,args:['--mute-audio']});
 try {
  const pages=[],errors=[];
  for(let i=0;i<4;i++){const c=await browser.newContext(),p=await c.newPage();pages.push(p);p.on('pageerror',e=>errors.push(e.message));await p.goto(manualURL(process.env.PARTYBOARD_TEST_URL||'http://127.0.0.1:8785'));}
  if(process.env.PARTYBOARD_TEST_MUSIC) {
   await pages[2].evaluate(()=>{const original=HTMLMediaElement.prototype.play;let first=true;HTMLMediaElement.prototype.play=function(){if(first){first=false;return Promise.reject(new DOMException('Test autoplay block','NotAllowedError'));}return original.call(this);};});
  }
  const h=pages[0];await h.locator('#file').setInputFiles({name:'test.bin',mimeType:'application/octet-stream',buffer:Buffer.alloc(600123,91)});
  for(let i=1;i<4;i++) {
   if(i===2&&process.env.PARTYBOARD_TEST_CANCEL_INVITE) {
    const previous=await h.locator('#local').inputValue();await h.locator('#host').click();
    await h.waitForFunction(old=>document.querySelector('#local').value!==old,previous);
    await h.locator('#cancel-invite').click();
    assert.equal(await h.locator('#local').inputValue(),'');
    assert.equal(await h.locator('#answer').isDisabled(),true);
    await pages[1].waitForFunction(()=>document.querySelectorAll('#lobby-players progress').length===2);
   }
   const old=await h.locator('#local').inputValue();await h.locator('#host').click();
   await h.waitForFunction(old=>document.querySelector('#local').value!==old&&document.querySelector('#local').value.length>0,old);
   await pages[i].locator('#remote').fill(await h.locator('#local').inputValue());await pages[i].locator('#join').click();
   await pages[i].waitForFunction(()=>document.querySelector('#local').value.length>0);
   if(process.env.PARTYBOARD_TEST_MUSIC) {
    if(i===2){assert.equal(await pages[i].locator('#music-toggle').textContent(),'Play music');await pages[i].locator('#music-toggle').click();}
    await pages[i].waitForFunction(()=>document.querySelector('#lobby-audio').currentTime>0&&!document.querySelector('#lobby-audio').paused);
   }
   await h.locator('#remote').fill(await pages[i].locator('#local').inputValue());await h.locator('#answer').click();
   await pages[i].waitForFunction(()=>!document.querySelector('#accept').disabled&&document.querySelectorAll('#lobby-players progress').length>=2);
  }
  const active=process.env.PARTYBOARD_TEST_GUEST_LEAVES?3:4;
  if(active===3) {
   await pages[3].locator('#stop').click();
   for(const page of pages.slice(0,3))await page.waitForFunction(()=>document.querySelectorAll('#lobby-players progress').length===3);
  }
  await Promise.all(pages.slice(1,active).map(p=>p.locator('#accept').click()));
  for(let i=0;i<active;i++) {
   await pages[i].waitForFunction(active=>document.querySelectorAll('#lobby-players progress').length===active&&[...document.querySelectorAll('#lobby-players progress')].every(p=>p.value===1),active);
   assert.match(await pages[i].locator('#lobby-players h3').nth(i).textContent(),/\(You\)$/);
   await pages[i].waitForFunction(active=>[...document.querySelectorAll('#lobby-players article p')].filter(p=>p.textContent.includes('Awaiting shader preparation')).length===active,active);
  }
  assert.equal(await h.locator('#host').isDisabled(),active===4);
  for(const guest of pages.slice(1,active))await guest.waitForFunction(()=>document.querySelector('#status').textContent.startsWith('Assets checked and prepared.'));
  if(process.env.PARTYBOARD_TEST_REPLACE_GUEST) {
   const g=pages[3],oldReply=await g.locator('#local').inputValue();
   await g.locator('#stop').click();await h.waitForFunction(()=>!document.querySelector('#host').disabled);
   const oldInvite=await h.locator('#local').inputValue();await h.locator('#host').click();
   await h.waitForFunction(old=>document.querySelector('#local').value!==old,oldInvite);
   await h.locator('#remote').fill(oldReply);await h.locator('#answer').click();
   assert.match(await h.locator('#status').textContent(),/another invitation/);
   await g.locator('#remote').fill(await h.locator('#local').inputValue());await g.locator('#join').click();
   await g.waitForFunction(()=>document.querySelector('#local').value.length>0);
   await h.locator('#remote').fill(await g.locator('#local').inputValue());await h.locator('#answer').click();
   await g.waitForFunction(()=>!document.querySelector('#accept').disabled&&document.querySelectorAll('#lobby-players progress').length===4);
   await g.locator('#accept').click();
   for(const p of pages)await p.waitForFunction(()=>document.querySelectorAll('#lobby-players progress').length===4&&[...document.querySelectorAll('#lobby-players progress')].every(n=>n.value===1));
   assert.match(await g.locator('#lobby-players h3').nth(3).textContent(),/Guest 4 \(You\)/);
  }
  if(process.env.PARTYBOARD_TEST_MUSIC) {
   for(const p of pages.slice(1,active))assert.equal(await p.locator('#lobby-audio').evaluate(a=>a.paused),false);
   assert.equal(await h.locator('#lobby-audio').evaluate(a=>a.hasAttribute('src')),false);
  }
  let routeEvidence;
  if(process.env.PARTYBOARD_TEST_ROUTES){
   await h.waitForFunction(count=>(document.querySelector('#connection-route')?.textContent.match(/guest\d+: Direct/g)||[]).length===count,active-1);
   for(const page of pages.slice(1,active))await page.waitForFunction(()=>document.querySelector('#connection-route')?.textContent==='Host: Direct');
   routeEvidence={host:await h.locator('#connection-route').textContent(),guests:await Promise.all(pages.slice(1,active).map(page=>page.locator('#connection-route').textContent())),actualRelayedConnectionVerified:false,gameRuntimeExecuted:false};
  }
  await h.locator('#stop').click();
  if(process.env.PARTYBOARD_TEST_MUSIC)for(const p of pages.slice(1,active))await p.waitForFunction(()=>document.querySelector('#lobby-audio').paused&&!document.querySelector('#lobby-audio').hasAttribute('src'));
  assert.equal(await h.locator('#lobby-players article').count(),0);
  if(routeEvidence){
   for(const page of pages.slice(0,active))await page.waitForFunction(()=>document.querySelector('#connection-route')?.textContent==='Connection: not connected.');
   routeEvidence.cleanupVerified=true;
   if(process.env.PARTYBOARD_TEST_ROUTES_REPORT)fs.writeFileSync(process.env.PARTYBOARD_TEST_ROUTES_REPORT,JSON.stringify(routeEvidence,null,2)+'\n');
  }
  assert.deepEqual(errors,[]);console.log('PASS: actual page completed downloads for '+active+' remaining players with correct local labels and cleanup.');
 }finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
