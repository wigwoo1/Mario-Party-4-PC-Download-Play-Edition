'use strict';
const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),os=require('node:os'),path=require('node:path'),{spawnSync}=require('node:child_process');
function fixture(t){
 const root=fs.mkdtempSync(path.join(os.tmpdir(),'partyboard-launcher-'));
 t.after(()=>{const relative=path.relative(os.tmpdir(),root);assert.ok(relative&&!relative.startsWith('..')&&!path.isAbsolute(relative));fs.rmSync(root,{recursive:true,force:true});});
 const tools=path.join(root,'partyboard/tools'),runtime=path.join(root,'runtime');fs.mkdirSync(tools,{recursive:true});fs.mkdirSync(path.join(runtime,'browser-runtime'),{recursive:true});
 const script=path.join(tools,'launch-local.ps1');fs.copyFileSync(path.join(__dirname,'launch-local.ps1'),script);
 for(const name of ['partyboard.exe','dol.dll','initial_pipeline_cache.db','initial_pipeline_cache.json','native-session-package.json','browser-runtime/contract.mjs','browser-runtime/contract.wasm'])fs.writeFileSync(path.join(runtime,name),'fixture only; never executed');
 fs.writeFileSync(path.join(root,'native-runtime-path.txt'),'runtime\n');
 const run=(args=[],extra={})=>{
  const env={...process.env,...extra};if(!Object.hasOwn(extra,'PARTYBOARD_LOBBY_URL'))delete env.PARTYBOARD_LOBBY_URL;
  const result=spawnSync('powershell.exe',['-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',script,'-Inspect',...args],{env,encoding:'utf8',timeout:15000,windowsHide:true});
  if(result.error)throw result.error;return result;
 };
 const inspect=(args,env)=>{const result=run(args,env);assert.equal(result.status,0,result.stderr);const value=JSON.parse(result.stdout);assert.equal(value.launched,false);return value;};
 return {root,run,inspect,setting:path.join(root,'lobby-service-url.txt')};
}
test('native launcher loads one saved public lobby website and allows a per-launch override without launching a game',t=>{
 const f=fixture(t);assert.equal(f.inspect([]).lobbyWebsite,null);assert.equal(f.inspect([]).automaticHosting,true);
 assert.equal(f.inspect(['-LocalOnly']).automaticHosting,false);
 fs.writeFileSync(f.setting,' https://lobby.example/index.html\r\n');
 assert.equal(f.inspect(['-Mode','Keyboard']).lobbyWebsite,'https://lobby.example/');
 assert.equal(f.inspect([]).automaticHosting,false);
 assert.equal(f.inspect(['-LocalOnly']).lobbyWebsite,null);
 assert.equal(f.inspect(['-LobbyWebsite','https://other.example:9443/']).lobbyWebsite,'https://other.example:9443/');
 assert.equal(fs.readFileSync(f.setting,'utf8'),' https://lobby.example/index.html\r\n');
});
test('native launcher supports existing service environment and loopback test origins',t=>{
 const f=fixture(t);assert.equal(f.inspect([],{PARTYBOARD_LOBBY_URL:'https://configured.example/'}).lobbyWebsite,'https://configured.example/');
 for(const url of ['http://127.0.0.1:8954/','http://localhost:8784/','http://[::1]:8954/'])assert.equal(f.inspect(['-LobbyWebsite',url]).lobbyWebsite,url);
});
test('native launcher rejects remote plaintext, embedded credentials and private join links before a process can start',t=>{
 const f=fixture(t);
 for(const url of ['http://10.0.0.198:8784/','https://name:password@lobby.example/','https://lobby.example/#room=private&key=private','https://lobby.example/?token=private','https://lobby.example/nested/','file:///C:/private']){
  const result=f.run(['-LobbyWebsite',url]);assert.notEqual(result.status,0,url);assert.match(result.stderr,/HTTPS|shared lobby website/);
 }
});
test('empty or oversized saved websites fail clearly; browser serving does not inherit a native service setting',t=>{
 const f=fixture(t);fs.writeFileSync(f.setting,'\r\n');assert.match(f.run([]).stderr,/setting is empty/);
 fs.writeFileSync(f.setting,'x'.repeat(4097));assert.match(f.run([]).stderr,/setting is too large/);
 assert.equal(f.inspect(['-Mode','Browser']).lobbyWebsite,null);
});
