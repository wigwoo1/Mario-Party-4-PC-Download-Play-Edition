// Exercise the retained diagnostic transport controls explicitly.
const manualURL=value=>{const url=new URL(value);url.searchParams.set('manual','1');return url.href;};
// Run with PLAYWRIGHT_MODULE pointing to Playwright if it is not on NODE_PATH.
const { chromium } = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const assert = require('node:assert/strict');
(async()=>{
  const browser=await chromium.launch({channel:process.env.PLAYWRIGHT_CHANNEL||'msedge',headless:true,args:['--mute-audio',...(process.env.PARTYBOARD_TEST_DIRECT_ICE==='1'?['--disable-features=WebRtcHideLocalIpsWithMdns']:[])]});
  try {
    const hostContext=await browser.newContext(),guestContext=await browser.newContext();
    const h=await hostContext.newPage(),g=await guestContext.newPage();
    const errors=[];
    h.on('pageerror',e=>errors.push(e.message));g.on('pageerror',e=>errors.push(e.message));
    const base=process.env.PARTYBOARD_TEST_URL || 'http://127.0.0.1:8784';
    await Promise.all([h.goto(manualURL(base)),g.goto(manualURL(base))]);
    const lobbyResult=await g.evaluate(async()=>{
      const {LobbyState}=await import('/lobby-state.mjs');
      const {renderLobby}=await import('/lobby-view.mjs');
      const model=new LobbyState('a'.repeat(64),100);
      model.join('host','Host'); model.join('guest','<img src=x onerror=alert(1)>');
      model.report('guest',{assetId:model.assetId,sequence:1,bytes:25,verified:false,shaders:false,ready:false});
      const container=document.createElement('section');
      renderLobby(container,model.snapshot(),'guest');
      const result={rows:container.children.length,bars:[...container.querySelectorAll('progress')].map(p=>p.value),
        local:container.children[1].querySelector('h3').textContent,images:container.querySelectorAll('img').length};
      model.leave('guest'); renderLobby(container,model.snapshot(),'guest');
      result.remainingBars=container.querySelectorAll('progress').length;
      return result;
    });
    assert.equal(lobbyResult.rows,4); assert.deepEqual(lobbyResult.bars,[0,.25]);
    assert.match(lobbyResult.local,/\(You\)$/); assert.equal(lobbyResult.images,0);
    assert.equal(lobbyResult.remainingBars,1);
    console.log('PASS: asset-free lobby renderer shows four slots, local progress and safe player names.');
    for(const page of [h,g]) {
      assert.equal(await page.evaluate(async()=>{
        const {encodeCapabilities,decodeCapabilities}=await import('/controller-capabilities.mjs');
        const value={session:Uint8Array.of(9,...Array(15).fill(0)),revision:0xffffffffffffffffn,mask:0xa0000000};
        const offer=encodeCapabilities(value),ack=encodeCapabilities(value,true);
        return decodeCapabilities(offer).revision===value.revision &&
          decodeCapabilities(ack,true).mask===value.mask && decodeCapabilities(offer,true)===null;
      }),true);
    }
    console.log('PASS: capability codec executed in both isolated Edge contexts.');
    for(const page of [h,g]) await page.evaluate(()=>{
      globalThis.capabilityTestPeer=new RTCPeerConnection({iceServers:[]});
      globalThis.capabilityGather=async()=>{
        const pc=globalThis.capabilityTestPeer;
        if(pc.iceGatheringState!=='complete') await new Promise((resolve,reject)=>{
          const timer=setTimeout(()=>reject(Error('Capability ICE deadline')),15000);
          pc.addEventListener('icegatheringstatechange',()=>{
            if(pc.iceGatheringState==='complete'){clearTimeout(timer);resolve();}
          });
        });
        return pc.localDescription.toJSON();
      };
    });
    await g.evaluate(async()=>{
      const codec=await import('/controller-capabilities.mjs');
      const {probe}=await import('/lockstep-probe.mjs');
      capabilityTestPeer.ondatachannel=event=>{
        if(event.channel.label==='partyboard-lockstep-disconnect-test'){
          globalThis.disconnectResult=probe(event.channel,22,11,10).then(
            value=>({ok:true,value}),error=>({ok:false,error:error.message}));return;
        }
        if(event.channel.label==='partyboard-lockstep-test'){
          globalThis.lockstepResult=probe(event.channel,22,11);return;
        }
        const channel=event.channel; channel.binaryType='arraybuffer';
        channel.onmessage=message=>{
          const offer=codec.decodeCapabilities(new Uint8Array(message.data));
          if(!offer || offer.revision!==7n || offer.mask!==0xa0000000 || offer.session[0]!==9)
            throw Error('Invalid capability test offer');
          channel.send(codec.encodeCapabilities(offer,true));
        };
      };
    });
    const capOffer=await h.evaluate(async()=>{
      const codec=await import('/controller-capabilities.mjs');
      const channel=capabilityTestPeer.createDataChannel('partyboard-capability-test',{ordered:true});
      channel.binaryType='arraybuffer';
      globalThis.capabilityAcknowledged=false;
      channel.onopen=()=>channel.send(codec.encodeCapabilities({
        session:Uint8Array.of(9,...Array(15).fill(0)),revision:7n,mask:0xa0000000}));
      channel.onmessage=message=>{
        const ack=codec.decodeCapabilities(new Uint8Array(message.data),true);
        globalThis.capabilityAcknowledged=!!ack && ack.revision===7n && ack.mask===0xa0000000 && ack.session[0]===9;
      };
      await capabilityTestPeer.setLocalDescription(await capabilityTestPeer.createOffer());
      return capabilityGather();
    });
    const capAnswer=await g.evaluate(async offer=>{
      await capabilityTestPeer.setRemoteDescription(offer);
      await capabilityTestPeer.setLocalDescription(await capabilityTestPeer.createAnswer());
      return capabilityGather();
    },capOffer);
    await h.evaluate(answer=>capabilityTestPeer.setRemoteDescription(answer),capAnswer);
    await h.waitForFunction(()=>globalThis.capabilityAcknowledged,{},{timeout:15000});
    await h.evaluate(async()=>{
      const {probe}=await import('/lockstep-probe.mjs');
      globalThis.lockstepResult=probe(capabilityTestPeer.createDataChannel('partyboard-lockstep-test',{ordered:true}),11,22);
    });
    await g.waitForFunction(()=>!!globalThis.lockstepResult);
    const lockstepResults=await Promise.all([h.evaluate(()=>lockstepResult),g.evaluate(()=>lockstepResult)]);
    assert.deepEqual(lockstepResults[0],lockstepResults[1]);
    assert.equal(lockstepResults[0].frames,120);
    console.log('PASS: browser queues matched 120 four-controller frames over WebRTC, including final checkpoint.');
    await h.evaluate(async()=>{
      const {probe}=await import('/lockstep-probe.mjs');
      globalThis.disconnectResult=probe(capabilityTestPeer.createDataChannel('partyboard-lockstep-disconnect-test',{ordered:true}),11,22).then(
        value=>({ok:true,value}),error=>({ok:false,error:error.message}));
    });
    await g.waitForFunction(()=>!!globalThis.disconnectResult);
    const disconnected=await Promise.all([h.evaluate(()=>disconnectResult),g.evaluate(()=>disconnectResult)]);
    assert.equal(disconnected[0].ok,false); assert.match(disconnected[0].error,/disconnected|Lockstep probe channel error/);
    assert.equal(disconnected[1].ok,false); assert.match(disconnected[1].error,/Injected/);
    console.log('PASS: mid-input WebRTC disconnect stopped both browser probes without success.');
    for(const page of [h,g]) await page.evaluate(()=>capabilityTestPeer.close());
    console.log('PASS: capability offer and acknowledgement crossed a separate WebRTC channel.');
    const localFile=process.env.PARTYBOARD_TEST_TRANSFER_FILE;
    let guestMemory,guestHeapPeak=0,guestHeapSamples=0,guestHeapTimer;
    if(localFile){
      guestMemory=await guestContext.newCDPSession(g);
      guestHeapTimer=setInterval(()=>{void guestMemory.send('Runtime.getHeapUsage').then(value=>{
        guestHeapPeak=Math.max(guestHeapPeak,value.usedSize);guestHeapSamples++;
      }).catch(()=>{});},500);
      guestHeapTimer.unref();
    }
    const payload=localFile?undefined:Buffer.from(Array.from({length:600123},(_,i)=>i%251));
    await h.locator('#file').setInputFiles(localFile||{name:'synthetic-test.bin',mimeType:'application/octet-stream',buffer:payload});
    const expected=localFile?await h.evaluate(async()=>{const {prepare}=await import('/transfer.mjs');const {manifest}=await prepare(document.querySelector('#file').files[0]);return {id:manifest.id,size:manifest.size};}):undefined;
    await h.locator('#host').click();
    try{await h.waitForFunction(()=>document.querySelector('#local').value.length>0,{},{timeout:120000});}
    catch(cause){
      console.error('Host invitation diagnostic: '+JSON.stringify({status:await h.locator('#status').textContent(),pageErrors:errors}));
      throw cause;
    }
    await g.locator('#remote').fill(await h.locator('#local').inputValue());
    await g.locator('#join').click();
    await g.waitForFunction(()=>document.querySelector('#local').value.length>0);
    await h.locator('#remote').fill(await g.locator('#local').inputValue());
    await h.locator('#answer').click();
    await g.locator('#accept').waitFor({state:'visible'});
    await g.waitForFunction(()=>!document.querySelector('#accept').disabled);
    await h.evaluate(()=>{globalThis.guestProgressSamples=[];new MutationObserver(()=>guestProgressSamples.push(document.querySelector('#guest-progress').value)).observe(document.querySelector('#guest-download'),{childList:true,subtree:true});});
    await g.locator('#accept').click();
    if(process.env.PARTYBOARD_TEST_DOWNLOAD_CONTROLS==='1'){
      if(!localFile)throw Error('Download control acceptance requires the explicit local game file');
      await g.waitForFunction(()=>{const p=document.querySelector('#progress');return p.value>0&&p.value<1;},{},{timeout:120000});
      const before=await g.locator('#progress').evaluate(p=>p.value);
      await g.getByRole('button',{name:'Customize Controls'}).click();
      await g.getByLabel('Keyboard: A button',{exact:true}).selectOption('Space');
      await g.getByLabel('Controller: L trigger',{exact:true}).selectOption('button:4');
      await g.getByRole('button',{name:'Apply controls'}).click();
      await g.getByRole('button',{name:'Customize Controls'}).click();
      assert.equal(await g.getByLabel('Keyboard: A button',{exact:true}).inputValue(),'Space');
      assert.equal(await g.getByLabel('Controller: L trigger',{exact:true}).inputValue(),'button:4');
      await g.waitForFunction(previous=>{const p=document.querySelector('#progress');return p.value>previous&&p.value<1;},before,{timeout:30000});
      await g.getByRole('button',{name:'Cancel',exact:true}).click();
      console.log('PASS: keyboard/controller preferences retained while real game asset download advanced');

      const hostBefore=await g.locator('#progress').evaluate(p=>p.value);
      await h.getByRole('button',{name:'Customize Controls'}).click();
      await h.getByLabel('Keyboard: A button',{exact:true}).selectOption('KeyP');
      await h.getByLabel('Controller: R trigger',{exact:true}).selectOption('button:4');
      await h.getByRole('button',{name:'Apply controls'}).click();
      await h.getByRole('button',{name:'Customize Controls'}).click();
      assert.equal(await h.getByLabel('Keyboard: A button',{exact:true}).inputValue(),'KeyP');
      assert.equal(await h.getByLabel('Controller: R trigger',{exact:true}).inputValue(),'button:4');
      await h.getByRole('button',{name:'Cancel',exact:true}).click();
      await g.waitForFunction(previous=>{const p=document.querySelector('#progress');return p.value>previous&&p.value<1;},hostBefore,{timeout:30000});
      console.log('PASS: host keyboard/controller preferences retained while guest asset transfer advanced');
    }
    await g.waitForFunction(()=>document.querySelector('#status').textContent.startsWith('Assets checked and prepared.'),{},{timeout:localFile?600000:30000});
    if(process.env.PARTYBOARD_TEST_DOWNLOAD_CONTROLS==='1'){
      await g.getByRole('button',{name:'Customize Controls'}).click();
      assert.equal(await g.getByLabel('Keyboard: A button',{exact:true}).inputValue(),'Space');
      assert.equal(await g.getByLabel('Controller: L trigger',{exact:true}).inputValue(),'button:4');
      await g.getByRole('button',{name:'Cancel',exact:true}).click();
      await h.getByRole('button',{name:'Customize Controls'}).click();
      assert.equal(await h.getByLabel('Keyboard: A button',{exact:true}).inputValue(),'KeyP');
      assert.equal(await h.getByLabel('Controller: R trigger',{exact:true}).inputValue(),'button:4');
      await h.getByRole('button',{name:'Cancel',exact:true}).click();
      console.log('PASS: host and guest control choices survived verified download completion');
    }
    if(localFile){
      const cached=await g.evaluate(async expected=>{const {browserStore}=await import('/transfer.mjs');const store=await browserStore();try{const manifest=await store.manifest(expected.id);return {id:manifest?.id,size:manifest?.size};}finally{store.close();}},expected);
      assert.deepEqual(cached,expected);console.log('PASS: local file transfer and independent cached-asset integrity verification: '+JSON.stringify(cached));
      if(process.env.PARTYBOARD_TEST_RUNTIME_DIRECTORY){
        expected.prepareGame=process.env.PARTYBOARD_TEST_PREPARE_GAME==='1';
        expected.runGame=process.env.PARTYBOARD_TEST_RUN_GAME==='1';
        const mounted=await g.evaluate(async expected=>{
          const {browserStore}=await import('/transfer.mjs');
          const {prepareMountedRuntime}=await import('/runtime-mount.mjs');
          const {prepareFingerprintedRuntimeFactory}=await import('/runtime-build.mjs');
          const artifacts=await Promise.all(['contract.mjs','contract.wasm'].map(async name=>{
            const response=await fetch('/'+name);if(!response.ok)throw Error('Runtime artifact fetch failed');
            return {name,blob:await response.blob()};
          }));
          const {buildId,createRuntime:create}=await prepareFingerprintedRuntimeFactory(artifacts);
          const store=await browserStore();let handle,stream,engine;const output=[];
          const canvas=document.createElement('canvas');canvas.id='canvas';canvas.width=640;canvas.height=480;document.body.append(canvas);
          try {
            handle=await prepareMountedRuntime(store,expected.id,options=>create({...options,canvas,
              print:line=>output.push(line),printErr:line=>output.push(line)}),{
              shutdown:async module=>{if(engine)await engine.dispose();if(stream){module.FS.close(stream);stream=undefined;}},
            });
            const module=handle.module;stream=module.FS.open('/assets/verified-disc.ciso','r');
            const size=module.FS.stat('/assets/verified-disc.ciso').size;
            const readResult=await module.ccall('PartyBoardTestReadMountedDiscHeader','number',[],[],{async:true});
            if(readResult!==0||handle.stats.reads<1||handle.stats.bytes<4)throw Error('Mounted CISO async read probe failed');
            if(await module.ccall('PartyBoardTestInspectMountedGame','number',[],[],{async:true})!==0)
              throw Error('Downloaded game disc adapter inspection failed');
            if(!output.some(line=>line.includes('Browser local game disc passed: GMPE01 revision 1'))||
                !output.some(line=>line.includes('Browser bootstrap assets passed:')))
              throw Error('Downloaded disc identity/file/bootstrap checks did not complete');
            const reads=handle.stats.reads,readBytes=handle.stats.bytes;
            let preparedCheckpoint,renderedImage,runFrames,pipelineDrain;
            if(expected.prepareGame){
              globalThis.testExternalEngine=true;globalThis.testSampleLimit=expected.runGame?1004:8;
              if(await module.ccall('PartyBoardTestPrepareMountedPlatform','number',[],[],{async:true})!==0)throw Error('Guest platform preparation failed');
              const {prepareEngineLifecycle}=await import('/engine-lifecycle.mjs');
              const prepared=await prepareEngineLifecycle(module,{sessionSeed:1,shutdown:async m=>{m._PartyBoard_GameRequestStop();m._PartyBoardTestFinishExternalEngine();}});
              engine=prepared.engine;preparedCheckpoint=prepared.checkpoint;
              if(engine.phase!=='Idle'||module._PartyBoardTestExternalFrames()!==0)throw Error('Guest preparation advanced simulation');
              const {drainIdleEnginePipelines}=await import('/pipeline-drain.mjs');
              const drain=await drainIdleEnginePipelines(engine,{
                read:()=>({queued:module._PartyBoardTestPipelineCount(0),created:module._PartyBoardTestPipelineCount(1)}),
                advance:async()=>{if(await module.ccall('PartyBoardTestAdvancePreparedRenderer','number',[],[],{async:true})!==0)throw Error('Renderer-only drain failed');},maxSteps:60});
              if(engine.phase!=='Idle'||module._PartyBoardTestExternalFrames()!==0||engine.preparedCheckpoint()!==preparedCheckpoint)
                throw Error('Pipeline drain changed prepared simulation');
              pipelineDrain=drain;
              if(expected.runGame){
                const {StartBarrier}=await import('/start-barrier.mjs');
                const {prepareStartAcknowledgment,resumeCommittedClientRuntime}=await import('/start-client.mjs');
                const snapshot={revision:1,membership:1,total:expected.size,assetId:expected.id,players:
                  ['host','guest'].map((peer,slot)=>({peer,slot,bytes:expected.size,verified:true,shaders:true,ready:true}))};
                // Measured build, fixed diagnostic session; no authenticated production lobby or
                // completed shader-prewarm claim is implied by this fixture.
                const barrier=new StartBarrier(snapshot,{session:new Uint8Array(16).fill(1),seed:1,buildId,checkpoint:preparedCheckpoint});
                const options={peer:'guest',buildId,snapshot:()=>snapshot};
                const accepted=await prepareStartAcknowledgment(barrier.proposal,{...options,
                  prepare:async()=>({...prepared,assetId:expected.id,buildId})});
                let rejected=false;
                try {resumeCommittedClientRuntime({...barrier.proposal,type:'session-start-commit',seed:2},accepted,options);}
                catch {rejected=true;}
                if(!rejected||engine.phase!=='Idle'||module._PartyBoardTestExternalFrames()!==0)throw Error('Guest start gate accepted altered commit');
                await resumeCommittedClientRuntime({...barrier.proposal,type:'session-start-commit'},accepted,options);runFrames=module._PartyBoardTestExternalFrames();
                if(engine.phase!=='Idle'||runFrames!==1001||!output.some(line=>line.includes('Input trace: recording complete (1004 samples)')))throw Error('Guest bounded execution did not complete: '+JSON.stringify({phase:engine.phase,runFrames,output:output.slice(-20)}));
                renderedImage=canvas.toDataURL('image/png');
              }
            }
            await handle.dispose();return {size,released:handle.module===undefined&&!module.FS.analyzePath('/assets').exists,
              reads,readBytes,preparedCheckpoint,runFrames,renderedImage,pipelineDrain,buildId,
              autoStarted:output.some(line=>/Game system initialization passed|Disc renderer passed: red/.test(line))};
          } finally {await handle?.dispose();store.close();}
        },expected);
        assert.equal(mounted.size,expected.size);assert.equal(mounted.released,true);assert.equal(mounted.autoStarted,false);
        assert.match(mounted.buildId,/^[a-f0-9]{64}$/);console.log('Guest fingerprinted executable build: '+mounted.buildId);
        assert.ok(mounted.reads>0&&mounted.readBytes>=4);
        if(expected.prepareGame){assert.ok(Number.isInteger(mounted.preparedCheckpoint));assert.equal(mounted.pipelineDrain.queued,0);console.log('PASS: downloaded guest game engine prepared idle at frame zero, checkpoint '+mounted.preparedCheckpoint+', then disposed.');console.log('Guest discovered pipeline drain (not full shader coverage): '+JSON.stringify(mounted.pipelineDrain));}
        if(expected.runGame){
          assert.equal(mounted.runFrames,1001);require('node:fs').writeFileSync('outputs/validation/browser-downloaded-game-startup.png',Buffer.from(mounted.renderedImage.split(',')[1],'base64'));
          console.log('PASS: downloaded guest game executed '+mounted.runFrames+' bounded startup frames and captured canvas; no multiplayer gameplay claim.');
        }
        console.log('Guest actual async disc inspection reads: '+JSON.stringify({reads:mounted.reads,bytes:mounted.readBytes}));
        console.log('PASS: downloaded game inspected through Aurora DVD adapter: USA Rev1 identity, bounded file read, embedded fonts/DOL comparison and cleanup; no production multiplayer claim.');
      }
    } else {
    const bytes=await g.evaluate(async()=>{
      const db=await new Promise((resolve,reject)=>{const r=indexedDB.open('partyboard-assets-v1');r.onsuccess=()=>resolve(r.result);r.onerror=()=>reject(r.error);});
      const items=await new Promise((resolve,reject)=>{const r=db.transaction('chunks').objectStore('chunks').getAll();r.onsuccess=()=>resolve(r.result);r.onerror=()=>reject(r.error);});
      const result=[];for(const item of items)result.push(...new Uint8Array(item.bytes));db.close();return result;
    });
    assert.deepEqual(Buffer.from(bytes),payload);
    }
    assert.equal(await g.locator('#progress').evaluate(el=>el.value),1);
    await h.waitForFunction(()=>document.querySelector('#guest-progress').value===1);
    const samples=await h.evaluate(()=>guestProgressSamples);
    assert.ok(samples.some(value=>value>0 && value<1), 'Host must see intermediate verified download progress');
    assert.match(await h.locator('#guest-download').textContent(),/100%/);
    for(const page of [h,g]) {
      await page.waitForFunction(()=>document.querySelectorAll('#lobby-players progress').length===2 && [...document.querySelectorAll('#lobby-players progress')].every(p=>p.value===1));
      assert.equal(await page.locator('#lobby-players article').count(),4);
      await page.waitForFunction(()=>[...document.querySelectorAll('#lobby-players article p')].every(p=>p.textContent.includes('Awaiting shader preparation')));
    }
    assert.match(await h.locator('#lobby-players h3').first().textContent(),/Host \(You\)/);
    assert.match(await g.locator('#lobby-players h3').nth(1).textContent(),/Guest \(You\)/);
    await h.locator('#stop').click();
    if(guestHeapTimer){clearInterval(guestHeapTimer);console.log('Guest sampled JS heap (excludes Blob backing/native/Wasm memory): '+JSON.stringify({peakBytes:guestHeapPeak,samples:guestHeapSamples}));}
    assert.equal(await h.locator('#guest-progress').evaluate(el=>el.value),0);
    console.log('PASS: host displayed intermediate and completed guest download progress, then reset on stop.');
    assert.deepEqual(errors,[]);
    console.log(localFile?'PASS: two isolated browser contexts transferred the selected local file over WebRTC; guest cache identity and size match, independent cache verification completed. No production launcher or playable-netplay claim.':'PASS: two isolated browser contexts exchanged a 600123-byte synthetic file over real WebRTC; cached bytes match exactly.');
  } finally {await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
