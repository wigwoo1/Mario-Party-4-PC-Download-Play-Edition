'use strict';
const fs=require('node:fs'),path=require('node:path'),http=require('node:http'),assert=require('node:assert/strict');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const output=path.resolve(process.env.PARTYBOARD_TEST_OUTPUT_DIRECTORY||'outputs/validation/start-glyphs-edge-'+Date.now());
assert.equal(fs.existsSync(output),false);fs.mkdirSync(output,{recursive:true});
const modules=['runtime-glyphs.mjs','browser-input-glyphs.mjs','input-glyph-state.mjs','control-glyphs.mjs','glyph-texture.mjs','control-bindings.mjs'];
const server=http.createServer((req,res)=>{
 if(req.url==='/'){res.end('<!doctype html><title>Production Start glyph rendering</title><canvas id="game" tabindex="0" width="680" height="300"></canvas>');return;}
 if(!modules.includes(req.url.slice(1))){res.writeHead(404);res.end();return;}
 res.setHeader('Content-Type','text/javascript');res.end(fs.readFileSync(path.join(__dirname,req.url.slice(1))));
});
(async()=>{
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));let browser;
 try{
  browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});
  const page=await browser.newPage(),errors=[];page.on('pageerror',error=>errors.push(error.message));
  await page.goto('http://127.0.0.1:'+server.address().port);await page.locator('#game').focus();
  const report=await page.evaluate(async()=>{
   const {prepareRuntimeGlyphs}=await import('/runtime-glyphs.mjs');
   const {defaultControlBindings,controlBindingGlyphs}=await import('/control-bindings.mjs');
   const {glyphTexture,gxRgba8}=await import('/glyph-texture.mjs');
   const target=document.querySelector('#game'),context=target.getContext('2d');context.fillStyle='#19263a';context.fillRect(0,0,680,300);
   const results=[];
   async function check(profile,controller,button,label){
    const glyphs=await prepareRuntimeGlyphs({bindings:controlBindingGlyphs(profile)}),module={partyBoardGlyphTextures:glyphs.initial};
    let poll;const pad={index:0,id:'DualSense Wireless Controller',mapping:'standard',connected:true,buttons:Array.from({length:17},()=>({pressed:false,value:0})),axes:[0,0,0,0]};
    const environment={document,navigator:{getGamepads:()=>[pad]},requestAnimationFrame:callback=>(poll=callback,1),cancelAnimationFrame:()=>{}};
    const bridge=glyphs.attach(module,target,{environment});poll();
    if(controller){pad.buttons[button].pressed=true;pad.buttons[button].value=1;poll();}
    const expected=gxRgba8(await glyphTexture(controller?{family:'playstation',binding:{kind:'button',button:button===9?'start':'select'}}:{binding:{kind:'key',label}}));
    const actual=module.partyBoardGlyphTextures[12];if(!actual||actual.length!==16384||!actual.every((value,index)=>value===expected[index]))throw Error('Production Start texture differs: '+label);
    const rgba=new Uint8ClampedArray(16384);
    for(let y=0;y<64;y++)for(let x=0;x<64;x++){const p=(y*64+x)*4,t=((y>>2)*16+(x>>2))*64+((y&3)*4+(x&3))*2;rgba[p]=actual[t+1];rgba[p+1]=actual[t+32];rgba[p+2]=actual[t+33];rgba[p+3]=actual[t];}
    const glyph=document.createElement('canvas');glyph.width=glyph.height=64;glyph.getContext('2d').putImageData(new ImageData(rgba,64,64),0,0);
    const row=results.length*72;context.drawImage(glyph,12,row+24,20,24);context.drawImage(glyph,60,row+4,64,64);context.fillStyle='#fff';context.font='18px Arial';context.fillText(label+' — authored20x24 and64x64',150,row+42);
    bridge.close();results.push({label,bank:12,bytes:actual.length,controller,visible:rgba.some((value,index)=>index%4===3&&value>0)});
   }
   const defaults=defaultControlBindings();await check(defaults,false,null,'Enter');await check(defaults,true,9,'Options');
   const remapped=defaultControlBindings();remapped.keyboard.start='Space';remapped.controller.start='button:8';await check(remapped,false,null,'Space');await check(remapped,true,8,'Share');
   return {status:'VERIFIED actual Edge production glyph textures and active-device bridge',cases:results,simulatedStandardGamepad:true,physicalControllerVerified:false,gameWindowRenderingVerified:false};
  });
  assert.equal(report.cases.length,4);assert.ok(report.cases.every(row=>row.visible));assert.deepEqual(errors,[]);
  await page.locator('#game').screenshot({path:path.join(output,'start-glyphs.png')});
  fs.writeFileSync(path.join(output,'acceptance.json'),JSON.stringify(report,null,2)+'\n',{flag:'wx'});console.log(JSON.stringify(report));
 }finally{await browser?.close();await new Promise(resolve=>server.close(resolve));}
})().catch(error=>{console.error(error);process.exitCode=1;});
