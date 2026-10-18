// Local transport contract, synthetic identities/state; not game-state recovery.
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const http=require('node:http'),fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const files=new Set(['input-channels.mjs','session-input.mjs','lockstep.mjs','reconnect-policy.mjs','input-packet.mjs']);
const server=http.createServer((req,res)=>{
 if(req.url==='/'){res.end('<!doctype html><title>Session input recovery test</title>');return;}
 const name=req.url.slice(1);if(!files.has(name)){res.writeHead(404);res.end();return;}
 res.setHeader('Content-Type','text/javascript');res.end(fs.readFileSync(path.join(__dirname,name)));
});
(async()=>{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));let browser;
 try {
  browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});const pages=[],errors=[];
  for(const local of [11,22]) {
   const page=await(await browser.newContext()).newPage();pages.push(page);page.on('pageerror',e=>errors.push(e.message));
   await page.goto(`http://127.0.0.1:${server.address().port}`);
   await page.evaluate(async local=>{
    const {InputChannels}=await import('/input-channels.mjs');const {SessionInput}=await import('/session-input.mjs');const {encodeInput}=await import('/input-packet.mjs');
    const session=Uint8Array.of(1,...Array(15).fill(0));const peer=local===11?22:11;
    globalThis.q=new SessionInput(session,[11,22,0,0]);globalThis.pc=new RTCPeerConnection({iceServers:[]});globalThis.received=0;
    const transport=new InputChannels(q,{received:()=>received++,error:(_peer,e)=>{throw e;}});
    globalThis.attach=channel=>{globalThis.channel=channel;transport.attach(peer,channel);};
    pc.ondatachannel=e=>attach(e.channel);
    globalThis.sendFrame=frame=>{const packet=encodeInput({session,frame:BigInt(frame),slot:local===11?0:1,checkpoint:42,pad:new Uint8Array(11)});if(q.receive(local,packet,performance.now())!=='Accepted')throw Error('Local input rejected');channel.send(packet);};
    globalThis.gather=async()=>{if(pc.iceGatheringState!=='complete')await new Promise(resolve=>pc.addEventListener('icegatheringstatechange',()=>{if(pc.iceGatheringState==='complete')resolve();}));return pc.localDescription.toJSON();};
   },local);
  }
  const [h,g]=pages;
  const offer=await h.evaluate(async()=>{attach(pc.createDataChannel('inputs',{ordered:true}));await pc.setLocalDescription(await pc.createOffer());return gather();});
  const answer=await g.evaluate(async offer=>{await pc.setRemoteDescription(offer);await pc.setLocalDescription(await pc.createAnswer());return gather();},offer);
  await h.evaluate(answer=>pc.setRemoteDescription(answer),answer);
  for(const p of pages)await p.waitForFunction(()=>globalThis.channel?.readyState==='open');
  for(const p of pages)await p.evaluate(()=>sendFrame(0));
  for(const p of pages)await p.waitForFunction(()=>received===1);
  const replacePeer=process.env.PARTYBOARD_TEST_REPLACE_PEER==='1';
  if(replacePeer)await h.evaluate(()=>pc.close());else await h.evaluate(()=>channel.close());
  for(const p of pages){await p.waitForFunction(()=>q.status(performance.now()).state==='Paused');assert.equal(await p.evaluate(()=>q.advance(42,performance.now())),null);}
  if(replacePeer) {
   for(const p of pages)await p.evaluate(()=>{pc.close();globalThis.pc=new RTCPeerConnection({iceServers:[]});pc.ondatachannel=e=>attach(e.channel);});
   const recoveredOffer=await h.evaluate(async()=>{attach(pc.createDataChannel('recovered-inputs',{ordered:true}));await pc.setLocalDescription(await pc.createOffer());return gather();});
   const recoveredAnswer=await g.evaluate(async offer=>{await pc.setRemoteDescription(offer);await pc.setLocalDescription(await pc.createAnswer());return gather();},recoveredOffer);
   await h.evaluate(answer=>pc.setRemoteDescription(answer),recoveredAnswer);
  }else await h.evaluate(()=>attach(pc.createDataChannel('recovered-inputs',{ordered:true})));

  for(const p of pages)await p.waitForFunction(()=>channel.label==='recovered-inputs'&&channel.readyState==='open');
  for(let i=0;i<pages.length;i++)await pages[i].evaluate(peer=>{const token=q.status(performance.now());if(!q.recovered(peer,token,performance.now()))throw Error('Recovery rejected');if(q.advance(42,performance.now())!==null)throw Error('Premature resume');if(!q.resume(token,performance.now()))throw Error('Resume rejected');},i===0?22:11);
  const frames=await Promise.all(pages.map(p=>p.evaluate(()=>{const f=q.advance(42,performance.now());return {...f,index:String(f.index),pads:f.pads.map(p=>Array.from(p))};})));
  assert.deepEqual(frames[0],frames[1]);assert.equal(frames[0].index,'0');
  for(const p of pages)await p.evaluate(()=>sendFrame(1));
  for(const p of pages){await p.waitForFunction(()=>received===2);assert.equal(await p.evaluate(()=>String(q.advance(42,performance.now()).index)),'1');}
  assert.deepEqual(errors,[]);console.log('Recovery transport: '+(replacePeer?'new peer connection with fresh SDP/ICE':'replacement data channel'));console.log('PASS: real channel loss freezes buffered frame; replacement channel and explicit recovery/resume preserve matching input, then advance next frame.');
 }finally{await browser?.close();await new Promise(r=>server.close(r));}
})().catch(e=>{console.error(e);process.exitCode=1;});
