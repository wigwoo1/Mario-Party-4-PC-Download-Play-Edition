// Real browser layout regression; the separate peer test exercises actual SDL.
const fs=require('node:fs'),path=require('node:path'),http=require('node:http'),assert=require('node:assert/strict');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const web=path.resolve(__dirname,'../web');
const html=fs.readFileSync(path.join(web,'index.html'),'utf8').replace(/<script\b[^>]*>[\s\S]*?<\/script>/g,'');
const server=http.createServer((req,res)=>{
 if(req.url==='/display-controls.mjs'){res.setHeader('Content-Type','text/javascript');res.end(fs.readFileSync(path.join(web,'display-controls.mjs')));}
 else{res.setHeader('Content-Type','text/html');res.end(html);}
});
(async()=>{
 let browser;
 try{
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  browser=await chromium.launch({channel:process.env.PARTYBOARD_TEST_BROWSER_CHANNEL||'chrome',headless:true,args:['--mute-audio']});
  const context=await browser.newContext({deviceScaleFactor:2.25}),page=await context.newPage();
  await page.goto('http://127.0.0.1:'+server.address().port);
  const result=await page.evaluate(async()=>{
   const view=document.querySelector('#game-view'),canvas=document.querySelector('#canvas');
   const original=document.createElement('div');original.hidden=true;original.innerHTML='<canvas></canvas>';document.body.append(original);
   const old=original.firstChild.getBoundingClientRect();
   if(old.width!==0||old.height!==0)throw Error('Original hidden layout did not reproduce zero CSS dimensions');
   original.remove();
   const measures=[];
   for(const width of [640,960,1280,1920,2560])for(const aspect of ['4 / 3','16 / 9','16 / 10','21 / 9']){
    canvas.style.setProperty('--game-preparation-width',width+'px');
    canvas.style.setProperty('--game-aspect',aspect);canvas.width=canvas.height=0;
    const rect=canvas.getBoundingClientRect(),[w,h]=aspect.split('/').map(Number);
    if(rect.width!==width||rect.height<=0||Math.abs(rect.width/rect.height-w/h)>.005)throw Error('Hidden canvas lost resolution or aspect '+aspect);
    const style=getComputedStyle(view);
    if(style.visibility!=='hidden'||rect.right>=0)throw Error('Prepared view must remain invisible and offscreen');
    measures.push({aspect,width:rect.width,height:rect.height});
   }
   // Model the SDL browser resize callback using the real CSS layout.
   let resizes=0;
   window.addEventListener('resize',()=>{const rect=canvas.getBoundingClientRect();canvas.width=Math.round(rect.width*devicePixelRatio);canvas.height=Math.round(rect.height*devicePixelRatio);resizes++;});
   const {exposeGameSurface}=await import('/display-controls.mjs');exposeGameSurface(view);
   if(resizes!==1||canvas.width<=0||canvas.height<=0||document.activeElement!==canvas)throw Error('Reveal failed to restore backing size before focus');
   return {baselineHiddenZero:true,hiddenAspects:measures,revealBacking:{width:canvas.width,height:canvas.height},resizes,devicePixelRatio};
  });
  console.log(JSON.stringify({status:'VERIFIED browser layout and resize ordering only',...result}));
 }finally{await browser?.close();await new Promise(resolve=>server.close(resolve));}
})().catch(error=>{console.error(error);process.exitCode=1;});
