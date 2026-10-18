'use strict';
const fs=require('node:fs');
// Keep this optional control endpoint in private test work, never web assets.
function retainBrowserInspection(filename,endpoint){
 if(!filename)return ()=>{};
 const url=new URL(endpoint);
 if(url.protocol!=='ws:'||!['127.0.0.1','localhost','[::1]'].includes(url.hostname))throw Error('Local browser inspection endpoint required.');
 const contents=JSON.stringify({endpoint,pid:process.pid})+'\n';
 fs.writeFileSync(filename,contents,{flag:'wx',mode:0o600});
 return ()=>{
  try{if(fs.readFileSync(filename,'utf8')===contents)fs.unlinkSync(filename);}
  catch(error){if(error.code!=='ENOENT')throw error;}
 };
}
async function retainBrowserServerInspection(filename,server){
 if(!filename)return ()=>{};
 const argument=server.process().spawnargs.find(value=>value.startsWith('--user-data-dir='));
 if(!argument)throw Error('Browser profile directory required for DevTools inspection.');
 const active=require('node:path').join(argument.slice('--user-data-dir='.length),'DevToolsActivePort');
 for(let attempt=0;attempt<100;attempt++){
  try{
   const [port,route]=fs.readFileSync(active,'utf8').trim().split(/\r?\n/);
   if(!/^\d+$/.test(port)||Number(port)<1||Number(port)>65535||!route?.startsWith('/devtools/browser/'))throw Error('Invalid local DevTools endpoint.');
   return retainBrowserInspection(filename,'ws://127.0.0.1:'+port+route);
  }catch(error){if(error.code!=='ENOENT')throw error;}
  await new Promise(resolve=>setTimeout(resolve,50));
 }
 throw Error('Browser DevTools endpoint did not become available.');
}
module.exports={retainBrowserInspection,retainBrowserServerInspection};
