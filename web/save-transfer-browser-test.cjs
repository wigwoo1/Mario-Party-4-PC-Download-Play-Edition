const http=require('node:http'),fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const server=http.createServer((req,res)=>{
  if(req.url==='/'){res.end('<!doctype html><title>Save channel test</title>');return;}
  if(!['/save-channel.mjs','/save-transfer.mjs','/transfer.mjs','/runtime-save.mjs'].includes(req.url)){res.writeHead(404);res.end();return;}
  res.setHeader('Content-Type','text/javascript');res.end(fs.readFileSync(path.join(__dirname,req.url.slice(1))));
});
(async()=>{
  await new Promise(r=>server.listen(0,'127.0.0.1',r));let browser;
  try{
    browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});
    const h=await (await browser.newContext()).newPage(),g=await (await browser.newContext()).newPage(),errors=[];
    for(const page of [h,g]){
      page.on('pageerror',e=>errors.push(e.message));await page.goto(`http://127.0.0.1:${server.address().port}`);
      await page.evaluate(()=>{
        globalThis.pc=new RTCPeerConnection({iceServers:[]});
        globalThis.gather=async()=>{
          if(pc.iceGatheringState!=='complete')await new Promise((resolve,reject)=>{
            const timer=setTimeout(()=>reject(Error('ICE timeout')),10000);
            pc.addEventListener('icegatheringstatechange',()=>{if(pc.iceGatheringState==='complete'){clearTimeout(timer);resolve();}});
          });
          return pc.localDescription.toJSON();
        };
      });
    }
    const offer=await h.evaluate(async()=>{
      const {prepareSaveTransfer}=await import('/save-transfer.mjs');
      globalThis.prepared=await prepareSaveTransfer(Uint8Array.from({length:16744448},(_,i)=>i%251),
        {assetId:'a'.repeat(64),sessionId:'browser-save-session-01'});
      return prepared.offer;
    });
    await g.evaluate(async offer=>{
      const {receiveSaveChannel}=await import('/save-channel.mjs');
      const {prepareGuestSaveSession}=await import('/runtime-save.mjs');
      pc.ondatachannel=event=>receiveSaveChannel(event.channel,
        {assetId:'a'.repeat(64),sessionId:'browser-save-session-01'}, {
          error:error=>{globalThis.result={error:String(error)};},
          install:async(snapshot,signal)=>{
            const session=await prepareGuestSaveSession({...snapshot,expectedAssetId:'a'.repeat(64),path:'/MemoryCardA.USA.raw'});
            try {
              signal.throwIfAborted();
              let installed;
              session.preRun({FS:{writeFile:(_path,bytes)=>{installed=bytes.slice();}}});
              globalThis.result={size:installed.length,exact:installed.every((byte,i)=>byte===i%251),persistent:session.status.persistent};
            } finally {await session.close();}
          }
        });
    },offer);
    const sdp=await h.evaluate(async()=>{
      const {sendSaveChannel}=await import('/save-channel.mjs');
      sendSaveChannel(pc.createDataChannel('partyboard-save-v1',{ordered:true}),prepared,{
        error:error=>{globalThis.failure=String(error);},complete:()=>{globalThis.acknowledged=true;}});
      await pc.setLocalDescription(await pc.createOffer());return gather();
    });
    const answer=await g.evaluate(async sdp=>{await pc.setRemoteDescription(sdp);await pc.setLocalDescription(await pc.createAnswer());return gather();},sdp);
    await h.evaluate(answer=>pc.setRemoteDescription(answer),answer);
    await g.waitForFunction(()=>globalThis.result,{},{timeout:30000});
    assert.deepEqual(await g.evaluate(()=>result),{size:16744448,exact:true,persistent:false});
    await h.waitForFunction(()=>globalThis.acknowledged||globalThis.failure,{},{timeout:5000});
    assert.equal(await h.evaluate(()=>globalThis.failure),undefined);assert.deepEqual(errors,[]);
    await g.evaluate(async offer=>{
      const {receiveSaveTransfer}=await import('/save-transfer.mjs');
      pc.ondatachannel=event=>{
        const cancel=event.channel.label==='save-cancel';
        const receiver=receiveSaveTransfer(event.channel,offer,
          {assetId:'a'.repeat(64),sessionId:'browser-save-session-01'}, {
            progress:bytes=>{if(cancel&&bytes>0){receiver.cancel();globalThis.cancelled=true;}},
            complete:snapshot=>{if(cancel)globalThis.badCompletion=true;else globalThis.retryExact=snapshot.bytes.every((byte,i)=>byte===i%251);},
            error:error=>{globalThis.retryError=String(error);}
          });
      };
    },offer);
    await h.evaluate(async()=>{
      const {host}=await import('/transfer.mjs');
      const channel=pc.createDataChannel('save-cancel',{ordered:true});
      channel.onclose=()=>{globalThis.cancelClosed=true;};
      host(channel,prepared.asset,()=>{},()=>{globalThis.badAcknowledgment=true;});
    });
    await g.waitForFunction(()=>globalThis.cancelled,{},{timeout:10000});
    await h.waitForFunction(()=>globalThis.cancelClosed,{},{timeout:10000});
    assert.equal(await g.evaluate(()=>globalThis.badCompletion),undefined);
    assert.equal(await h.evaluate(()=>globalThis.badAcknowledgment),undefined);
    await h.evaluate(async()=>{
      const {host}=await import('/transfer.mjs');
      host(pc.createDataChannel('save-retry',{ordered:true}),prepared.asset,
        error=>{globalThis.retryFailure=String(error);},()=>{globalThis.retryAcknowledged=true;});
    });
    await g.waitForFunction(()=>globalThis.retryExact||globalThis.retryError,{},{timeout:30000});
    assert.equal(await g.evaluate(()=>globalThis.retryError),undefined);
    await h.waitForFunction(()=>globalThis.retryAcknowledged||globalThis.retryFailure,{},{timeout:5000});
    assert.equal(await h.evaluate(()=>globalThis.retryFailure),undefined);assert.deepEqual(errors,[]);
    console.log('PASS: cancelled partial save was neither completed nor acknowledged; fresh channel retry verified exact bytes.');
    await Promise.all([h.evaluate(()=>pc.close()),g.evaluate(()=>pc.close())]);
    console.log('PASS: 16,744,448-byte synthetic save transferred over real WebRTC between isolated Edge contexts, verified, acknowledged and installed in ephemeral guest session.');
  }finally{if(browser)await browser.close();server.closeAllConnections();await new Promise(r=>server.close(r));}
})().catch(error=>{console.error(error);process.exitCode=1;});
