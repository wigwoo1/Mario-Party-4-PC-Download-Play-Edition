'use strict';
// Stage a measured local runtime for normal native Host/Join discovery.
// Existing native build files remain authoritative; never overwrite artifacts.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),{spawnSync}=require('node:child_process');
const [source,target]=process.argv.slice(2);
if(!source||!target||process.argv.length!==4)throw Error('Usage: node install-native-runtime.cjs SOURCE_DESCRIPTOR NATIVE_DIRECTORY');
const sourcePath=path.resolve(source),root=path.resolve(target),descriptor=JSON.parse(fs.readFileSync(sourcePath,'utf8'));
if(!fs.statSync(root).isDirectory()||!fs.statSync(path.join(root,'partyboard.exe')).isFile())throw Error('Existing native executable directory required');
if(!Array.isArray(descriptor.files)||descriptor.files.length<1||descriptor.files.length>256)throw Error('Bounded runtime artifact list required');
const output=path.join(root,'native-session-package.json');
if(fs.existsSync(output))throw Error('Default descriptor already exists; inspect it before replacing');
const digest=filename=>crypto.createHash('sha256').update(fs.readFileSync(filename)).digest('hex');
const destinations=new Set(),plans=[];
for(const entry of descriptor.files){
 if(typeof entry.name!=='string'||typeof entry.path!=='string'||!entry.path)throw Error('Named local artifact path required');
 let relative;
 if(entry.name.startsWith('native/res/')){
  const encoded=entry.name.slice('native/res/'.length);
  if(!/^(?:[a-f0-9]{2})+$/.test(encoded))throw Error('Canonical encoded resource name required');
  const decoded=Buffer.from(encoded,'hex').toString('utf8');
  if(Buffer.from(decoded).toString('hex')!==encoded)throw Error('Invalid UTF-8 resource name');
  relative='res/'+decoded;
 }else if(entry.name.startsWith('native/'))relative=entry.name.slice(7);
 else if(['contract.mjs','contract.wasm'].includes(entry.name))relative='browser-runtime/'+entry.name;
 else throw Error('Unsupported distribution artifact: '+entry.name);
 if(!/^[A-Za-z0-9 _.-]+(?:\/[A-Za-z0-9 _.-]+)*$/.test(relative)||relative.split('/').some(part=>part==='.'||part==='..'||part!==part.trim()||part.endsWith('.')))throw Error('Safe relative distribution path required: '+relative);
 const destination=path.resolve(root,relative),folded=destination.toLowerCase();
 if(!folded.startsWith(root.toLowerCase()+path.sep)||destinations.has(folded))throw Error('Artifact escapes bundle or collides');
 destinations.add(folded);
 const input=path.resolve(path.dirname(sourcePath),entry.path),stat=fs.statSync(input);
 if(!stat.isFile()||stat.size<1||stat.size>1024**3)throw Error('Bounded source artifact required');
 const exists=fs.existsSync(destination);
 if(exists&&!fs.statSync(destination).isFile())throw Error('Distribution destination is not a file');
 // A browser pair is supplied as a unit by this package, never silently mixed.
 if(exists&&!entry.name.startsWith('native/')&&digest(input)!==digest(destination))throw Error('Different browser artifact already installed');
 plans.push({entry,input,destination,exists});
}
const staged=[];
for(const plan of plans){
 if(!plan.exists){fs.mkdirSync(path.dirname(plan.destination),{recursive:true});fs.copyFileSync(plan.input,plan.destination,fs.constants.COPYFILE_EXCL);staged.push(path.relative(root,plan.destination));}
 plan.entry.path=path.relative(root,plan.destination);
}
const prepared=path.join(root,'.native-session-package-prepared.json');
fs.writeFileSync(prepared,JSON.stringify(descriptor,null,2)+'\n',{flag:'wx'});
try{
 const result=spawnSync(process.execPath,[path.join(__dirname,'write-native-package.cjs'),prepared,output],{encoding:'utf8'});
 if(result.status!==0)throw Error(result.stderr||result.stdout||'Runtime measurement failed');
 const measurement=JSON.parse(result.stdout);
 console.log(JSON.stringify({...measurement,stagedFiles:staged,existingNativeFilesPreserved:true,gameAssetCopied:false}));
}finally{fs.unlinkSync(prepared);}
