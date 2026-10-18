const http=require('node:http'),fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const server=http.createServer((req,res)=>{
  if(req.url==='/') {res.end('<!doctype html><title>Input glyph test</title><canvas id="game" tabindex="0" width="320" height="200"></canvas><input id="chat"><div id="prompt"></div>');return;}
  if(!['/browser-input-glyphs.mjs','/input-glyph-state.mjs','/control-glyphs.mjs','/glyph-texture.mjs'].includes(req.url)){res.writeHead(404);res.end();return;}
  res.setHeader('Content-Type','text/javascript');res.end(fs.readFileSync(path.join(__dirname,req.url.slice(1))));
});
(async()=>{
  await new Promise(r=>server.listen(0,'127.0.0.1',r));let browser;
  try {
    browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});
    const page=await browser.newPage(),errors=[];page.on('pageerror',e=>errors.push(e.message));
    await page.goto(`http://127.0.0.1:${server.address().port}`);
    await page.evaluate(async()=>{
      const {attachInputGlyphs}=await import('/browser-input-glyphs.mjs');
      globalThis.keyLabel='Enter';globalThis.updates=0;
      globalThis.adapter=attachInputGlyphs({target:document.querySelector('#game'),
        bindings:()=>({keyboard:{confirm:{kind:'key',label:keyLabel}}}),
        onChange:s=>{updates++;document.querySelector('#prompt').innerHTML=s.glyphs.confirm?.svg||'';}});
    });
    await page.locator('#game').focus();await page.keyboard.press('Enter');
    await page.waitForFunction(()=>document.querySelector('#prompt title')?.textContent==='Enter');
    await page.evaluate(()=>{keyLabel='Space';adapter.refresh();});
    assert.equal(await page.locator('#prompt title').textContent(),'Space');
    await page.locator('#chat').fill('Lobby text must not change prompts');
    assert.equal(await page.locator('#prompt title').textContent(),'Space');
    await page.evaluate(()=>{adapter.close();globalThis.closedUpdates=updates;keyLabel='Esc';});
    await page.locator('#game').focus();await page.keyboard.press('Escape');
    assert.equal(await page.evaluate(()=>updates===closedUpdates),true);
    const texture=await page.evaluate(async()=>{
      const {glyphTexture,gxRgba8}=await import('/glyph-texture.mjs');
      const normal=await glyphTexture({family:'playstation',binding:{kind:'button',button:'south'}});
      const pressed=await glyphTexture({binding:{kind:'key',label:'Enter'},pressed:true});
      const tiled=gxRgba8(normal);
      let exact=true;
      for(let y=0;y<64;y++)for(let x=0;x<64;x++) {
        const s=(y*64+x)*4,d=((y>>2)*16+(x>>2))*64+((y&3)*4+(x&3))*2;
        exact&&=normal.rgba[s]===tiled[d+1]&&normal.rgba[s+1]===tiled[d+32]&&normal.rgba[s+2]===tiled[d+33]&&normal.rgba[s+3]===tiled[d];
      }
      return {label:normal.label,bytes:tiled.length,exact,transparent:normal.rgba[3]===0,visible:normal.rgba.some((v,i)=>i%4===3&&v>0),pressedVisible:pressed.rgba.some((v,i)=>i%4===3&&v>0)};
    });
    assert.deepEqual(texture,{label:'Cross',bytes:16384,exact:true,transparent:true,visible:true,pressedVisible:true});
    assert.deepEqual(errors,[]);
    console.log('PASS Edge glyph events, remaps, focus, teardown and RGBA/GX texture conversion');
  } finally {await browser?.close();await new Promise(r=>server.close(r));}
})().catch(e=>{console.error(e);process.exitCode=1;});
