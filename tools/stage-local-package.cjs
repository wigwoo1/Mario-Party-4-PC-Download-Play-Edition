'use strict';
// Create a fresh local release candidate from an installed, measured runtime.
// No build directory traversal, user profile, game image or machine paths copied.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const {runtimePackageRoutes}=require('../web/runtime-package-routes.cjs');
const sha=bytes=>crypto.createHash('sha256').update(bytes).digest('hex');
function fingerprint(root,descriptor){
 const files=descriptor.files.map(entry=>{const bytes=fs.readFileSync(path.resolve(root,entry.path));
  return {name:entry.name,size:bytes.length,sha256:sha(bytes)};});
 files.sort((a,b)=>a.name<b.name?-1:a.name>b.name?1:0);
 return sha(JSON.stringify({version:1,profile:'unlocked-no-save-v1',files}));
}
function stageLocalPackage(runtimeDirectory,destination,projectDirectory=path.resolve(__dirname,'..')){
 const runtime=fs.realpathSync(runtimeDirectory),target=path.resolve(destination),project=fs.realpathSync(projectDirectory);
 if(fs.existsSync(target))throw Error('Package destination already exists; choose a fresh directory');
 const descriptorFile=path.join(runtime,'native-session-package.json');
 const {routes,buildId}=runtimePackageRoutes(descriptorFile);
 const descriptor=JSON.parse(fs.readFileSync(descriptorFile,'utf8'));
 for(const required of ['/native/partyboard.exe','/native/dol.dll'])if(!routes.has(required))throw Error('Native executable and game library required');
 if(fingerprint(runtime,descriptor)!==buildId)throw Error('Installed runtime fingerprint differs from descriptor; rebuild measurement first');
 const metadata=JSON.parse(fs.readFileSync(path.join(runtime,'initial_pipeline_cache.json'),'utf8'));
 const seed=fs.readFileSync(path.join(runtime,'initial_pipeline_cache.db'));
 if(metadata.version!==1||metadata.scope!=='discovered-pipelines-only'||!Number.isInteger(metadata.pipelineCount)||metadata.pipelineCount<1||metadata.bytes!==seed.length||metadata.sha256!==sha(seed))throw Error('Bundled shader integrity check failed');
 const plan=new Map();
 function add(source,relative,allowedRoot,expectedSha){
  const actual=fs.realpathSync(source),inside=path.relative(allowedRoot,actual);
  if(inside==='..'||inside.startsWith('..'+path.sep)||path.isAbsolute(inside)||!fs.statSync(actual).isFile())throw Error('Package input escapes its allowed directory');
  if(plan.has(relative))throw Error('Duplicate package destination');
  const bytes=fs.readFileSync(actual);
  if(expectedSha&&sha(bytes)!==expectedSha)throw Error('Declared notice hash differs: '+relative);
  plan.set(relative,{source:actual,bytes});
 }
 for(const entry of descriptor.files)add(path.resolve(runtime,entry.path),'runtime/'+path.relative(runtime,path.resolve(runtime,entry.path)).split(path.sep).join('/'),runtime);
 for(const entry of descriptor.files)entry.path=path.relative(runtime,path.resolve(runtime,entry.path)).split(path.sep).join('/');
 for(const name of ['initial_pipeline_cache.db','initial_pipeline_cache.json'])add(path.join(runtime,name),'runtime/'+name,runtime);
 const web=path.join(project,'web');
 for(const name of fs.readdirSync(web))if((name.endsWith('.mjs')&&!name.endsWith('.test.mjs'))||['index.html','favicon.svg','serve.cjs','runtime-package-routes.cjs','server-listen.cjs','room-service.cjs'].includes(name))add(path.join(web,name),'partyboard/web/'+name,web);
 for(const name of ['wifi-menu.opus','currently-spectating.opus'])add(path.join(web,'audio',name),'partyboard/web/audio/'+name,web);
 add(path.join(project,'tools/launch-local.ps1'),'partyboard/tools/launch-local.ps1',project);
 for(const name of ['automatic-hosting.cjs','host-game.cjs','ensure-hosting-tools.cjs','hosting-tools.json','hosting-process-job.ps1'])
  add(path.join(project,'tools',name),'partyboard/tools/'+name,project);
 const nodePin=require('./hosting-tools.json').node;
 add(process.execPath,'runtime/node.exe',path.dirname(process.execPath),nodePin.sha256);
 for(const name of ['cloudflared-LICENSE','caddy-LICENSE','hosting-node-LICENSE'])
  add(path.join(project,'licenses',name),'licenses/'+name,project);
 add(path.join(project,'LICENSE'),'licenses/project-LICENSE',project);
 add(path.join(project,'licenses/miniupnpc-LICENSE'),'licenses/miniupnpc-LICENSE',project);
 for(const dependency of ['aurora','libco','musyx'])
  add(path.join(project,'extern',dependency,'LICENSE'),'licenses/'+dependency+'-LICENSE',project);
 const noticeManifest=path.join(project,'licenses/dependency-notices.json');
 const notices=JSON.parse(fs.readFileSync(noticeManifest,'utf8'));
 if(notices.version!==1||!Array.isArray(notices.notices)||notices.notices.length<1||notices.notices.length>256)
  throw Error('Bounded dependency notice manifest required');
 const noticeNames=new Set([...plan.keys()].map(name=>name.toLowerCase()));
 for(const notice of notices.notices){
  if(typeof notice.file!=='string'||!/^[a-zA-Z0-9][a-zA-Z0-9_.-]{0,99}$/.test(notice.file)||typeof notice.sha256!=='string'||!/^[a-f0-9]{64}$/.test(notice.sha256))
   throw Error('Invalid dependency notice entry');
  const destination='licenses/'+notice.file;
  if(noticeNames.has(destination.toLowerCase()))throw Error('Duplicate package destination');
  noticeNames.add(destination.toLowerCase());
  add(path.join(project,'licenses',notice.file),destination,project,notice.sha256);
 }
 add(noticeManifest,'licenses/dependency-notices.json',project);
 const text=(name,value)=>plan.set(name,{bytes:Buffer.from(value)});
 text('runtime/native-session-package.json',JSON.stringify(descriptor,null,2)+'\n');
 text('native-runtime-path.txt','runtime\r\n');
 text('lobby-service-url.example.txt','https://YOUR-LOBBY-DOMAIN/\r\n');
 for(const [name,mode] of [['Play Mario Party 4.cmd','Native'],['Play with Keyboard.cmd','Keyboard'],['Browser Transfer.cmd','Browser']])text(name,
  '@echo off\r\npowershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0partyboard\\tools\\launch-local.ps1" -Mode '+mode+' %*\r\nset "RESULT=%ERRORLEVEL%"\r\nif not "%RESULT%"=="0" pause\r\nexit /b %RESULT%\r\n');
 text('READ ME.txt',[
  'Mario Party 4 local release candidate','',
  'Extract the entire folder before using the launchers. Windows is required for the native game.',
  'Before native play, install the latest Microsoft Visual C++ Redistributable (x64): https://aka.ms/vc14/vc_redist.x64.exe',
  'Play Mario Party 4.cmd: choose your own game image when prompted.',
  'Play with Keyboard.cmd: launch with keyboard controls.',
  'Browser Transfer.cmd: requires Node.js on PATH (or node.exe beside runtime/partyboard.exe). Open the printed URL in desktop Edge or Chrome.',
  'Automatic native Host uses the shared HTTPS website in lobby-service-url.txt beside these launchers. The example file is a template, not a working service.',
  'Without a saved website, native launch automatically tries a public-IP HTTPS address, then a free temporary tunnel. Initial setup downloads checksum-pinned Caddy and cloudflared tools from their official GitHub releases. No account or payment is needed.',
  'The hosting Node.js runtime is included. Use -LocalOnly to skip online startup. -LobbyWebsite https://your-domain/ selects an existing service for one native launch.',
  'Direct hosting needs reachable TCP ports 80/443 and a trusted certificate. Existing router mappings/firewall rules are preserved; blocked direct connections automatically use the outgoing-only fallback.',
  'The free fallback has no uptime guarantee; its address lasts while this game is open. It publishes only browser/runtime files and lobby signaling, never your selected game image. A public website does not by itself relay peer-to-peer gameplay.',
  'The browser server defaults to this computer only. For another device, supply -ListenAddress, -TlsCertificate and -TlsKey using a certificate that device trusts.',
  'For example: Browser Transfer.cmd -ListenAddress 0.0.0.0 -TlsCertificate C:\\certificates\\server.pem -TlsKey C:\\certificates\\server-key.pem',
  'Use the hostname covered by your certificate. Certificate files remain external to this package. Internet connectivity and real phones still need validation.',
  'No game image or saved personal settings are included. Keep all runtime and partyboard files together.',
  'Shader preparation includes discovered scenes only. Hardware, Internet reliability and full game coverage remain under validation.',
  'This folder is a local test candidate, not a completed or publicly released product.',
  'Third-party redistribution notices and clean-machine prerequisites still require a release audit.',''
 ].join('\r\n'));
 // Preflight finished before creating the destination. Preserve partial output on
 // I/O failure and mark it unusable, instead of recursively deleting any paths.
 fs.mkdirSync(target,{recursive:true});
 const incomplete=path.join(target,'PACKAGE-INCOMPLETE.txt');fs.writeFileSync(incomplete,'Packaging did not finish. Do not use this folder.\n',{flag:'wx'});
 const files=[];
 for(const [relative,input] of plan){
  const output=path.join(target,relative);fs.mkdirSync(path.dirname(output),{recursive:true});
  fs.writeFileSync(output,input.bytes,{flag:'wx'});
  const digest=sha(input.bytes);
  if(sha(fs.readFileSync(output))!==digest)throw Error('Copied package bytes differ: '+relative);
  files.push({path:relative,bytes:input.bytes.length,sha256:digest});
 }
 const installed=path.join(target,'runtime');
 runtimePackageRoutes(path.join(installed,'native-session-package.json'));
 if(fingerprint(installed,descriptor)!==buildId)throw Error('Staged runtime fingerprint differs');
 const report={version:1,kind:'local-release-candidate',buildId,artifacts:descriptor.files.length,files,
  copiedFilesVerified:true,gameImageIncluded:false,cleanMachineVerified:false,publicReleaseReady:false};
 fs.writeFileSync(path.join(target,'package-manifest.json'),JSON.stringify(report,null,2)+'\n',{flag:'wx'});
 fs.unlinkSync(incomplete);return report;
}
if(require.main===module){
 const args=process.argv.slice(2);
 if(args.length!==2)throw Error('Usage: node stage-local-package.cjs INSTALLED_RUNTIME FRESH_DESTINATION');
 const report=stageLocalPackage(...args);console.log(JSON.stringify({buildId:report.buildId,artifacts:report.artifacts,files:report.files.length,bytes:report.files.reduce((sum,file)=>sum+file.bytes,0),cleanMachineVerified:false,publicReleaseReady:false}));
}
module.exports={stageLocalPackage,fingerprint};
