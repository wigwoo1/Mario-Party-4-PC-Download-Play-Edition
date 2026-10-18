// Local diagnostic IPC relay, not production transport or peer authentication.
const {fork}=require('node:child_process');
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {compareBoundaryLogs}=require('./compare-boundary-logs.cjs');
(async()=>{
 const polls=Number(process.argv[3]||3);assert.ok(Number.isInteger(polls)&&polls>=1&&polls<=12000);
 const stimulusMode=process.argv[5],stimulus=['stimulus','startup','party','fourhuman','characters','board','boardturn','fourstimulus'].includes(stimulusMode),rtc=process.argv[4]==='rtc',ready=[false,false],signals=[[],[]];
 const digest=file=>new Promise((resolve,reject)=>{const hash=require('node:crypto').createHash('sha256'),stream=fs.createReadStream(file);stream.on('error',reject);stream.on('data',chunk=>hash.update(chunk));stream.on('end',()=>resolve(hash.digest('hex')));});
 const [buildId,assetId]=process.argv[6]==='loss'?await Promise.all([(async()=>{const {fingerprintRuntimeFiles}=await import(require('node:url').pathToFileURL(path.resolve(__dirname,'../web/runtime-build.mjs')).href);return (await fingerprintRuntimeFiles(['contract.mjs','contract.wasm'].map(name=>({name,blob:new Blob([fs.readFileSync(path.join(process.argv[2],name))])})))).buildId;})(),digest(process.env.PARTYBOARD_LOCAL_CISO)]):['',''];
 const children=[],logs=[],pending=[[],[]];let relayed=0;const audited=new Map();
 const timer=setTimeout(()=>children.forEach(child=>child.kill()),Math.max(60000,polls*40+60000));
 try {
  const runs=[11,22].map((peer,index)=>new Promise((resolve,reject)=>{
   const env={...process.env,PARTYBOARD_TEST_TRACE_STATE:'1',PARTYBOARD_TEST_RECOVERY_BUILD_ID:process.argv[7]==='badbuild'&&index===1?'0'.repeat(64):buildId,PARTYBOARD_TEST_RECOVERY_ASSET_ID:process.argv[7]==='badasset'&&index===1?'0'.repeat(64):assetId,PARTYBOARD_TEST_INPUT_CROSS_LOSS:process.argv[6]==='loss'?'1':'0',PARTYBOARD_TEST_INPUT_CROSS_STIMULUS_MODE:stimulusMode||'stimulus',PARTYBOARD_TEST_INPUT_CROSS_STIMULUS:stimulus?'1':'0',PARTYBOARD_TEST_INPUT_CROSS_RTC:rtc?'1':'0',PARTYBOARD_TEST_INPUT_CROSS_PEER:String(peer),PARTYBOARD_TEST_INPUT_CROSS_POLLS:String(polls),PARTYBOARD_TEST_IMAGE_OUTPUT:`outputs/validation/browser-cross-engine-${peer}.png`};
   for(const flag of ['EXTERNAL_ENGINE','EXTERNAL_ENGINE_STOP','ALIGN_START_CLOCK','INPUT_EXCHANGE','INPUT_CANCEL','INPUT_QUEUE'])env['PARTYBOARD_TEST_'+flag]='1';
   for(const flag of ['INPUT_RTC','INPUT_RTC_LOSS','INPUT_RTC_RESUME','INPUT_SUSPEND','INPUT_MULTI_LOSS'])delete env['PARTYBOARD_TEST_'+flag];
   const child=fork(path.join(__dirname,'test-wasm-browser.cjs'),[process.argv[2],'watcherboot'],{env,silent:true});children[index]=child;logs[index]='';
   const liveLog=`work/browser-cross-engine-${peer}.log`;fs.writeFileSync(liveLog,'');const capture=data=>{logs[index]+=data;fs.appendFileSync(liveLog,data);};child.stdout.on('data',capture);child.stderr.on('data',capture);
   child.on('message',message=>{
    try {
     if(message.type==='ready'){ready[index]=true;for(const signal of signals[index])child.send(signal);signals[index]=[];return;}
     if(message.type==='signal'){const target=1-index;if(ready[target])children[target].send(message);else signals[target].push(message);return;}
     assert.equal(message.type,'input');const bytes=Buffer.from(message.packet);assert.equal(bytes.length,48);assert.equal(['fourhuman','characters','board','boardturn','fourstimulus'].includes(stimulusMode)?bytes[5]%2:bytes[5],index);
     const auditedFrame=bytes.readBigUInt64LE(24),auditLimit=polls+(process.argv[6]==='loss'?64:0);
     if(auditedFrame<BigInt(auditLimit)){
       const key=`${bytes[5]}:${auditedFrame}`,previous=audited.get(key);
       if(previous){assert.deepEqual(bytes,previous.bytes,'Retained resend bytes changed');previous.count++;}
       else audited.set(key,{bytes:Buffer.from(bytes),count:1});
     }
     // Forward the first poll; deliberately withhold the next to prove waiting.
     if(bytes.readBigUInt64LE(24)>=BigInt(polls))return;
     const target=1-index,packet={type:'input',peer,packet:message.packet};relayed++;
     if(rtc)return;
     if(children[target])children[target].send(packet);else pending[target].push(packet);
    }catch(error){reject(error);}
   });
   for(const packet of pending[index])child.send(packet);
   child.on('error',reject);child.on('exit',code=>code===0?resolve():reject(Error(`Engine ${peer} exited ${code}`)));
  }));
  await Promise.all(runs);assert.equal(relayed,polls*(['fourhuman','characters','board','boardturn','fourstimulus'].includes(stimulusMode)?4:2));
  const auditPolls=polls+(process.argv[6]==='loss'?64:0),slots=['fourhuman','characters','board','boardturn','fourstimulus'].includes(stimulusMode)?4:2;
  assert.equal(audited.size,auditPolls*slots,'Every merged poll must have every owned packet');
  for(let frame=0;frame<auditPolls;frame++)for(let slot=0;slot<slots;slot++)assert.equal(audited.get(`${slot}:${frame}`).count,process.argv[6]==='loss'&&frame===polls?2:1,'Unexpected retained packet send count');
  const paths=logs.map((log,index)=>{const file=`work/browser-cross-engine-${[11,22][index]}.log`;fs.writeFileSync(file,log);return file;});
  if(stimulusMode==='startup'&&polls>=2004)for(const log of logs)assert.ok(log.includes('External engine final overlay: 74'),'Startup must reach actual mode-select overlay');
  if(['party','fourhuman','characters'].includes(stimulusMode)&&polls>=3204)for(const log of logs)assert.ok(log.includes('External engine final overlay: 70'),'Party stimulus must reach actual party setup overlay');
  if(process.argv[6]==='loss')for(const log of logs){assert.ok(log.includes('Independent actual engine start agreement passed:'),'Both actual engines must complete RTC start agreement');assert.ok(log.includes('Actual cross-engine RTC loss passed:'));assert.ok(log.includes('Actual cross-engine RTC deadline passed:'));assert.ok(log.includes('Actual cross-engine RTC replacement passed:'));assert.ok(log.includes('Actual cross-engine RTC recovery state passed:'));assert.ok(log.includes('Actual cross-engine RTC resume passed:'));}
  for(const log of logs){
    const prepared=log.match(/Actual prepared selected-state checkpoint: (\d+)/);
    const first=log.match(/Actual PAD boundary: (\{[^\r\n]+\})/);
    assert.ok(prepared&&first,'Prepared and first online boundary checkpoints required');
    assert.equal(JSON.parse(first[1]).checkpoint,Number(prepared[1]),'First poll must precede any prepared simulation change');
  }
  const result=compareBoundaryLogs(...paths);
  if(process.argv[6]==='loss')result.pacing=logs.map((log,index)=>{
    const match=log.match(/Actual resumed engine pacing: (\{[^\r\n]+\})/);assert.ok(match,'Every engine must report resumed pacing');
    const measurement=JSON.parse(match[1]);assert.equal(measurement.polls,64);assert.ok(Number.isFinite(measurement.elapsedMs)&&measurement.elapsedMs>=measurement.minimumMs);
    return {peer:[11,22][index],...measurement};
  });
  const finalStates=logs.map(log=>[...log.matchAll(/State snapshot: (\{[^\r\n]+\})/g)].map(match=>JSON.parse(match[1])).find(state=>state.sample===4294967293));
  assert.ok(finalStates.every(Boolean),'Actual final engine snapshots required');assert.deepEqual(finalStates[0],finalStates[1],'Final engine state differs');result.finalState=finalStates[0];assert.equal(result.finalState.frame,auditPolls,'Only accepted online polls may advance simulation frames');
  const prefix='Cross-engine suspended PAD: ';
  const suspended=logs.map(log=>{const line=log.split('\n').find(line=>line.includes(prefix));assert.ok(line,'Suspended PAD metadata required');return JSON.parse(line.slice(line.indexOf(prefix)+prefix.length));});assert.deepEqual(suspended[0],suspended[1],'Suspended PAD execution point differs');assert.equal(suspended[0].frame,String(polls));result.suspendedInput=suspended[0];
  if(stimulusMode==='board'&&polls>=10004){assert.equal(result.finalState.overlay,89,'Board stimulus must reach Toad board');assert.equal(result.finalState.system[0],1,'Board must start on turn one');assert.deepEqual(result.finalState.players.map(player=>player.slice(0,3)),[[0,0,0],[1,1,0],[2,2,0],[3,3,0]],'Four human character configurations must be committed');}
  const merged=logs.map(log=>[...log.matchAll(/Actual merged PAD: (\{[^\r\n]+\})/g)].map(match=>JSON.parse(match[1])));
  assert.equal(merged[0].length,polls+(process.argv[6]==='loss'?64:0));assert.deepEqual(merged[0],merged[1],'Merged engine PAD input differs');
  if(stimulus){const {diagnosticPad}=await import('../web/diagnostic-input.mjs');for(const record of merged[0]){
    const expected=new Uint8Array(44);expected[32]=expected[43]=255;expected.set(diagnosticPad(11,record.poll,stimulusMode),0);expected.set(diagnosticPad(22,record.poll,stimulusMode),11);if(['fourhuman','characters','board','boardturn','fourstimulus'].includes(stimulusMode)){expected.set(diagnosticPad(11,record.poll,stimulusMode,2),22);expected.set(diagnosticPad(22,record.poll,stimulusMode,3),33);}
    assert.deepEqual(record.pads,Array.from(expected),'Merged input must match both owned stimuli');}}
  result.start=rtc&&process.argv[6]==='loss'?{independentEngines:2,proposalAcknowledgmentCommit:true,onlineInputRequired:true}:undefined;result.identity=process.argv[6]==='loss'?{buildId,assetId}:undefined;result.packetAudit={unique:audited.size,sends:[...audited.values()].reduce((sum,entry)=>sum+entry.count,0)};result.input={matched:true,polls:merged[0].length,stimulus,stimulusMode};result.recovery=process.argv[6]==='loss'?{disconnected:true,replacement:true,stateAgreement:true,resumedPolls:64,injectedDeadline:true}:undefined;result.scope=`Two actual concurrent engines exchanging ${merged[0].length} merged PAD samples over ${rtc?'real cross-process WebRTC':'diagnostic IPC'}; next poll withheld. No full-state or playable-netplay claim.`;
  fs.writeFileSync('outputs/validation/browser-cross-engine-comparison.json',JSON.stringify(result,null,2)+'\n');const {records,...summary}=result;console.log(JSON.stringify(summary));
 }finally{clearTimeout(timer);children.forEach(child=>{if(child.exitCode===null)child.kill();});logs.forEach((log,index)=>fs.writeFileSync(`work/browser-cross-engine-${[11,22][index]}.log`,log));}
})().catch(error=>{
 if(['badbuild','badasset'].includes(process.argv[7])){
  const rejected=fs.readFileSync('work/browser-cross-engine-22.log','utf8');
  assert.ok(rejected.includes('Local file digest mismatch'),'Wrong build must reject local digest');
  assert.ok(!rejected.includes('Actual PAD boundary:')&&!rejected.includes('Independent local recovery digests verified'),'Mismatched peer must never execute the game');
  console.log('PASS: mismatched recovery '+(process.argv[7]==='badbuild'?'build':'asset')+' rejected before browser launch/game input.');return;
 }
 console.error(error);process.exitCode=1;
});
