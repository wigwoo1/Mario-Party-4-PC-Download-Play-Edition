 'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),{spawn}=require('node:child_process'),{createInterface}=require('node:readline');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
(async()=>{
 const executable=process.env.PARTYBOARD_NATIVE_RECOVERY_CODEC;assert.ok(executable&&fs.existsSync(executable));
 const port=process.env.PARTYBOARD_TEST_PORT||'8818';
 const server=spawn(process.execPath,[path.resolve(__dirname,'../../web/serve.cjs')],{env:{...process.env,PARTYBOARD_TEST_PORT:port},stdio:['ignore','pipe','pipe']});
 const native=spawn(executable,[],{stdio:['pipe','pipe','pipe']});let browser,pending,stderr='';native.stderr.on('data',data=>stderr+=data);
 createInterface({input:native.stdout}).on('line',line=>{if(!pending)return;const task=pending;pending=null;clearTimeout(task.timer);try{task.resolve(JSON.parse(line));}catch(error){task.reject(error);}});
 native.on('error',error=>pending?.reject(error));
 const exited=new Promise(resolve=>native.on('exit',(code,signal)=>{pending?.reject(Error('Native recovery codec exited'));resolve({code,signal});}));
 const send=value=>new Promise((resolve,reject)=>{assert.ok(!pending);pending={resolve,reject,timer:setTimeout(()=>reject(Error('Native recovery response timeout')),10000)};native.stdin.write(JSON.stringify(value)+'\n');});
 try{
  for(let i=0;i<100;i++){try{if((await fetch('http://127.0.0.1:'+port+'/recovery-identity.mjs')).ok)break;}catch{}if(i===99)throw Error('Recovery test server unavailable');await new Promise(resolve=>setTimeout(resolve,50));}
  browser=await chromium.launch({channel:'msedge',headless:true,args:['--mute-audio']});const page=await browser.newPage();await page.goto('http://127.0.0.1:'+port);
  const credential=await send({type:'create-credential',session:'1'.repeat(32),assetId:'a'.repeat(64),peer:'guest1'});
  assert.match(credential.secret,/^[a-f0-9]{64}$/);assert.notEqual(credential.secret,'0'.repeat(64));
  await page.evaluate(async value=>{globalThis.recoveryCodec=await import('/recovery-identity.mjs');globalThis.credential=recoveryCodec.receiveRecoveryCredential(value,value);},credential);
  const nonces=new Set();for(let i=0;i<32;i++){const value=await send({type:'nonce'});assert.match(value.nonce,/^[a-f0-9]{32}$/);assert.ok(!nonces.has(value.nonce));nonces.add(value.nonce);}
  const description={type:'offer',sdp:'v=0\r\na=fingerprint:sha-256 ORIGINAL\r\na=note:\u00e9\u{1f642}\r\n'},nonce='2'.repeat(32);
  const signed=await send({type:'sign',description,nonce});
  const browserSigned=await page.evaluate(async({description,nonce})=>recoveryCodec.signRecoveryDescription(credential,description,{nonce}),{description,nonce});
  assert.deepEqual(signed,browserSigned);
  assert.equal(await page.evaluate(async packet=>!!await recoveryCodec.verifyRecoveryDescription(packet,credential,{type:'offer',usedNonces:new Set()}),signed),true);
  const altered={...browserSigned,description:{...description,sdp:description.sdp+'tampered'}};
  assert.deepEqual(await send({type:'verify',packet:altered,expectedType:'offer'}),{valid:false});
  assert.deepEqual(await send({type:'verify',packet:browserSigned,expectedType:'offer'}),{valid:true});
  assert.deepEqual(await send({type:'verify',packet:browserSigned,expectedType:'offer'}),{valid:false});
  const answer=await page.evaluate(async nonce=>recoveryCodec.signRecoveryDescription(credential,{type:'answer',sdp:'v=0\r\nanswer'},{nonce}),nonce);
  assert.deepEqual(await send({type:'verify',packet:answer,expectedType:'answer',expectedNonce:'0'.repeat(32)}),{valid:false});
  assert.deepEqual(await send({type:'verify',packet:answer,expectedType:'answer',expectedNonce:nonce}),{valid:true});
  assert.deepEqual(await send({type:'verify',packet:{...browserSigned,peer:'guest2'},expectedType:'offer'}),{error:'Rejected recovery command'});
  assert.deepEqual(await send({type:'credential',credential:{...credential,secret:'b'.repeat(64)}}),{error:'Rejected recovery command'});
  for(const sdp of ['a'.repeat(100000),'\u00e9'.repeat(100000),'\u{1f642}'.repeat(50000)]){
   const boundary={type:'offer',sdp};
   const nativePacket=await send({type:'sign',description:boundary,nonce:'3'.repeat(32)});
   const edgePacket=await page.evaluate(async description=>recoveryCodec.signRecoveryDescription(credential,description,{nonce:'3'.repeat(32)}),boundary);
   assert.deepEqual(nativePacket,edgePacket,'UTF-16 boundary signatures must agree');
   const oversized={type:'offer',sdp:sdp+'a'};
   assert.deepEqual(await send({type:'sign',description:oversized,nonce:'4'.repeat(32)}),{error:'Rejected recovery command'});
   assert.equal(await page.evaluate(async description=>{try{await recoveryCodec.signRecoveryDescription(credential,description);return false;}catch{return true;}},oversized),true);
  }
  native.stdin.end();const result=await exited;assert.equal(result.code,0);
  const report={scope:'Actual native MbedTLS and Edge WebCrypto recovery-description interoperability',nativeCredentialGenerationVerified:true,nativeNonceGenerationVerified:true,utf16SDPBoundaryParityVerified:true,nativeAndBrowserSignaturesIdentical:true,nativeToBrowserVerified:true,browserToNativeVerified:true,tamperedSDPRejected:true,replayRejected:true,failedChecksDoNotConsumeNonce:true,answerNonceBound:true,peerChangeRejected:true,credentialReplacementRejected:true,nativeExitCode:0,nativeRTCRecoveryVerified:false,nativeGameIntegrationVerified:false};
  fs.writeFileSync(process.env.PARTYBOARD_NATIVE_RECOVERY_REPORT||'outputs/validation/native-edge-recovery-codec-20261005.json',JSON.stringify(report,null,2)+'\n');console.log('PASS native MbedTLS / actual Edge WebCrypto recovery codec and UTF-16 boundaries.');
 }catch(error){if(stderr)console.error(stderr);throw error;}finally{if(pending)clearTimeout(pending.timer);if(native.exitCode===null)native.kill();server.kill();await browser?.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
