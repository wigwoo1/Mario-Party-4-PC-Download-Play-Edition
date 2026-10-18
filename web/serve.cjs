const http=require('node:http');
const https=require('node:https');
const fs=require('node:fs');
const path=require('node:path');
const {serverListenConfiguration,listeningUrls}=require('./server-listen.cjs');
const {roomHttpHandler}=require('./room-service.cjs');
let roomRequests=roomHttpHandler({publicOrigin:process.env.PARTYBOARD_PUBLIC_ORIGIN});
// Only an owned IPC parent can set the origin during automatic startup. Never
// expose a reconfiguration endpoint to browsers or reset an active room registry.
let hostingPublished=false;
const hostingInstance=process.send?require('node:crypto').randomBytes(24).toString('hex'):null;
const listening=serverListenConfiguration();
const files=new Map([['/','index.html'],['/index.html','index.html'],['/app.mjs','app.mjs'],['/transfer.mjs','transfer.mjs'],['/controller-capabilities.mjs','controller-capabilities.mjs']]);
files.set('/ice-servers.mjs','ice-servers.mjs');
files.set('/display-controls.mjs','display-controls.mjs');
files.set('/touch-controls.mjs','touch-controls.mjs');
for(const name of ['lobby-music.mjs','audio/wifi-menu.opus','audio/currently-spectating.opus'])files.set('/'+name,name);
const port=Number(process.env.PARTYBOARD_PORT || process.env.PARTYBOARD_TEST_PORT || 8784);
files.set('/favicon.ico','favicon.svg');
// Runtime JavaScript may be imported while preparing host/guest lobbies even
// in asset-transfer-only tests. Keep these source files explicitly allowlisted;
// the compiled executable pair remains unavailable unless a runtime directory
// is intentionally configured below.
files.set('/online-input-gate.mjs','online-input-gate.mjs');
for(const name of ['browser-runtime-package.mjs','browser-platform.mjs','session-runtime.mjs','browser-input-exchange.mjs','runtime-mount.mjs','runtime-build.mjs','async-blob-fs.mjs','engine-lifecycle.mjs','start-client.mjs','start-barrier.mjs','pipeline-drain.mjs'])files.set('/'+name,name);
files.set('/audio-layout.mjs','audio-layout.mjs');
files.set('/game-audio-controls.mjs','game-audio-controls.mjs');
files.set('/browser-audio-destination.mjs','browser-audio-destination.mjs');
files.set('/runtime-asset.mjs','runtime-asset.mjs');
files.set('/verified-asset-file.mjs','verified-asset-file.mjs');
files.set('/session-config.mjs','session-config.mjs');
files.set('/download-progress-reporter.mjs','download-progress-reporter.mjs');
files.set('/cached-download-controls.mjs','cached-download-controls.mjs');
files.set('/connection-route.mjs','connection-route.mjs');
files.set('/prepared-runtime-report.mjs','prepared-runtime-report.mjs');
files.set('/shader-readiness.mjs','shader-readiness.mjs');
files.set('/room-client.mjs','room-client.mjs');
for(const name of ['automatic-room.mjs','automatic-preparation.mjs','bundled-pipeline-seed.mjs'])files.set('/'+name,name);
files.set('/lobby-input-ownership.mjs','lobby-input-ownership.mjs');
files.set('/lobby-input-router.mjs','lobby-input-router.mjs');
for(const name of ['realtime-input-recovery.mjs','suspended-input-recovery.mjs'])files.set('/'+name,name);
files.set('/session-input-loss.mjs','session-input-loss.mjs');
files.set('/recovery-identity.mjs','recovery-identity.mjs');
files.set('/replacement-transport.mjs','replacement-transport.mjs');
files.set('/native-gameplay-recovery-test-support.mjs','native-gameplay-recovery-test-support.mjs');
for(const name of ['paused-state-agreement.mjs','recovery-commit.mjs','recovery-poll-replay.mjs','paused-poll-reconciliation.mjs','recovery-route-alignment.mjs'])files.set('/'+name,name);
files.set('/verified-input-adoption.mjs','verified-input-adoption.mjs');
for(const name of ['recovery-barrier.mjs','committed-input-recovery.mjs'])files.set('/'+name,name);
files.set('/pipeline-seed.mjs','pipeline-seed.mjs');
files.set('/pipeline-seed-channel.mjs','pipeline-seed-channel.mjs');
for(const name of ['start-barrier.mjs','start-channels.mjs','start-host.mjs','start-client.mjs','online-input-gate.mjs','reconnect-policy.mjs','lobby-game-input.mjs','session-input.mjs','session-input-exchange.mjs','browser-input-exchange.mjs'])files.set('/'+name,name);
for(const name of ['local-asset-store.mjs','online-menu-handoff.mjs'])files.set('/'+name,name);
for(const name of ['runtime-glyphs.mjs','glyph-texture.mjs','control-glyphs.mjs','browser-input-glyphs.mjs','input-glyph-state.mjs'])files.set('/'+name,name);
for(const name of ['control-bindings.mjs','runtime-control-bindings.mjs','lobby-control-editor.mjs'])files.set('/'+name,name);
for(const name of ['input-packet.mjs','lockstep.mjs','lockstep-probe.mjs','lobby-state.mjs','lobby-view.mjs','lobby-host.mjs','lobby-client.mjs'])files.set('/'+name,name);
// Optional diagnostic runtime: expose only these compiled artifacts and helpers,
// never the selected game file or arbitrary paths in the runtime directory.
const runtimeDirectory=process.env.PARTYBOARD_RUNTIME_DIRECTORY||process.env.PARTYBOARD_TEST_RUNTIME_DIRECTORY;
const runtimePackage=process.env.PARTYBOARD_RUNTIME_PACKAGE;
let runtimeInventory;
if(runtimePackage&&runtimeDirectory)throw Error('Configure either a runtime package or runtime directory');
if(runtimePackage){
  const {runtimePackageRoutes}=require('./runtime-package-routes.cjs');
  const {routes,buildId}=runtimePackageRoutes(runtimePackage);
  // Publish names/identity only: never descriptor paths, asset IDs or secrets.
  runtimeInventory=Buffer.from(JSON.stringify({version:1,profile:'unlocked-no-save-v1',buildId,runtimeArtifacts:[...routes.keys()].filter(name=>!['/contract.mjs','/contract.wasm'].includes(name)).map(name=>name.slice(1))}));
  for(const [route,filename] of routes)files.set(route,filename);
  for(const name of ['initial_pipeline_cache.db','initial_pipeline_cache.json'])files.set('/'+name,path.resolve(path.dirname(runtimePackage),name));
}
if(runtimeDirectory){
  for(const name of ['contract.mjs','contract.wasm'])files.set('/'+name,path.resolve(runtimeDirectory,name));
}
if(!Number.isInteger(port)||port<(process.send?0:1)||port>65535)throw new Error('Invalid port');
const handleRequest=(req,res)=>{
  if(hostingInstance&&req.method==='GET'&&req.url==='/hosting-check.json'){
    res.writeHead(200,{'Content-Type':'application/json','Cache-Control':'no-store'});
    res.end(JSON.stringify({version:1,instance:hostingInstance,buildId:runtimeInventory?JSON.parse(runtimeInventory).buildId:null}));return;
  }
  if(roomRequests(req,res))return;
  let pathname;try{pathname=new URL(req.url,'http://localhost').pathname;}catch{}
  if(runtimeInventory&&['GET','HEAD'].includes(req.method)&&pathname==='/runtime-package.json'){
    res.writeHead(200,{'Content-Type':'application/json','Content-Length':runtimeInventory.length,'Cache-Control':'no-store','X-Content-Type-Options':'nosniff'});res.end(req.method==='HEAD'?undefined:runtimeInventory);return;
  }
  const name=files.get(pathname);
  if(!['GET','HEAD'].includes(req.method)||!name){res.writeHead(404);res.end('Not found');return;}
  res.setHeader('Content-Type',name.endsWith('.wasm')?'application/wasm':name.endsWith('.opus')?'audio/ogg; codecs=opus':name.endsWith('.html')?'text/html; charset=utf-8':name.endsWith('.svg')?'image/svg+xml':name.endsWith('.mjs')?'text/javascript; charset=utf-8':'application/octet-stream');
  res.setHeader('X-Content-Type-Options','nosniff');
  res.setHeader('Cache-Control','no-store');
  const filename=path.isAbsolute(name)?name:path.join(__dirname,name);
  fs.stat(filename,(error,stat)=>{
    if(error||!stat.isFile()){res.statusCode=500;res.end('Read failed');return;}
    let start=0,end=stat.size-1;
    if(name.endsWith('.opus')){
      res.setHeader('Accept-Ranges','bytes');
      if(req.headers.range){
        const range=/^bytes=(\d*)-(\d*)$/.exec(req.headers.range);
        if(range&&(range[1]||range[2])){
          if(!range[1])start=Math.max(0,stat.size-Number(range[2]));
          else{start=Number(range[1]);if(range[2])end=Math.min(end,Number(range[2]));}
        }
        if(!range||!(range[1]||range[2])||(!range[1]&&Number(range[2])===0)||!Number.isSafeInteger(start)||!Number.isSafeInteger(end)||start>end||start>=stat.size){
          res.writeHead(416,{'Content-Range':'bytes */'+stat.size,'Content-Length':0});res.end();return;
        }
        res.statusCode=206;res.setHeader('Content-Range','bytes '+start+'-'+end+'/'+stat.size);
      }
    }
    res.setHeader('Content-Length',Math.max(0,end-start+1));
    if(req.method==='HEAD'||stat.size===0){res.end();return;}
    fs.createReadStream(filename,{start,end}).on('error',()=>{
      if(res.headersSent)res.destroy();else{res.removeHeader('Content-Length');res.statusCode=500;res.end('Read failed');}
    }).pipe(res);
  });
};
const server=listening.tls?https.createServer(listening.tls,handleRequest):http.createServer(handleRequest);
server.on('error',error=>{console.error(error.message);process.exitCode=1;}).listen(port,listening.host,()=>{
  for(const url of listeningUrls(listening,server.address().port))console.log(`Peer transfer page: ${url}`);
  if(process.send)process.send({type:'hosting-listening',port:server.address().port,instance:hostingInstance,buildId:runtimeInventory?JSON.parse(runtimeInventory).buildId:null});
  console.log(listening.loopback?'Local-only static server.':'HTTPS server: guests must use an address covered by a certificate their browser trusts.');
  console.log('No game files or TLS keys are exposed over HTTP. Press Ctrl+C to stop.');
});
if(process.send){
  process.on('message',message=>{
    if(message?.type!=='hosting-publish'||hostingPublished)return;
    const origin=new URL(message.origin);
    if(origin.protocol!=='https:'||origin.origin!==message.origin||origin.username||origin.password)return;
    roomRequests=roomHttpHandler({publicOrigin:origin.origin});hostingPublished=true;
    process.send({type:'hosting-published',origin:origin.origin});
  });
  process.on('disconnect',()=>{server.close();server.closeAllConnections();});
}
