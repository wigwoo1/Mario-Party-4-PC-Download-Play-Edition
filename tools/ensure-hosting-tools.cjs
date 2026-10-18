'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),{spawn}=require('node:child_process');
const pins=require('./hosting-tools.json');
const sha=bytes=>crypto.createHash('sha256').update(bytes).digest('hex');
async function downloadPinned(pin,destination,signal){
 if(fs.existsSync(destination)){
  const bytes=fs.readFileSync(destination);
  if(bytes.length===pin.size&&sha(bytes)===pin.sha256)return;
  throw Error('Cached hosting download failed its integrity check: '+path.basename(destination));
 }
 const response=await fetch(pin.url,{signal:AbortSignal.any([AbortSignal.timeout(120000),...(signal?[signal]:[])])});
 if(!response.ok)throw Error('Hosting tool download failed: HTTP '+response.status);
 const chunks=[];let size=0;
 for await(const chunk of response.body){size+=chunk.length;if(size>pin.size)throw Error('Hosting tool exceeded its pinned download size.');chunks.push(chunk);}
 const bytes=Buffer.concat(chunks);
 if(size!==pin.size||sha(bytes)!==pin.sha256)throw Error('Hosting tool download did not match the publisher checksum.');
 const temporary=destination+'.'+crypto.randomBytes(8).toString('hex')+'.tmp';
 fs.writeFileSync(temporary,bytes,{flag:'wx'});
 try{fs.renameSync(temporary,destination);}finally{if(fs.existsSync(temporary))fs.unlinkSync(temporary);}
}
async function ensureHostingTool(name,directory,signal){
 if(process.platform!=='win32'||process.arch!=='x64')throw Error('Automatic hosting tools currently require 64-bit Windows.');
 const pin=pins[name];if(!['caddy','cloudflared'].includes(name))throw Error('Unknown hosting tool.');
 fs.mkdirSync(directory,{recursive:true});
 if(name==='cloudflared'){
  const filename=path.join(directory,'cloudflared-'+pin.version+'.exe');await downloadPinned(pin,filename,signal);return filename;
 }
 const executable=path.join(directory,'caddy-'+pin.version+'.exe');
 if(fs.existsSync(executable)){
  if(sha(fs.readFileSync(executable))!==pin.executableSha256)throw Error('Cached Caddy executable failed its integrity check.');
  return executable;
 }
 const archive=path.join(directory,'caddy-'+pin.version+'.zip');await downloadPinned(pin,archive,signal);
 const extraction=fs.mkdtempSync(path.join(directory,'caddy-extract-'));
 // Expand only a pinned publisher archive. Pass paths as environment values,
 // never interpolate caller-controlled paths into PowerShell source.
 try{await new Promise((resolve,reject)=>{
  const child=spawn('powershell.exe',['-NoLogo','-NoProfile','-NonInteractive','-Command',
   "Expand-Archive -LiteralPath $env:PARTYBOARD_TOOL_ARCHIVE -DestinationPath $env:PARTYBOARD_TOOL_EXTRACTION -ErrorAction Stop"],
   {windowsHide:true,stdio:'ignore',env:{...process.env,PARTYBOARD_TOOL_ARCHIVE:archive,PARTYBOARD_TOOL_EXTRACTION:extraction}});
  const timer=setTimeout(()=>{child.kill();reject(Error('Hosting tool extraction timed out.'));},30000);
  child.once('error',error=>{clearTimeout(timer);reject(error);});child.once('exit',code=>{clearTimeout(timer);code===0?resolve():reject(Error('Hosting tool extraction failed.'));});
 });
 const bytes=fs.readFileSync(path.join(extraction,'caddy.exe'));
 if(sha(bytes)!==pin.executableSha256)throw Error('Extracted Caddy executable failed its integrity check.');
 fs.writeFileSync(executable,bytes,{flag:'wx'});return executable;
 }finally{
  const relative=path.relative(path.resolve(directory),path.resolve(extraction));
  if(!relative||relative.startsWith('..')||path.isAbsolute(relative))throw Error('Unsafe hosting extraction cleanup path.');
  fs.rmSync(extraction,{recursive:true,force:true});
 }
}
module.exports={ensureHostingTool,downloadPinned};
