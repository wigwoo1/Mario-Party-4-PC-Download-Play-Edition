const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright'),assert=require('node:assert/strict'),{spawn}=require('node:child_process'),path=require('node:path');
(async()=>{
 const port=8952,server=spawn(process.execPath,[path.join(__dirname,'serve.cjs')],{env:{...process.env,PARTYBOARD_PORT:String(port)},stdio:['ignore','pipe','inherit']});let browser,release;
 try{
  await new Promise((r,j)=>{server.stdout.once('data',r);server.once('error',j);});
  browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});const page=await browser.newPage(),errors=[];let requests=0;
  page.on('pageerror',e=>errors.push(e.message));page.on('request',r=>{if(new URL(r.url()).pathname.startsWith('/api/lobbies'))requests++;});
  await page.goto('http://127.0.0.1:'+port+'/');
  for(const id of ['host','join','ready-game','prepare-game','answer'])assert.equal(await page.locator('#'+id).isVisible(),false);
  assert.equal(await page.getByRole('button',{name:'Customize Controls',exact:true}).count(),1);
  assert.match(await page.locator('#room-service-note').textContent(),/not deployed/);
  await page.evaluate(()=>{document.querySelector('#recovery-panel').hidden=false;for(const node of document.querySelectorAll('[data-recovery-manual]'))node.hidden=true;});
  for(const id of ['recovery-remote','recovery-local','recovery-peer','recovery-copy'])assert.equal(await page.locator('#'+id).isVisible(),false,'Hidden recovery controls: '+id);
  await page.evaluate(()=>{document.querySelector('#recovery-panel').hidden=true;});
  let entered;const waiting=new Promise(r=>entered=r),hold=new Promise(r=>release=r);
  await page.route('**/initial_pipeline_cache.json',async route=>{entered();await hold;await route.fulfill({status:500,body:'cancelled fixture'}).catch(()=>{});});
  await page.locator('#file').setInputFiles({name:'private-fixture.bin',mimeType:'application/octet-stream',buffer:Buffer.from('private')});
  await page.locator('#host-game').click();await waiting;await page.locator('#stop').click();
  await page.waitForFunction(()=>document.querySelector('#status').textContent.startsWith('Session stopped.'));
  release();await page.waitForTimeout(150);assert.equal(requests,0,'Cancelled shader loading must not open a late room');assert.equal(await page.locator('#host-game').isDisabled(),false);
  assert.match(await page.locator('#status').textContent(),/^Session stopped\./);assert.deepEqual(errors,[]);
  await page.goto('http://127.0.0.1:'+port+'/?manual=1');assert.equal(await page.locator('#host').isVisible(),true);assert.equal(await page.locator('#prepare-game').isVisible(),true);assert.equal(await page.locator('#host-game').isVisible(),false);
  console.log('PASS default automatic controls, retained explicit diagnostics, device control editor and Stop during pending setup.');
 }finally{release?.();await browser?.close();server.kill();}
})().catch(e=>{console.error(e);process.exitCode=1;});
