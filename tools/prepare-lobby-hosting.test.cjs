'use strict';
const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),os=require('node:os');
const {hostingOrigin,prepareLobbyHosting}=require('./prepare-lobby-hosting.cjs'),{stageLocalPackage,fingerprint}=require('./stage-local-package.cjs');
function fixture(t){
 const root=fs.mkdtempSync(path.join(os.tmpdir(),'partyboard-hosting-'));
 t.after(()=>{const r=path.relative(os.tmpdir(),root);assert.ok(r&&!r.startsWith('..')&&!path.isAbsolute(r));fs.rmSync(root,{recursive:true,force:true});});
 const runtime=path.join(root,'runtime');fs.mkdirSync(path.join(runtime,'browser-runtime'),{recursive:true});
 const files=[['native/dol.dll','dol.dll'],['native/partyboard.exe','partyboard.exe'],['contract.mjs','browser-runtime/contract.mjs'],['contract.wasm','browser-runtime/contract.wasm']].map(([name,file])=>{fs.writeFileSync(path.join(runtime,file),'fixture '+name);return {name,path:file};});
 const d={configuration:{profile:'unlocked-no-save-v1',runtimeArtifacts:['native/dol.dll','native/partyboard.exe']},files};d.configuration.buildId=fingerprint(runtime,d);fs.writeFileSync(path.join(runtime,'native-session-package.json'),JSON.stringify(d));
 const seed=Buffer.from('fixture seed');fs.writeFileSync(path.join(runtime,'initial_pipeline_cache.db'),seed);fs.writeFileSync(path.join(runtime,'initial_pipeline_cache.json'),JSON.stringify({version:1,scope:'discovered-pipelines-only',pipelineCount:1,bytes:seed.length,sha256:require('node:crypto').createHash('sha256').update(seed).digest('hex')}));
 const candidate=path.join(root,'candidate');stageLocalPackage(runtime,candidate);return {root,candidate,target:path.join(root,'hosting'),d};
}
test('hosting origin accepts a root HTTPS domain and rejects settings that could enter server configuration',()=>{
 assert.equal(hostingOrigin('https://Lobby.Example.com/'),'https://lobby.example.com');
 for(const value of ['http://lobby.example.com/','https://lobby.example.com:9443/','https://lobby.example.com/private','https://u:p@lobby.example.com/','https://lobby.example.com/#room=private','https://127.0.0.1/','https://localhost/','https://lobby.example.com/?key=x','https://lobby.example.com/\nother {'])assert.throws(()=>hostingOrigin(value));
});
test('hosting kit preserves the measured release and separates the public TLS proxy from the loopback service',t=>{
 const f=fixture(t),r=prepareLobbyHosting(f.candidate,'https://lobby.example.com/',f.target);
 assert.equal(r.buildId,f.d.configuration.buildId);assert.equal(r.publicDeploymentVerified,false);assert.equal(r.linuxServiceVerified,false);
 const unit=fs.readFileSync(path.join(f.target,'partyboard-lobby.service'),'utf8'),env=fs.readFileSync(path.join(f.target,'partyboard-lobby.env'),'utf8'),proxy=fs.readFileSync(path.join(f.target,'Caddyfile'),'utf8');
 assert.match(unit,/DynamicUser=yes/);assert.match(unit,/ProtectSystem=strict/);assert.match(unit,/ExecStart=\/usr\/bin\/node \/opt\/partyboard\/current\/partyboard\/web\/serve.cjs/);
 assert.match(env,/PARTYBOARD_LISTEN_ADDRESS=127.0.0.1/);assert.match(env,/PARTYBOARD_PUBLIC_ORIGIN=https:\/\/lobby.example.com/);assert.match(proxy,/reverse_proxy 127.0.0.1:8784/);assert.doesNotMatch(proxy,/file_server/);
 assert.equal(fs.readFileSync(path.join(f.target,'lobby-service-url.txt'),'utf8'),'https://lobby.example.com/\n');
 assert.ok(!fs.readFileSync(path.join(f.target,'READ ME.md'),'utf8').includes(f.root));
 assert.throws(()=>prepareLobbyHosting(f.candidate,'https://lobby.example.com/',f.target),/already exists/);
});
test('changed runtime bytes reject hosting preparation before any output exists',t=>{
 const f=fixture(t);fs.appendFileSync(path.join(f.candidate,'runtime/dol.dll'),'changed');
 assert.throws(()=>prepareLobbyHosting(f.candidate,'https://lobby.example.com/',f.target),/measured candidate/);assert.equal(fs.existsSync(f.target),false);
});
