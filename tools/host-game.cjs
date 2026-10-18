'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const {spawn,fork}=require('node:child_process');
const {automaticHosting,tunnelOrigin}=require('./automatic-hosting.cjs');
const {ensureHostingTool}=require('./ensure-hosting-tools.cjs');
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
function ownedProcess(executable,args,options={}){
 const child=spawn(executable,args,{windowsHide:true,stdio:['ignore','pipe','pipe'],...options});
 let output='',failure=null;
 const capture=bytes=>{output=(output+bytes.toString()).slice(-32768);};
 child.stdout?.on('data',capture);child.stderr?.on('data',capture);
 child.on('error',error=>{failure=error;});
 const done=new Promise(resolve=>{child.once('exit',resolve);child.once('error',()=>resolve(1));});
 let closing;
 return {child,done,get output(){return output;},get failed(){return failure||child.exitCode!==null||child.signalCode!==null;},
  close(){return closing??=(async()=>{
   if(child.exitCode!==null||child.signalCode!==null||failure)return;
   child.kill();await Promise.race([done,delay(3000)]);
   if(child.exitCode===null&&child.signalCode===null){child.kill('SIGKILL');await Promise.race([done,delay(2000)]);}
  })();}};
}
async function jsonRequest(url,{timeout=5000,...options}={}){
 const response=await fetch(url,{...options,redirect:'error',signal:AbortSignal.timeout(timeout)});
 if(!response.ok)throw Error('Website check failed: HTTP '+response.status);
 const length=Number(response.headers.get('content-length'));
 if(length>65536)throw Error('Website check response was too large.');
 const chunks=[];let size=0;for await(const chunk of response.body){size+=chunk.length;if(size>65536)throw Error('Website check response was too large.');chunks.push(chunk);}
 return JSON.parse(Buffer.concat(chunks).toString());
}
async function verifyWebsite(origin,service,{timeout=45000,owner,signal}={}){
 const deadline=Date.now()+timeout;let last;
 do{
  if(service.process.failed)throw Error('The local lobby service stopped.');
  if(owner?.failed)throw Error('The hosting process stopped during startup.');
  signal?.throwIfAborted();
  try{
   const proof=await jsonRequest(origin+'/hosting-check.json',{timeout:Math.min(5000,Math.max(1,deadline-Date.now()))});
   if(proof.instance!==service.instance||proof.buildId!==service.buildId)throw Error('Website belongs to a different game instance.');
   const inventory=await jsonRequest(origin+'/runtime-package.json');
   if(inventory.buildId!==service.buildId)throw Error('Website runtime differs from this game.');
   for(const route of ['/native-session-package.json','/game-disc-path.txt']){
    const response=await fetch(origin+route,{method:'HEAD',redirect:'error',signal:AbortSignal.timeout(5000)});
    if(response.status!==404)throw Error('The website exposed a private package route.');
   }
   return;
  }catch(error){last=error;}
  await delay(500);
 }while(Date.now()<deadline);
 const dnsError=[last,last?.cause,last?.cause?.cause].some(error=>['ENOTFOUND','EAI_AGAIN'].includes(error?.code));
 throw Error(dnsError?'This network could not resolve the lobby website address. Try another network or DNS service; local play is still available.':'Public HTTPS website could not be verified.',{cause:last});
}
function waitMessage(child,predicate,timeout=15000){
 return new Promise((resolve,reject)=>{
  const cleanup=()=>{clearTimeout(timer);child.off('message',message);child.off('exit',exit);child.off('error',error);};
  const message=value=>{if(predicate(value)){cleanup();resolve(value);}};
  const exit=()=>{cleanup();reject(Error('Lobby service stopped during startup.'));};
  const error=value=>{cleanup();reject(value);};
  const timer=setTimeout(()=>{cleanup();reject(Error('Lobby service startup timed out.'));},timeout);
  child.on('message',message);child.once('exit',exit);child.once('error',error);
 });
}
async function createService(runtime){
 const env={...process.env,PARTYBOARD_PORT:'0',PARTYBOARD_LISTEN_ADDRESS:'127.0.0.1',PARTYBOARD_RUNTIME_PACKAGE:path.join(runtime,'native-session-package.json')};
 for(const key of ['PARTYBOARD_PUBLIC_ORIGIN','PARTYBOARD_TLS_CERTIFICATE','PARTYBOARD_TLS_KEY','PARTYBOARD_RUNTIME_DIRECTORY','PARTYBOARD_TEST_RUNTIME_DIRECTORY'])delete env[key];
 const child=fork(path.join(__dirname,'../web/serve.cjs'),[],{env,windowsHide:true,silent:true});
 child.stdout.resume();child.stderr.resume();
 const done=new Promise(resolve=>child.once('exit',resolve));let closing;
 const processOwner={child,get failed(){return child.exitCode!==null||child.signalCode!==null;},close(){return closing??=(async()=>{if(child.connected)child.disconnect();await Promise.race([done,delay(2000)]);if(child.exitCode===null){child.kill();await done;}})();}};
 try{
  const ready=await waitMessage(child,value=>value.type==='hosting-listening');
  if(!/^[a-f0-9]{64}$/.test(ready.buildId))throw Error('A measured runtime is required for automatic hosting.');
  return {...ready,process:processOwner,close:()=>processOwner.close(),async publish(origin){
   const published=waitMessage(child,value=>value.type==='hosting-published'&&value.origin===origin);
   child.send({type:'hosting-publish',origin});await published;
  }};
 }catch(error){await processOwner.close();throw error;}
}
function windowsAdapter({runtime,state,notice=console.error,tool=ensureHostingTool,directTimeout=45000,fallbackTimeout=60000,signal}={}){
 const cache=path.join(state,'tools');
 return {
  notice,signal,service:()=>createService(runtime),
  async publicAddress(){
   const result=await fetch('https://www.cloudflare.com/cdn-cgi/trace',{redirect:'error',signal:AbortSignal.any([AbortSignal.timeout(8000),...(signal?[signal]:[])])});
   if(!result.ok)throw Error('Public address discovery unavailable.');
   const body=await result.text();if(body.length>16384)throw Error('Unexpected public address response.');
   return /^ip=(.+)$/m.exec(body)?.[1].trim();
  },
  async direct(origin,service){
   const binary=await tool('caddy',cache,signal);signal?.throwIfAborted();
   const config=path.join(state,'Caddyfile-'+crypto.randomBytes(12).toString('hex'));
   // No admin API, account credentials, directory serving or system installation.
   // Existing router mappings/firewall rules are never overwritten. If these
   // ports are blocked or owned by another host, use the outgoing-only fallback.
   fs.writeFileSync(config,`{\n admin off\n auto_https disable_redirects\n}\n${new URL(origin).hostname} {\n reverse_proxy 127.0.0.1:${service.port}\n}\n`,{flag:'wx'});
   const owner=ownedProcess(binary,['run','--config',config,'--adapter','caddyfile'],{cwd:state,
    env:{...process.env,XDG_DATA_HOME:path.join(state,'certificates'),XDG_CONFIG_HOME:path.join(state,'caddy-config')}});
   let closing;return {owner,close(){return closing??=(async()=>{await owner.close();fs.unlinkSync(config);})();}};
  },
  async fallback(service){
   const binary=await tool('cloudflared',cache,signal);signal?.throwIfAborted();
   const configuration=path.join(state,'empty-cloudflared-'+crypto.randomBytes(12).toString('hex')+'.yml');
   // Explicit empty config prevents an unrelated user's named-tunnel settings
   // or credentials from being loaded by the account-free Quick Tunnel.
   fs.writeFileSync(configuration,'{}\n',{flag:'wx'});
   const owner=ownedProcess(binary,['tunnel','--config',configuration,'--no-autoupdate','--url','http://127.0.0.1:'+service.port],{cwd:state});
   try{
    const deadline=Date.now()+fallbackTimeout;let origin;
    while(!(origin=tunnelOrigin(owner.output))){signal?.throwIfAborted();if(owner.failed||Date.now()>deadline)throw Error('Free fallback could not start. Check the Internet connection.');await delay(100);}
    let closing;return {origin,owner,close(){return closing??=(async()=>{await owner.close();fs.unlinkSync(configuration);})();}};
   }catch(error){await owner.close();fs.unlinkSync(configuration);throw error;}
  },
  verify:(origin,service,resource)=>verifyWebsite(origin,service,{timeout:origin.endsWith('.trycloudflare.com')?fallbackTimeout:directTimeout,owner:resource?.owner,signal})
 };
}
async function hostGame({runtime,args=[],state,hostingOnly=false,adapter}={}){
 fs.mkdirSync(state,{recursive:true});
 let hosting,game,closing=false,closePromise;
 const controller=new AbortController();
 const close=()=>closePromise??=(async()=>{closing=true;controller.abort();await game?.close();await hosting?.close();})();
 const stop=()=>{close().finally(()=>{process.exitCode=130;});};
 process.once('SIGINT',stop);process.once('SIGTERM',stop);
 try{
  try{hosting=await automaticHosting(adapter||windowsAdapter({runtime,state,signal:controller.signal}));}
  catch(error){if(hostingOnly)throw error;console.error('Online hosting is unavailable; local play can continue. '+error.message);}
  if(closing){await hosting?.close();return 130;}
  if(hosting){console.log('Online lobby website: '+hosting.origin+'/ ('+hosting.mode+')');if(process.send)process.send({type:'hosting-ready',origin:hosting.origin,mode:hosting.mode});}
  if(hostingOnly){await new Promise(resolve=>{process.once('SIGINT',resolve);process.once('SIGTERM',resolve);if(process.send)process.once('message',value=>{if(value?.type==='stop')resolve();});});return 0;}
  const env={...process.env};delete env.PARTYBOARD_LOBBY_URL;if(hosting)env.PARTYBOARD_LOBBY_URL=hosting.origin+'/';
  game=ownedProcess(path.join(runtime,'partyboard.exe'),args,{cwd:runtime,env,stdio:'inherit'});
  return await game.done??1;
 }finally{await close();process.off('SIGINT',stop);process.off('SIGTERM',stop);}
}
if(require.main===module){
 const [runtime,state,...args]=process.argv.slice(2);
 if(!runtime||!state)throw Error('Usage: node host-game.cjs RUNTIME STATE_DIRECTORY [--hosting-only | GAME_ARGUMENTS...]');
 hostGame({runtime:path.resolve(runtime),state:path.resolve(state),hostingOnly:args[0]==='--hosting-only',args}).then(code=>{process.exitCode=code;},error=>{console.error(error.message);process.exitCode=1;});
}
module.exports={windowsAdapter,createService,verifyWebsite,jsonRequest,hostGame,ownedProcess};
