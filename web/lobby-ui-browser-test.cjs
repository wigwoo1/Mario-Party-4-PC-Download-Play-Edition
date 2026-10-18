// Exercise the retained diagnostic transport controls explicitly.
const manualURL=value=>{const url=new URL(value);url.searchParams.set('manual','1');return url.href;};
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const assert=require('node:assert/strict');
(async()=>{
  const browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});
  try {
    const pages=[],errors=[];
  for(let i=0;i<4;i++) {
      const context=await browser.newContext(),page=await context.newPage();pages.push(page);
      page.on('pageerror',error=>errors.push(error.message));
      await page.goto(manualURL(process.env.PARTYBOARD_TEST_URL||'http://127.0.0.1:8784'));
    try{await page.waitForFunction(()=>document.querySelector('[data-display-fullscreen]')&&document.querySelector('[data-display-aspect]'));}
    catch(error){throw Error('Shared display controls did not initialize: '+JSON.stringify({pageErrors:errors,status:await page.locator('#status').textContent(),url:page.url()})+' '+error.message);}
    assert.equal(await page.getByRole('button',{name:'Customize Controls'}).count(),1,'Every host and guest must have the page control editor before preparing a game.');
    }
  await pages[0].locator('#guest-resolution').selectOption('960x720');await pages[0].locator('[data-display-aspect]').selectOption('16:9');
  await pages[1].locator('#guest-resolution').selectOption('1280x960');await pages[1].locator('[data-display-aspect]').selectOption('16:10');
  assert.equal(await pages[0].evaluate(()=>localStorage.getItem('partyboard-game-aspect')),'16:9');
  assert.equal(await pages[1].evaluate(()=>localStorage.getItem('partyboard-game-aspect')),'16:10');
  assert.equal(await pages[2].evaluate(()=>localStorage.getItem('partyboard-game-aspect')),null,'Resolution and aspect preferences stay local to each player device.');
  await pages[1].getByRole('button',{name:'Customize Controls'}).click();
  await pages[1].getByLabel('Controller: Z button',{exact:true}).selectOption('button:16');
  await pages[1].getByRole('button',{name:'Apply controls'}).click();
  const guestBindings=JSON.parse(await pages[1].evaluate(()=>localStorage.getItem('partyboard-control-bindings')));
  assert.equal(guestBindings.controller.z,'button:16');
  assert.equal(await pages[0].evaluate(()=>localStorage.getItem('partyboard-control-bindings')),null,'Host and guest control rebindings remain device-local.');
  if(process.env.PARTYBOARD_TEST_DISPLAY_CONTROLS_ONLY==='1'){
    assert.deepEqual(errors,[]);console.log('PASS actual four-page host/guest display controls, aspect/resolution isolation, and persisted guest rebind');return;
  }
    const host=pages[0],payload=Buffer.from(Uint8Array.from({length:600123},(_,i)=>i%251));
    for(const page of pages){
      assert.equal(await page.locator('#recovery-panel').isHidden(),true);
      assert.equal(await page.evaluate(async()=>{const app=await import('/app.mjs');try{await app.beginLobbyRecoveryAgreement();return false;}catch{return true;}}),true);
    }
    await host.locator('#file').setInputFiles({name:'four-player-fixture.bin',mimeType:'application/octet-stream',buffer:payload});
    const invite=async page=>{
      await host.locator('#host').click();
      await host.waitForFunction(()=>!document.querySelector('#answer').disabled&&document.querySelector('#local').value.length>0);
      await page.locator('#remote').fill(await host.locator('#local').inputValue());
      await page.locator('#join').click();
      await page.waitForFunction(()=>document.querySelector('#local').value.length>0);
      await host.locator('#remote').fill(await page.locator('#local').inputValue());
      await host.locator('#answer').click();
      await page.waitForFunction(()=>!document.querySelector('#accept').disabled);
    };
    for(const page of pages.slice(1,3))await invite(page);
    await host.locator('summary').click();
    await host.locator('#stun').fill('invalid server');await host.locator('#host').click();
    await host.waitForFunction(()=>document.querySelector('#status').textContent.startsWith('Invitation failed.'));
    assert.equal(await host.locator('#host').isDisabled(),false);
    await Promise.all(pages.slice(0,3).map(page=>page.waitForFunction(()=>document.querySelectorAll('#lobby-players progress').length===3)));
    for(const page of pages.slice(1,3))assert.equal(await page.locator('#accept').isDisabled(),false);
    await host.locator('#stun').fill('');
    // Fail after RTCPeerConnection/channels/lobby slot allocation, then restore
    // the native browser API before the next real invitation.
    await host.evaluate(()=>{
      const original=RTCPeerConnection.prototype.createOffer;
      RTCPeerConnection.prototype.createOffer=function(...args){
        globalThis.failedInvitePeer=this;
        RTCPeerConnection.prototype.createOffer=original;
        return Promise.reject(Error('Injected offer failure'));
      };
    });
    await host.locator('#host').click();
    await host.waitForFunction(()=>document.querySelector('#status').textContent.includes('Invitation failed.')&&document.querySelector('#status').textContent.includes('Injected offer failure'));
    await Promise.all(pages.slice(0,3).map(page=>page.waitForFunction(()=>document.querySelectorAll('#lobby-players progress').length===3)));
    assert.equal(await host.locator('#host').isDisabled(),false);
    assert.equal(await host.locator('#cancel-invite').isDisabled(),true);
    assert.equal(await host.evaluate(()=>failedInvitePeer.connectionState),'closed');
    await invite(pages[3]);
    assert.equal(await host.locator('#host').isDisabled(),true);
    await Promise.all(pages.slice(1).map(page=>page.locator('#accept').click()));
    const completed=async page=>page.waitForFunction(()=>document.querySelector('#status').textContent.startsWith('Assets checked and prepared.'));
    await Promise.all(pages.slice(1).map(completed));
    const completeLobby=async page=>page.waitForFunction(()=>
      document.querySelectorAll('#lobby-players progress').length===4&&
      [...document.querySelectorAll('#lobby-players progress')].every(progress=>progress.value===1));
    await Promise.all(pages.map(completeLobby));
    const inputPlans=await Promise.all(pages.map(page=>page.evaluate(async()=>(await import('/app.mjs')).lobbyInputPlan())));
    for(let slot=0;slot<4;slot++){
      assert.deepEqual(inputPlans[slot].owners,[1,2,3,4]);assert.equal(inputPlans[slot].owner,slot+1);
      assert.equal(inputPlans[slot].localPadSlots[slot],0);
      assert.equal(inputPlans[slot].localPadSlots.filter(lane=>lane!==null).length,1);
    }
    const identities=[];
    for(const page of pages.slice(1)) {
      const checked=await page.evaluate(async()=>{
        const id=document.querySelector('#status').textContent.match(/Asset ID: ([a-f0-9]{64})/)[1];
        const {browserStore}=await import('/transfer.mjs'),{verifiedAssetBlob}=await import('/runtime-asset.mjs');
        const store=await browserStore();
        try {
          const asset=await verifiedAssetBlob(store,id),bytes=new Uint8Array(await asset.blob.arrayBuffer());
          return {id,size:bytes.length,correct:bytes.every((byte,index)=>byte===index%251)};
        } finally {store.close();}
      });
      assert.equal(checked.size,payload.length);assert.equal(checked.correct,true);identities.push(checked.id);
      assert.match(await page.locator('#lobby-players h3').filter({hasText:'(You)'}).textContent(),/Guest/);
    }
    assert.ok(identities.every(id=>id===identities[0]));
    const configuration=async page=>page.evaluate(async()=> (await import('/app.mjs')).lobbySessionConfiguration());
    const agreed=await configuration(host);
    assert.equal(agreed.assetId,identities[0]);
    for(const page of pages.slice(1))assert.deepEqual(await configuration(page),agreed);
    for(const page of pages)assert.ok((await page.locator('#lobby-players').textContent()).includes('Awaiting shader preparation'));
    await pages[3].locator('#stop').click();
    await Promise.all(pages.slice(0,3).map(page=>page.waitForFunction(()=>document.querySelectorAll('#lobby-players progress').length===3)));
    assert.equal(await host.locator('#host').isDisabled(),false);
    await invite(pages[3]);await pages[3].locator('#accept').click();await completed(pages[3]);
    await Promise.all(pages.map(completeLobby));
    for(const page of pages)assert.deepEqual(await configuration(page),agreed);
    const replacementPlan=await pages[3].evaluate(async()=>(await import('/app.mjs')).lobbyInputPlan());
    assert.equal(replacementPlan.peer,'guest5');assert.equal(replacementPlan.owner,4);
    assert.ok(replacementPlan.membership>inputPlans[3].membership);
    assert.deepEqual(replacementPlan.localPadSlots,[null,null,null,0]);
    assert.match(await pages[3].locator('#lobby-players h3').nth(3).textContent(),/Guest 5.*\(You\)/);
    const credentials=async page=>page.evaluate(async()=>(await import('/app.mjs')).lobbyRecoveryCredentialState());
    assert.deepEqual((await credentials(host)).map(value=>value.peer).sort(),['guest1','guest2','guest5']);
    for(const page of pages.slice(1)){
      const plan=await page.evaluate(async()=>(await import('/app.mjs')).lobbyInputPlan());
      assert.deepEqual(await credentials(page),[{session:agreed.session,peer:plan.peer,available:true}]);
      assert.equal(await page.evaluate(async()=>{try{(await import('/app.mjs')).createLobbyReplacementTransport('host');return false;}catch(error){return /Paused running lobby recovery required/.test(error.message);}}),true);
    }
    for(const page of pages.slice(1,3))assert.match(await page.locator('#status').textContent(),/Assets checked and prepared/);
    for(const page of pages)await page.evaluate(async()=>{
      const app=await import('/app.mjs');globalThis.inputReceived=[];
      globalThis.testInputRouter=app.bindLobbyInput((owner,bytes)=>inputReceived.push({owner,length:bytes.length}));
    });
    for(const page of pages)await page.evaluate(async()=>{
      const app=await import('/app.mjs'),{encodeInput}=await import('/input-packet.mjs');
      const plan=app.lobbyInputPlan(),config=app.lobbySessionConfiguration();
      const session=Uint8Array.from(config.session.match(/../g),b=>parseInt(b,16));
      testInputRouter.send(encodeInput({session,frame:0n,checkpoint:42,slot:plan.owner-1,pad:new Uint8Array(11)}));
    });
    for(let i=0;i<pages.length;i++){
      await pages[i].waitForFunction(()=>inputReceived.length===3);
      const received=await pages[i].evaluate(()=>inputReceived);
      assert.deepEqual(received.map(packet=>packet.owner).sort(),[1,2,3,4].filter(owner=>owner!==i+1));
      assert.ok(received.every(packet=>packet.length===48));
    }
    await host.locator('#stop').click();
    await Promise.all(pages.slice(1).map(page=>page.waitForFunction(()=>document.querySelector('#status').textContent.includes('Connection closed.'))));
    for(const page of pages)assert.equal(await configuration(page),null);
    for(const page of pages)assert.deepEqual(await credentials(page),[]);
    for(const page of pages){assert.equal(await page.locator('#recovery-panel').isHidden(),true);assert.equal(await page.locator('#recovery-local').inputValue(),'');assert.equal(await page.locator('#recovery-remote').inputValue(),'');}
    assert.deepEqual(errors,[]);
    console.log('PASS: actual invitation/join/download UI preserved two existing guests after invalid settings and an allocated-peer offer failure, served three guests with independently verified 600123-byte caches, replaced a disconnected guest, and stopped all connections. Shader preparation/game launch remain unavailable.');
  } finally {await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
