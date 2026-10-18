const http=require('node:http'),fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const server=http.createServer((req,res)=>{
 if(req.url==='/'){res.end('<!doctype html><title>Lobby controls test</title><div id="controls"></div>');return;}
 if(!['/lobby-control-editor.mjs','/control-bindings.mjs'].includes(req.url)){res.writeHead(404);res.end();return;}
 res.setHeader('Content-Type','text/javascript');res.end(fs.readFileSync(path.join(__dirname,req.url.slice(1))));
});
(async()=>{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));let browser;
 try{
  browser=await chromium.launch({channel:process.env.PLAYWRIGHT_CHANNEL||'msedge',headless:true,args:['--mute-audio']});
  const page=await browser.newPage(),errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  await page.evaluate(async()=>{const {lobbyControlEditor}=await import('/lobby-control-editor.mjs');globalThis.editor=lobbyControlEditor(document.querySelector('#controls'));globalThis.escapedKeys=0;document.addEventListener('keydown',()=>escapedKeys++);});
  const open=()=>page.getByRole('button',{name:'Customize Controls'}).click();
  const apply=()=>page.getByRole('button',{name:'Apply controls'}).click();
  const cancel=()=>page.getByRole('button',{name:'Cancel',exact:true}).click();
  const reset=()=>page.getByRole('button',{name:'Reset to defaults'}).click();
  const profile=()=>page.evaluate(()=>editor.profile());
  const initial=await profile();
  await open();await page.getByLabel('Keyboard: A button',{exact:true}).selectOption('Space');await apply();
  assert.equal((await profile()).keyboard.a,'Space');
  await open();await page.getByLabel('Keyboard: A button',{exact:true}).selectOption('KeyX');await apply();
  assert.equal(await page.locator('fieldset').isVisible(),true);
  assert.ok(await page.getByRole('status').textContent());assert.equal((await profile()).keyboard.a,'Space');await cancel();
  await open();await reset();await cancel();assert.equal((await profile()).keyboard.a,'Space');
  await open();await reset();await apply();assert.deepEqual(await profile(),initial);
  await open();await page.getByLabel('Controller: L trigger',{exact:true}).selectOption('button:4');await apply();
  assert.equal((await profile()).controller.l,'button:4');assert.equal((await profile()).controller.stickUp,initial.controller.stickUp);
  await open();await page.getByLabel('Keyboard: A button',{exact:true}).focus();await page.keyboard.press('ArrowDown');
  assert.equal(await page.evaluate(()=>escapedKeys),0);
  await page.evaluate(()=>editor.setLocked(true));assert.equal(await page.getByRole('button',{name:'Customize Controls'}).isDisabled(),true);
  assert.equal(await page.locator('fieldset').isVisible(),false);
  await page.evaluate(()=>editor.setLocked(false));await open();assert.equal(await page.locator('fieldset').isVisible(),true);
  assert.equal((await profile()).controller.l,'button:4');assert.deepEqual(errors,[]);
  const restored=await page.evaluate(async()=>{const mount=document.createElement('div');document.body.append(mount);const {lobbyControlEditor}=await import('/lobby-control-editor.mjs');globalThis.reopenedEditor=lobbyControlEditor(mount);return reopenedEditor.profile();});
  assert.deepEqual(restored,await profile(),'Accepted keyboard and controller bindings persist for the same host/client browser after rebuilding the page editor.');
  const saved=await profile();
  await page.reload();
  const afterReload=await page.evaluate(async()=>{
   const {lobbyControlEditor}=await import('/lobby-control-editor.mjs');
   return lobbyControlEditor(document.querySelector('#controls')).profile();
  });
  assert.deepEqual(afterReload,saved,'Accepted bindings survive a full browser page reload.');
  assert.deepEqual(errors,[]);
  console.log('PASS '+(process.env.PLAYWRIGHT_CHANNEL||'msedge')+' lobby keyboard/controller edits, conflicts, cancel/reset, preparation lock, key isolation and full-page persistence');
 }finally{await browser?.close();await new Promise(r=>server.close(r));}
})().catch(e=>{console.error(e);process.exitCode=1;});
