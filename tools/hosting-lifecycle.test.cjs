'use strict';
const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),os=require('node:os');
const {spawn}=require('node:child_process');
const {createService,jsonRequest}=require('./host-game.cjs');
const {fingerprint}=require('./stage-local-package.cjs');
const {downloadPinned}=require('./ensure-hosting-tools.cjs');
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
function directory(t){const dir=fs.mkdtempSync(path.join(os.tmpdir(),'partyboard-hosting-'));t.after(()=>{const relative=path.relative(os.tmpdir(),dir);assert.ok(relative&&!relative.startsWith('..')&&!path.isAbsolute(relative));fs.rmSync(dir,{recursive:true,force:true});});return dir;}
function fixtureRuntime(dir){
 fs.mkdirSync(path.join(dir,'browser-runtime'),{recursive:true});
 const files=[['native/partyboard.exe','partyboard.exe'],['native/dol.dll','dol.dll'],['contract.mjs','browser-runtime/contract.mjs'],['contract.wasm','browser-runtime/contract.wasm']].map(([name,file])=>{fs.writeFileSync(path.join(dir,file),'fixture '+name);return {name,path:file};});
 const descriptor={configuration:{profile:'unlocked-no-save-v1',runtimeArtifacts:['native/partyboard.exe','native/dol.dll']},files};
 descriptor.configuration.buildId=fingerprint(dir,descriptor);fs.writeFileSync(path.join(dir,'native-session-package.json'),JSON.stringify(descriptor));return descriptor;
}
test('actual loopback service publishes its verified origin once, keeps rooms, and shuts down its owned listener',async t=>{
 const runtime=directory(t),descriptor=fixtureRuntime(runtime),service=await createService(runtime);
 t.after(()=>service.close());const origin='https://verified-lobby.example';
 assert.equal(service.buildId,descriptor.configuration.buildId);
 const url='http://127.0.0.1:'+service.port;
 assert.equal((await jsonRequest(url+'/hosting-check.json')).instance,service.instance);
 await service.publish(origin);
 const headers={'content-type':'application/json',origin};
 const room=await jsonRequest(url+'/api/lobbies',{method:'POST',headers,body:JSON.stringify({version:1})});
 assert.ok(room.roomId||room.room,JSON.stringify(Object.keys(room)));
 service.process.child.send({type:'hosting-publish',origin:'https://replacement.example'});
 await delay(30);
 const events=await jsonRequest(url+'/api/lobbies/'+room.room+'/events',{headers:{origin,authorization:'Bearer '+room.token}});
 assert.ok(Array.isArray(events.events));
 const inventory=await jsonRequest(url+'/runtime-package.json');assert.equal(inventory.buildId,service.buildId);
 for(const route of ['/native-session-package.json','/game-disc-path.txt','/hosting-tools.json','/tools/host-game.cjs'])assert.equal((await fetch(url+route)).status,404,route);
 assert.equal((await fetch(url+'/api/lobbies',{method:'POST',headers:{...headers,origin:'https://other.example'},body:'{"version":1}'})).status,403);
 await service.close();await assert.rejects(fetch(url));assert.equal(service.process.failed,true);
});
test('native launch receives only the verified origin and local play survives hosting failure',async t=>{
 const {hostGame}=require('./host-game.cjs');
 const runtime=directory(t),state=path.join(runtime,'state'),script=path.join(runtime,'native-fixture.js'),record=path.join(runtime,'native-fixture.json');
 fs.copyFileSync(process.execPath,path.join(runtime,'partyboard.exe'));
 fs.writeFileSync(script,'require("node:fs").writeFileSync(process.argv[2],JSON.stringify({origin:process.env.PARTYBOARD_LOBBY_URL??null,args:process.argv.slice(3)}));');
 let closed=0,published;
 const adapter={notice(){},service:async()=>({close:async()=>{closed++;},publish:async origin=>{published=origin;}}),publicAddress:async()=> '8.8.4.4',direct:async()=>({close:async()=>{closed++;}}),verify:async()=>{},fallback:async()=>{throw Error('unexpected fallback');}};
 assert.equal(await hostGame({runtime,state,args:[script,record,'--keyboard'],adapter}),0);
 assert.deepEqual(JSON.parse(fs.readFileSync(record)),{origin:published+'/',args:['--keyboard']});assert.equal(closed,2);
 assert.equal(await hostGame({runtime,state,args:[script,record],adapter:{service:async()=>{throw Error('offline');}}}),0);
 assert.equal(JSON.parse(fs.readFileSync(record)).origin,null);
});
test('a corrupt cached hosting tool is rejected before execution or network access',async t=>{
 const dir=directory(t),file=path.join(dir,'tool.exe');fs.writeFileSync(file,'changed');
 await assert.rejects(downloadPinned({size:7,sha256:'0'.repeat(64),url:'https://unused.invalid/'},file),/integrity check/);
 assert.equal(fs.readFileSync(file,'utf8'),'changed');
});
test('DNS failure is explained without advertising an unverified website',async t=>{
 const {verifyWebsite}=require('./host-game.cjs');
 t.mock.method(globalThis,'fetch',async()=>{throw new TypeError('fetch failed',{cause:Object.assign(Error('name unavailable'),{code:'ENOTFOUND'})});});
 await assert.rejects(verifyWebsite('https://unresolved.trycloudflare.com',{process:{failed:false}},{timeout:1}),/network could not resolve.*local play/);
});
for(const hardClose of [false,true])test('Windows job '+(hardClose?'kills descendants when launcher closes abruptly':'preserves arguments and returns the owned process exit code'),{skip:process.platform!=='win32'},async t=>{
 const dir=directory(t),fixture=path.join(dir,'owned child.js'),record=path.join(dir,'record.json');
 fs.writeFileSync(fixture,hardClose?
  "const {spawn}=require('node:child_process');const c=spawn(process.execPath,['-e','setInterval(()=>{},1000)'],{windowsHide:true,stdio:'ignore'});require('node:fs').writeFileSync(process.env.PARTYBOARD_JOB_RECORD,JSON.stringify({pid:process.pid,grandchild:c.pid}));setInterval(()=>{},1000);":
  "require('node:fs').writeFileSync(process.env.PARTYBOARD_JOB_RECORD,JSON.stringify(process.argv.slice(2)));process.exitCode=7;");
 const args=[fixture,'one two','a"b','C:\\folder with spaces\\'];
 const child=spawn('powershell.exe',['-NoLogo','-NoProfile','-NonInteractive','-Command',
  '. $env:PARTYBOARD_JOB_SCRIPT; exit [Partyboard.HostingJob]::Run($env:PARTYBOARD_JOB_NODE,[string[]](ConvertFrom-Json $env:PARTYBOARD_JOB_ARGUMENTS),$env:PARTYBOARD_JOB_CWD)'],
  {windowsHide:true,stdio:['ignore','pipe','pipe'],env:{...process.env,PARTYBOARD_JOB_SCRIPT:path.join(__dirname,'hosting-process-job.ps1'),PARTYBOARD_JOB_NODE:process.execPath,PARTYBOARD_JOB_ARGUMENTS:JSON.stringify(args),PARTYBOARD_JOB_CWD:dir,PARTYBOARD_JOB_RECORD:record}});
 let stderr='';child.stderr.on('data',b=>{stderr+=b;});child.stdout.resume();
 const done=new Promise(resolve=>child.once('exit',resolve));t.after(()=>{if(child.exitCode===null)child.kill();});
 const deadline=Date.now()+15000;while(!fs.existsSync(record)){if(child.exitCode!==null||Date.now()>deadline)assert.fail('Owned child did not start: '+stderr);await delay(50);}
 if(!hardClose){assert.equal(await done,7,stderr);assert.deepEqual(JSON.parse(fs.readFileSync(record)),args.slice(1));return;}
 const owned=JSON.parse(fs.readFileSync(record));child.kill();await done;
 const alive=pid=>{try{process.kill(pid,0);return true;}catch{return false;}};
 const until=Date.now()+5000;while((alive(owned.pid)||alive(owned.grandchild))&&Date.now()<until)await delay(50);
 assert.equal(alive(owned.pid),false);assert.equal(alive(owned.grandchild),false);
});
