'use strict';
const http=require('node:http'),https=require('node:https'),fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),{spawn,execFileSync}=require('node:child_process');
const {roomHttpHandler}=require('../web/room-service.cjs');
const executable=path.resolve(process.argv[2]||'work/native-room-cmake/partyboard_native_room_probe.exe'),reportPath=path.resolve(process.argv[3]||'../validation/native-room-http-20261017.json');
const checks=[];const sleep=ms=>new Promise(r=>setTimeout(r,ms));
async function until(check,timeout=5000){const end=Date.now()+timeout;while(!check()){if(Date.now()>end)throw Error('Native room probe observation timed out');await sleep(10);}}
function probe(mode,url){
 const child=spawn(executable,[mode,url],{windowsHide:true,stdio:['pipe','pipe','pipe']}),messages=[];let stdout='',stderr='',exit;
 child.stdout.on('data',b=>{stdout+=b;if(stdout.length>1024*1024){child.kill();return;}let end;while((end=stdout.indexOf('\n'))>=0){messages.push(JSON.parse(stdout.slice(0,end)));stdout=stdout.slice(end+1);}});child.stderr.on('data',b=>stderr=(stderr+b).slice(-16000));child.on('error',e=>{stderr=e.message;exit=1;});child.on('exit',code=>exit=code);
 return {child,messages,get exit(){return exit;},send:message=>child.stdin.write(JSON.stringify(message)+'\n'),
  async wait(match,timeout=6000){await until(()=>messages.some(match)||exit!==undefined,timeout);const index=messages.findIndex(match);if(index<0)throw Error('Native probe exited '+exit+': '+stderr+' '+JSON.stringify(messages.filter(m=>m.type==='error')));return messages.splice(index,1)[0];},
  async close(){if(exit!==undefined)return;this.send({type:'stop'});await until(()=>exit!==undefined,4000);assert.equal(exit,0,stderr);},kill(){if(exit===undefined)child.kill();}
 };
}
async function raw(url,cancel=0,timeout=1500){
 const child=spawn(executable,['http',url,String(cancel),String(timeout)],{windowsHide:true,stdio:['ignore','pipe','pipe']});let text='',errors='';child.stdout.on('data',b=>text+=b);child.stderr.on('data',b=>errors+=b);
 const timer=setTimeout(()=>child.kill(),5000);try{const code=await new Promise((r,j)=>{child.once('error',j);child.once('exit',r);});assert.equal(code,0,errors);return JSON.parse(text);}finally{clearTimeout(timer);}
}
async function run(name,action){const result=await action();checks.push({name,status:'passed',...result});console.log('PASS '+name);}
(async()=>{
 const {createRoomClient,readRoomJoinLink}=await import('../web/room-client.mjs');
 let dropCreate=false,dropJoin=false,dropSignal=false,stallPoll=false,malformedPoll=false,redirectHits=0;const registrationIds={create:[],join:[],signal:[]},commands=[],sockets=new Set(),timers=new Set();
 const room=roomHttpHandler();
 const server=http.createServer((req,res)=>{
  const url=new URL(req.url,'http://localhost');
  if(url.pathname.startsWith('/api/lobbies/fixture/')){
   const name=url.pathname.split('/').at(-1);
   if(name==='redirect'){res.writeHead(302,{Location:'/capture'});res.end();return;}
   if(name==='oversize'){res.writeHead(200,{'Content-Length':2048});res.end('x'.repeat(2048));return;}
   if(name==='chunks'){res.writeHead(200);res.write('x'.repeat(800));res.end('y'.repeat(800));return;}
   if(name==='stall')return;
   if(name==='trickle'){res.writeHead(200);const timer=setInterval(()=>res.write('x'),20);timers.add(timer);res.once('close',()=>{clearInterval(timer);timers.delete(timer);});return;}
   assert.equal(req.headers.authorization,'Bearer '+'a'.repeat(43));assert.equal(req.headers.cookie,undefined);res.writeHead(200);res.end('{"version":1}');return;
  }
  if(url.pathname==='/capture'){redirectHits++;res.end('leaked');return;}
  if(url.pathname.endsWith('/events')&&stallPoll)return;
  if(url.pathname.endsWith('/events')&&malformedPoll){res.writeHead(200,{'Content-Type':'application/json'});res.end('{"version":1,"events":[],"cursor":1}');return;}
  const action=req.method==='POST'&&url.pathname==='/api/lobbies'?'create':req.method==='POST'&&url.pathname.endsWith('/join')?'join':req.method==='POST'&&url.pathname.endsWith('/signals')?'signal':null;
  if(action){let body='';req.on('data',b=>body+=b);req.on('end',()=>{registrationIds[action].push(JSON.parse(body).id??null);});}
  if((action==='create'&&dropCreate)||(action==='join'&&dropJoin)||(action==='signal'&&dropSignal)){
   if(action==='create')dropCreate=false;else if(action==='join')dropJoin=false;else dropSignal=false;
   // Let the real service commit the registration, then lose its response.
   res.end=()=>{res.destroy();};
  }
  if(!room(req,res)){res.writeHead(404);res.end();}
 });server.on('connection',socket=>{sockets.add(socket);socket.once('close',()=>sockets.delete(socket));});
 await new Promise(r=>server.listen(0,'127.0.0.1',r));const base='http://127.0.0.1:'+server.address().port,owned=[];let browserClients=[];
 try{
  await run('native URL policy rejects remote plaintext, user info and noncanonical links',async()=>{const result=JSON.parse(execFileSync(executable,['policy'],{encoding:'utf8',windowsHide:true}));assert.equal(result.rejected,9);return result;});
  await run('native async HTTP reads a bounded authenticated JSON response',async()=>{const r=await raw(base+'/api/lobbies/fixture/ok');assert.equal(r.ok,true);assert.equal(r.status,200);return {elapsedMs:r.elapsedMs};});
  await run('HTTP adapter itself rejects embedded credentials and remote plaintext before sending',async()=>{
   const r=await raw(base.replace('http://','http://user:secret@')+'/api/lobbies/fixture/ok');assert.equal(r.ok,false);assert.match(r.error,/Trusted HTTPS/);
   const other=await raw('http://127.0.0.1.evil.invalid/api/lobbies');assert.equal(other.ok,false);assert.match(other.error,/Trusted HTTPS/);
  });
  await run('redirects never forward the bearer credential',async()=>{const r=await raw(base+'/api/lobbies/fixture/redirect');assert.equal(r.status,302);assert.equal(redirectHits,0);});
  await run('declared and chunked oversized responses are rejected',async()=>{for(const name of ['oversize','chunks']){const r=await raw(base+'/api/lobbies/fixture/'+name);assert.equal(r.ok,false);assert.match(r.error,/size limit/);}});
  await run('slow trickling response cannot extend the overall deadline',async()=>{const r=await raw(base+'/api/lobbies/fixture/trickle',0,300);assert.equal(r.ok,false);assert.ok(r.elapsedMs<1200,JSON.stringify(r));return {elapsedMs:r.elapsedMs};});
  await run('cancellation closes stalled async requests and drains final callbacks',async()=>{const results=await Promise.all(Array.from({length:12},()=>raw(base+'/api/lobbies/fixture/stall',100)));for(const r of results){assert.equal(r.ok,false);assert.match(r.error,/cancel/);assert.ok(r.elapsedMs<1200,JSON.stringify(r));}return {attempts:results.length,maximumElapsedMs:Math.max(...results.map(r=>r.elapsedMs))};});
  await run('native host and three browser clients share the same four-player room and reconnect mailbox',async()=>{
   dropCreate=true;const start=registrationIds.create.length,h=probe('host',base+'/');owned.push(h);const {link}=await h.wait(e=>e.type==='ready');
   assert.ok(registrationIds.create.length>=start+2);const ids=registrationIds.create.slice(start);assert.ok(ids.every(id=>id===ids[0]&&/^[a-f0-9]{32}$/.test(id)));
   const guests=Array.from({length:3},()=>createRoomClient({baseURL:base}));browserClients.push(...guests);await Promise.all(guests.map(g=>g.join(readRoomJoinLink(link))));
   const joined=[];for(let i=0;i<3;i++)joined.push(await h.wait(e=>e.type==='joined'));assert.equal(new Set(joined.map(e=>e.peer)).size,3);
   const received=[];const watchers=guests.map(g=>g.watch(async event=>{received.push(event);if(event.kind==='offer')await g.send('host','answer','fixture-answer-'+g.identity.peer);else if(event.kind==='recovery-offer')await g.send('host','recovery-answer','signed-fixture-answer');}).catch(e=>{if(e.name!=='AbortError')commands.push({error:e.message});}));
   dropSignal=true;for(const event of joined)h.send({type:'send',to:event.peer,kind:'offer',payload:'fixture-offer'});
   for(let i=0;i<3;i++){const e=await h.wait(e=>e.kind==='answer');assert.equal(e.payload,'fixture-answer-'+e.from);}
   assert.equal(received.filter(e=>e.kind==='offer').length,3,'A lost send response must not duplicate the delivered offer');assert.ok(new Set(registrationIds.signal).size<registrationIds.signal.length,'The native send retry must reuse its identifier');
   h.send({type:'seal'});await h.wait(e=>e.type==='sealed');const extra=createRoomClient({baseURL:base});await assert.rejects(extra.join(readRoomJoinLink(link)),/already started/);await extra.close();
   h.send({type:'send',to:joined[1].peer,kind:'recovery-offer',payload:'signed-fixture-offer'});const recovered=await h.wait(e=>e.kind==='recovery-answer');assert.equal(recovered.from,joined[1].peer);
   for(const g of guests)await g.close();await Promise.all(watchers);browserClients=[];await h.close();return {peers:4,registrationRetryRecovered:true,reconnectPayloadsAreFixtures:true,gameplayTested:false};
  });
  await run('native guest joins a browser host once despite a lost registration response',async()=>{
   const h=createRoomClient({baseURL:base});browserClients.push(h);const identity=await h.host();const {roomJoinLink}=await import('../web/room-client.mjs');dropJoin=true;const start=registrationIds.join.length;
   const g=probe('join',roomJoinLink(base+'/',identity));owned.push(g);await g.wait(e=>e.type==='ready');const events=[];
   const watch=h.watch(async e=>{events.push(e);if(e.type==='joined')await h.send(e.peer,'offer','guest-fixture-offer');}).catch(e=>{if(e.name!=='AbortError')commands.push({error:e.message});});
   const offer=await g.wait(e=>e.kind==='offer');assert.equal(offer.payload,'guest-fixture-offer');g.send({type:'send',to:'host',kind:'answer',payload:'guest-fixture-answer'});
   await until(()=>events.some(e=>e.payload==='guest-fixture-answer'));assert.equal(events.filter(e=>e.type==='joined').length,1);
   const ids=registrationIds.join.slice(start);assert.ok(ids.length>=2&&ids.every(id=>id===ids[0]));await g.close();await h.close();await watch;browserClients=[];
  });
  await run('closing the native owner cancels a stalled poll without blocking its caller',async()=>{
   const h=probe('host',base+'/');owned.push(h);await h.wait(e=>e.type==='ready');stallPoll=true;await sleep(900);const start=Date.now();await h.close();stallPoll=false;assert.ok(Date.now()-start<1500);return {closeMs:Date.now()-start};
  });
  await run('malformed event history is terminal instead of retried or acknowledged',async()=>{
   malformedPoll=true;const h=probe('host',base+'/');owned.push(h);await h.wait(e=>e.type==='ready');const error=await h.wait(e=>e.type==='error');assert.match(error.message,/Incomplete lobby event history/);await until(()=>h.exit!==undefined);assert.equal(h.exit,2);malformedPoll=false;
  });
  await run('HTTPS rejects an untrusted self-signed certificate without weakening verification',async()=>{
   const fixture=fs.mkdtempSync(path.resolve('work/native-room-tls-')),key=path.join(fixture,'key.pem'),cert=path.join(fixture,'cert.pem');let tlsServer;
   try{
    execFileSync(process.env.PARTYBOARD_TEST_OPENSSL||'C:/Program Files/Git/usr/bin/openssl.exe',['req','-x509','-newkey','rsa:2048','-nodes','-days','1','-subj','/CN=localhost','-addext','subjectAltName=DNS:localhost,IP:127.0.0.1','-keyout',key,'-out',cert],{windowsHide:true,stdio:'ignore'});
    let hits=0;tlsServer=https.createServer({key:fs.readFileSync(key),cert:fs.readFileSync(cert)},(q,s)=>{hits++;s.end('{}');});await new Promise(r=>tlsServer.listen(0,'127.0.0.1',r));
    const r=await raw('https://127.0.0.1:'+tlsServer.address().port+'/api/lobbies');assert.equal(r.ok,false);assert.match(r.error,/Windows error/);assert.equal(hits,0);return {certificateChecksDisabled:false};
   }finally{if(tlsServer)await new Promise(r=>{tlsServer.close(r);tlsServer.closeAllConnections();});for(const file of [key,cert])if(fs.existsSync(file))fs.unlinkSync(file);fs.rmdirSync(fixture);}
  });
  assert.deepEqual(commands,[]);fs.mkdirSync(path.dirname(reportPath),{recursive:true});fs.writeFileSync(reportPath,JSON.stringify({status:'passed',scope:'compiled native room worker + async WinHTTP against the actual local browser room service; signaling fixtures only, no native menu/gameplay claim',checks,at:new Date().toISOString()},null,2));
 }finally{for(const g of browserClients)await g.close().catch(()=>{});for(const p of owned)p.kill();for(const timer of timers)clearInterval(timer);for(const socket of sockets)socket.destroy();await new Promise(r=>server.close(r));}
})().catch(error=>{fs.mkdirSync(path.dirname(reportPath),{recursive:true});fs.writeFileSync(reportPath,JSON.stringify({status:'failed',checks,error:error.stack},null,2));console.error(error);process.exitCode=1;});
