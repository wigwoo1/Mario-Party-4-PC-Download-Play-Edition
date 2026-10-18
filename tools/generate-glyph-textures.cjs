// Export our generated keycaps for native GX loading, without game assets.
const http=require('node:http'),fs=require('node:fs/promises'),path=require('node:path');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const keys=[...'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789','Enter','Space','Esc','Tab','Shift','Ctrl','Alt'];
const inputs=keys.map(label=>({label,binding:{kind:'key',label}}));
for(const family of ['xbox','playstation','switch','generic'])for(const button of ['south','east','west','north','l1','r1','l2','r2','start','select'])inputs.push({label:`${family}-${button}`,family,binding:{kind:'button',button}});
const output=path.resolve(__dirname,'../res/control-glyphs');
const server=http.createServer(async(req,res)=>{
  if(req.url==='/'){res.end('<!doctype html><title>Glyph export</title>');return;}
  if(!['/glyph-texture.mjs','/control-glyphs.mjs'].includes(req.url)){res.writeHead(404);res.end();return;}
  try {res.setHeader('Content-Type','text/javascript');res.end(await fs.readFile(path.join(__dirname,'../web',req.url.slice(1))));}
  catch {res.writeHead(500);res.end();}
});
(async()=>{
  await new Promise(r=>server.listen(0,'127.0.0.1',r));let browser;
  try {
    browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});
    const page=await browser.newPage();await page.goto(`http://127.0.0.1:${server.address().port}`);
    const textures=await page.evaluate(async inputs=>{
      const {glyphTexture,gxRgba8}=await import('/glyph-texture.mjs');
      const result=[];
      for(const {label,family,binding} of inputs) {
        const texture=await glyphTexture({family,binding,size:32});
        result.push({label,bytes:Array.from(gxRgba8(texture))});
      }
      return result;
    },inputs);
    await fs.mkdir(output,{recursive:true});
    const manifest={version:1,format:'GX_RGBA8',width:32,height:32,glyphs:{}};
    for(const {label,bytes} of textures) {
      if(bytes.length!==4096)throw Error('Invalid texture length');
      const filename=`key-${label}.rgba8`;
      await fs.writeFile(path.join(output,filename),Buffer.from(bytes));
      manifest.glyphs[label]=filename;
    }
    await fs.writeFile(path.join(output,'manifest.json'),JSON.stringify(manifest,null,2)+'\n');
    console.log(`Generated ${textures.length} native key textures, ${textures.length*4096} bytes.`);
  } finally {await browser?.close();await new Promise(r=>server.close(r));}
})().catch(error=>{console.error(error);process.exitCode=1;});
