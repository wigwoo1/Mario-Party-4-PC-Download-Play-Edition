const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const assert=require('node:assert/strict');
(async()=>{
  const browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio',...(process.env.PARTYBOARD_TEST_DIRECT_ICE==='1'?['--disable-features=WebRtcHideLocalIpsWithMdns']:[])]});
  try {
    const pages=[],errors=[];
    for(let i=0;i<4;i++) {
      const context=await browser.newContext(),page=await context.newPage();pages.push(page);
      page.on('pageerror',e=>errors.push(e.message));
      await page.goto(process.env.PARTYBOARD_TEST_URL||'http://127.0.0.1:8785');
      await page.evaluate(async stun=>{
        const {iceServerConfiguration}=await import('/ice-servers.mjs');
        const effectiveStun=stun||document.querySelector('#stun').value;
        globalThis.testIceServers=iceServerConfiguration({stun:effectiveStun});globalThis.gatheredCandidateTypes=[];globalThis.gatheringErrors=[];
        globalThis.gather=async pc=>{
        pc.addEventListener('icecandidateerror',event=>gatheringErrors.push({code:event.errorCode,url:event.url}));
        if(pc.iceGatheringState!=='complete')await new Promise((resolve,reject)=>{
          const timer=setTimeout(()=>reject(Error('ICE timeout in state '+pc.iceGatheringState+'; partial candidate types '+JSON.stringify([...new Set([...pc.localDescription.sdp.matchAll(/\btyp\s+(host|srflx|relay|prflx)\b/g)].map(match=>match[1]))].sort())+'; ICE errors '+JSON.stringify(gatheringErrors))),16000);
          pc.addEventListener('icegatheringstatechange',()=>{if(pc.iceGatheringState==='complete'){clearTimeout(timer);resolve();}});
        });
        globalThis.gatheredCandidateTypes.push([...new Set([...pc.localDescription.sdp.matchAll(/\btyp\s+(host|srflx|relay|prflx)\b/g)].map(match=>match[1]))].sort());
        return pc.localDescription.toJSON();
      };
      },process.env.PARTYBOARD_TEST_STUN_SERVER||'');
    }
    const host=pages[0];
    if(process.env.PARTYBOARD_TEST_EXPECT_DEFAULT_STUN==='1')console.log('Default ICE configuration (URL only): '+JSON.stringify(await Promise.all(pages.map(page=>page.evaluate(()=>({stun:document.querySelector('#stun').value,iceServers:testIceServers}))))));
    const manifest=await host.evaluate(async()=>{
      const {LobbyState}=await import('/lobby-state.mjs'),{LobbyHost}=await import('/lobby-host.mjs');
      const {prepare}=await import('/transfer.mjs');
      globalThis.asset=await prepare(new Blob([Uint8Array.from({length:600123},(_,i)=>i%251)]));
      globalThis.state=new LobbyState(asset.manifest.id,asset.manifest.size);state.join('host','Host');
      state.report('host',{assetId:asset.manifest.id,sequence:1,bytes:asset.manifest.size,verified:true,shaders:false,ready:false});
      globalThis.transport=new LobbyHost(state);globalThis.connections=[];
      return asset.manifest;
    });
    for(let i=1;i<4;i++) {
      const offer=await host.evaluate(async i=>{
        const pc=new RTCPeerConnection({iceServers:testIceServers});connections.push(pc);
        transport.attach('p'+i,'Player '+i,pc.createDataChannel('lobby',{ordered:true}));
        const {host}=await import('/transfer.mjs');
        host(pc.createDataChannel('assets',{ordered:true}),asset,error=>{throw error;});
        await pc.setLocalDescription(await pc.createOffer());return gather(pc);
      },i);
      const answer=await pages[i].evaluate(async({offer,i,manifest})=>{
        const {LobbyClient}=await import('/lobby-client.mjs'),{renderLobby}=await import('/lobby-view.mjs');
        const {guest,browserStore}=await import('/transfer.mjs');
        const store=await browserStore();globalThis.assetStore=store;
        const client=new LobbyClient(manifest.id,manifest.size),pc=new RTCPeerConnection({iceServers:testIceServers});
        globalThis.lobbyPeer=pc;
        let sequence=0;
        const report=(bytes,verified=false)=>statusChannel.send(JSON.stringify({type:'lobby-report',version:1,report:{assetId:manifest.id,sequence:++sequence,bytes,verified,shaders:false,ready:false}}));
        globalThis.sawIntermediate=false;
        globalThis.lobbyPeer=pc;
        const view=document.createElement('section');view.id='test-lobby';document.body.append(view);
        pc.ondatachannel=event=>{
          if(event.channel.label==='assets') {
            globalThis.receiver=guest(event.channel,store,{offer:()=>{globalThis.assetOffered=true;},progress:bytes=>report(bytes),complete:()=>{report(manifest.size,true);globalThis.downloadDone=true;},error:error=>{throw error;}});
            return;
          }
          globalThis.statusChannel=event.channel;event.channel.onmessage=message=>{
          const snapshot=client.receive(message.data);if(snapshot){globalThis.snapshot=snapshot;renderLobby(view,snapshot,'p'+i);if(snapshot.players.some(p=>p.bytes>0&&p.bytes<manifest.size))globalThis.sawIntermediate=true;}
        };};
        await pc.setRemoteDescription(offer);await pc.setLocalDescription(await pc.createAnswer());return gather(pc);
      },{offer,i,manifest});
      await host.evaluate(({answer,i})=>connections[i-1].setRemoteDescription(answer),{answer,i});
      try {
        await pages[i].waitForFunction(()=>globalThis.statusChannel?.readyState==='open');
      } catch (error) {
        // Keep diagnostics to ICE candidate types and state; never record IPs
        // or mDNS hostnames from the browser's SDP.
        const hostState=await host.evaluate(index=>{
          const pc=connections[index];
          return {connection:pc?.connectionState,ice:pc?.iceConnectionState,signaling:pc?.signalingState,
            gathering:pc?.iceGatheringState,candidates:gatheredCandidateTypes[index]};
        },i-1);
        const guestState=await pages[i].evaluate(()=>({connection:lobbyPeer?.connectionState,
          ice:lobbyPeer?.iceConnectionState,signaling:lobbyPeer?.signalingState,
          gathering:lobbyPeer?.iceGatheringState,candidates:gatheredCandidateTypes.at(-1),
          status:document.querySelector('#status')?.textContent}));
        throw Error('Lobby channel did not open; safe ICE diagnostics '+JSON.stringify({host:hostState,guest:guestState})+'; '+error.message);
      }
    }
    for(const page of pages.slice(1))await page.waitForFunction(()=>globalThis.assetOffered);
    await Promise.all(pages.slice(1).map(page=>page.evaluate(()=>receiver.accept())));
    for(const page of pages.slice(1))await page.waitForFunction(()=>globalThis.downloadDone && globalThis.snapshot?.players.length===4 && snapshot.players.every(p=>p.verified));
    const expected=await host.evaluate(()=>state.snapshot());
    for(let i=1;i<4;i++) {
      assert.deepEqual(await pages[i].evaluate(()=>snapshot),expected);
      assert.deepEqual(await pages[i].locator('#test-lobby progress').evaluateAll(nodes=>nodes.map(n=>n.value)),[1,1,1,1]);
      assert.equal(await pages[i].evaluate(()=>sawIntermediate),true);
      assert.equal(expected.canStart,false); // Downloaded is not shader-ready.
      assert.equal(await pages[i].evaluate(async manifest=>{
        let offset=0;
        for(let n=0;n<manifest.count;n++) {
          const chunk=await assetStore.get(manifest.id,n);
          for(const byte of new Uint8Array(chunk.bytes))if(byte!==(offset++%251))return false;
        }
        return offset===manifest.size;
      },manifest),true);
      assert.match(await pages[i].locator('#test-lobby h3').nth(i).textContent(),/\(You\)$/);
    }
    if(process.env.PARTYBOARD_TEST_STUN_SERVER||process.env.PARTYBOARD_TEST_EXPECT_DEFAULT_STUN==='1'){
      const candidateTypes=await Promise.all(pages.map(page=>page.evaluate(()=>globalThis.gatheredCandidateTypes)));
      console.log('Public STUN production-component evidence (candidate types only): '+JSON.stringify(candidateTypes));
      for(const [peerIndex,gatherings] of candidateTypes.entries()){
        assert.ok(gatherings.length>0,'Peer '+peerIndex+' must gather ICE candidates');
        if(peerIndex===0)for(const [connectionIndex,types] of gatherings.entries())assert.ok(types.includes('srflx'),
          'Offerer connection '+connectionIndex+' must gather a server-reflexive candidate');
      }
    }
    await pages[3].evaluate(()=>lobbyPeer.close());
    for(const page of pages.slice(1,3))await page.waitForFunction(()=>snapshot.players.length===3);
    assert.deepEqual(errors,[]);
    console.log('PASS: three isolated Edge guests downloaded and verified 600123 bytes each over WebRTC, shared live progress, stayed unready for shaders, and removed a disconnected guest.');
  }finally{await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
