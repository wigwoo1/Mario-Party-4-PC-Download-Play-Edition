'use strict';
// Export only project source and pinned dependency source. Never traverse the
// development workspace, installed runtime, original discs or user profiles.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),{spawnSync}=require('node:child_process');
const sha=bytes=>crypto.createHash('sha256').update(bytes).digest('hex');
const folders=['assets','cmake','config','include','libc','libs','licenses','patches','platforms','res','src','tools','web'];
const topFiles=['.clangd','.flake8','.gitattributes','CMakeLists.txt','CMakePresets.json','cmake-variants.yaml','configure','configure.py','dol.def','files.cmake','ios.toolchain.cmake','LICENSE','README.md','building.md','version.h.in','CONTROLLERS.md','NETPLAY.md','REPLAY.md','HOSTING-STARTUP-PLAN.md'];
const blockedPart=/(^|\/)(?:\.git|\.codex|\.agents|node_modules|__pycache__|work|build|_deps|\.cache|profiles?)(\/|$)/i;
const blockedFile=/(?:\.(?:iso|ciso|gcm|wbfs|rvz|nkit|dol|rel|elf|o|obj|a|lib|dll|exe|pdb|ilk|exp|pem|key|pfx|p12|zip|7z|tar|gz|wasm|map)|(?:^|\/)\.env(?:\.[^/]*)?)$/i;
function safeRelative(relative){return typeof relative==='string'&&relative.length>0&&!/^(?:[A-Za-z]:|[\\/])/.test(relative)&&!path.isAbsolute(relative)&&!relative.split(/[\\/]/).some(p=>p==='..'||p==='.')&&!relative.includes('\0');}
function allowed(relative){return safeRelative(relative)&&!blockedPart.test(relative)&&!blockedFile.test(relative);}
function secretMatches(bytes){if(bytes.includes(0))return false;return /(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}|AKIA[A-Z0-9]{16}|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----)/.test(bytes.toString('utf8'));}
function git(cwd,args){const r=spawnSync('git',args,{cwd,encoding:'utf8',windowsHide:true,maxBuffer:16*1024*1024});if(r.status!==0)throw Error('Source inventory failed; git exit '+r.status);return r.stdout;}
function preparePublication(projectDirectory,destination,buildId,statusDocument){
 if(!/^[a-f0-9]{64}$/.test(buildId))throw Error('Measured build identity required');
 const project=fs.realpathSync(projectDirectory),target=path.resolve(destination);
 const relationship=path.relative(project,target);if(!relationship.startsWith('..')&&!path.isAbsolute(relationship))throw Error('Publication destination must be outside the source project');
 if(!fs.existsSync(path.join(target,'.git')))throw Error('Use a dedicated cloned publication repository');
 const previousFile=path.join(target,'PUBLICATION-MANIFEST.json'),previous=fs.existsSync(previousFile)?JSON.parse(fs.readFileSync(previousFile,'utf8')):null;
 const planned=new Map(),dependencies=[];
 function collect(root,prefix,selections){
  const names=git(root,['ls-files','-z','--cached','--others','--exclude-standard','--',...selections]).split('\0').filter(Boolean);
  for(const name of names){const relative=prefix+name;if(!allowed(relative))continue;
   const from=path.resolve(root,name);if(!fs.existsSync(from)||!fs.lstatSync(from).isFile())continue;
   const inside=path.relative(root,fs.realpathSync(from));if(inside.startsWith('..')||path.isAbsolute(inside))throw Error('Source file escaped its root');
   const bytes=fs.readFileSync(from);if(bytes.length>50*1024*1024)throw Error('Unexpected large source file: '+relative);if(secretMatches(bytes))throw Error('Potential credential in source file: '+relative);
   planned.set(relative,bytes);
  }
 }
 collect(project,'',[...folders,...topFiles]);
 for(const name of ['aurora','musyx','libco']){
  const directory=path.join(project,'extern',name);collect(directory,'extern/'+name+'/', ['.']);
  dependencies.push({name,upstream:git(directory,['remote','get-url','origin']).trim(),baseCommit:git(directory,['rev-parse','HEAD']).trim(),includesWorkingChanges:true});
 }
 const upstreamReadme=planned.get('README.md');if(upstreamReadme)planned.set('UPSTREAM-README.md',upstreamReadme);
 const day=new Date().toISOString().slice(0,10),repo='https://github.com/wigwoo1/Mario-Party-4-PC-Download-Play-Edition';
 const status=statusDocument||`# Development status\n\nUpdated ${day} (UTC). This project is incomplete.\n\nCurrent measured Windows/browser build: \`${buildId}\`.\n\nThis is a development snapshot. See its release notes for build-specific validation and open failures. No new gameplay, Internet or clean-machine acceptance is implied by a source publication.\n`;
 if(!status.includes(buildId)||secretMatches(Buffer.from(status)))throw Error('Public status must identify the measured build and contain no credentials');
 planned.set('STATUS.md',Buffer.from(status));
 planned.set('README.md',Buffer.from(`# Mario Party 4 PC Download & Play Edition\n\nDevelopment native Windows and browser port based on [Party Board](https://github.com/mariopartyrd/partyboard). **Work in progress; development builds may crash or fail to connect.**\n\n## Downloads\n\nGet the current Windows/browser development archive from [Releases](${repo}/releases). Extract the complete archive before launching. A game disc image is not included; the host selects their own compatible Mario Party 4 image. Browser guests receive the required data from the host.\n\nThe native application requires Windows x64 and the [Microsoft Visual C++ Redistributable](https://aka.ms/vc14/vc_redist.x64.exe). The archive includes the browser runtime and its local server. Read its bundled instructions before hosting; public Internet hosting and mobile compatibility remain under test.\n\n## Current state\n\nSee [STATUS.md](STATUS.md) for build identity, verified behavior and open failures. Releases are marked prerelease until the remaining acceptance checks pass.\n\n## Source and building\n\nThis repository contains current project source plus snapshots of the modified Aurora, MusyX and libco dependencies. Their origins and base commits are recorded in [PUBLICATION-MANIFEST.json](PUBLICATION-MANIFEST.json); their licenses remain included. These snapshots are ordinary source folders, so submodule initialization is not required.\n\nStart with [building.md](building.md), [CONTROLLERS.md](CONTROLLERS.md), [NETPLAY.md](NETPLAY.md) and [REPLAY.md](REPLAY.md). Browser build tools are in tools/browser-renderer and tools/link_wasm_overlays.py. Native and browser builds need their documented SDK/toolchain dependencies. The published binary snapshot is not a claim that every platform can currently be rebuilt on a fresh machine.\n\n## Attribution\n\nOriginal project and contributor credits are preserved in [UPSTREAM-README.md](UPSTREAM-README.md). The project license and third-party notices are retained in LICENSE, licenses and dependency source directories. Mario Party 4 and its characters belong to their respective owners.\n`));
 planned.set('.gitignore',Buffer.from('build/\nwork/\n_deps/\nnode_modules/\n__pycache__/\n*.iso\n*.ciso\n*.gcm\n*.rvz\n*.pdb\n*.dll\n*.exe\n*.wasm\n*.zip\n*.pem\n*.key\n.env\n.env.*\n'));
 // Refuse to overwrite an independently edited managed file on daily exports.
 if(previous)for(const entry of previous.files){if(!safeRelative(entry.path))throw Error('Invalid previous manifest path');const file=path.join(target,entry.path);if(fs.existsSync(file)&&sha(fs.readFileSync(file))!==entry.sha256)throw Error('Publication file changed independently: '+entry.path);}
 for(const [relative,bytes]of planned){const file=path.join(target,relative);fs.mkdirSync(path.dirname(file),{recursive:true});fs.writeFileSync(file,bytes);}
 if(previous)for(const entry of previous.files)if(!planned.has(entry.path)){const file=path.join(target,entry.path);if(fs.existsSync(file))fs.unlinkSync(file);}
 const files=[...planned].map(([name,bytes])=>({path:name,bytes:bytes.length,sha256:sha(bytes)})).sort((a,b)=>a.path.localeCompare(b.path));
 const report={version:1,repository:repo,buildId,sourceSnapshotSha256:sha(JSON.stringify(files)),upstream:{url:git(project,['remote','get-url','origin']).trim(),baseCommit:git(project,['rev-parse','HEAD']).trim()},dependencies,files,excluded:['game disc images','personal profiles','private keys and credentials','local test logs/screenshots','development work/build directories','Git history and local configuration'],reproducibleCleanMachineBuildVerified:false};
 fs.writeFileSync(previousFile,JSON.stringify(report,null,2)+'\n');
 for(const entry of files)if(sha(fs.readFileSync(path.join(target,entry.path)))!==entry.sha256)throw Error('Export verification failed');
 return report;
}
if(require.main===module){const [project,target,buildId,statusFile]=process.argv.slice(2),report=preparePublication(project,target,buildId,statusFile?fs.readFileSync(statusFile,'utf8'):undefined);console.log(JSON.stringify({files:report.files.length,bytes:report.files.reduce((n,f)=>n+f.bytes,0),sourceSnapshotSha256:report.sourceSnapshotSha256,buildId:report.buildId}));}
module.exports={allowed,safeRelative,secretMatches,preparePublication};
