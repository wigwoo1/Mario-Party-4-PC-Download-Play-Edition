'use strict';
// Local distribution metadata. No disc, credentials or peer messages are copied.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const [source,target]=process.argv.slice(2);
if(!source||!target||process.argv.length!==4)throw Error('Usage: node write-native-package.cjs source-descriptor output-descriptor');
const sourcePath=path.resolve(source),targetPath=path.resolve(target);
if(fs.existsSync(targetPath))throw Error('Output descriptor already exists');
const descriptor=JSON.parse(fs.readFileSync(sourcePath,'utf8'));
if(!Array.isArray(descriptor.files)||!descriptor.files.length||descriptor.files.length>256)throw Error('Bounded runtime artifact list required');
const names=new Set(),files=[];
for(const entry of descriptor.files){
 if(typeof entry.name!=='string'||!/^([a-zA-Z0-9_.-]+\/)*[a-zA-Z0-9_.-]+$/.test(entry.name)||entry.name.split('/').some(part=>part==='.'||part==='..')||names.has(entry.name))throw Error('Invalid artifact name');
 names.add(entry.name);
 if(typeof entry.path!=='string'||!entry.path)throw Error('Artifact path required');
 const filename=path.resolve(path.dirname(sourcePath),entry.path),before=fs.statSync(filename);
 if(!before.isFile()||before.size<=0||before.size>1024*1024*1024)throw Error('Bounded artifact required');
 const hash=crypto.createHash('sha256'),buffer=Buffer.alloc(65536),fd=fs.openSync(filename,'r');let count,total=0;
 try{while((count=fs.readSync(fd,buffer,0,buffer.length,null))>0){hash.update(buffer.subarray(0,count));total+=count;}}finally{fs.closeSync(fd);}
 const after=fs.statSync(filename);
 if(total!==before.size||after.size!==before.size||after.mtimeMs!==before.mtimeMs)throw Error('Artifact changed during measurement');
 files.push({name:entry.name,size:total,sha256:hash.digest('hex')});
 entry.path=path.relative(path.dirname(targetPath),filename);
}
files.sort((a,b)=>a.name<b.name?-1:a.name>b.name?1:0);
const buildId=crypto.createHash('sha256').update(JSON.stringify({version:1,profile:'unlocked-no-save-v1',files})).digest('hex');
if(!descriptor.configuration)throw Error('Source configuration required');
descriptor.configuration.buildId=buildId;
fs.writeFileSync(targetPath,JSON.stringify(descriptor,null,2)+'\n',{flag:'wx'});
console.log(JSON.stringify({buildId,artifacts:files.length,descriptor:targetPath}));