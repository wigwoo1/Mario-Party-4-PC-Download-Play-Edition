'use strict';
async function browserStorageObservation(page,{files=false}={}){
 const api=await page.evaluate(async files=>{
  const estimate=await navigator.storage.estimate(),result={estimate,status:document.querySelector('#status')?.textContent};
  if(files&&typeof navigator.storage.getDirectory==='function'){
   const directory=await navigator.storage.getDirectory();result.temporaryFiles=[];
   for await(const [name,handle]of directory.entries())if(name.startsWith('partyboard-runtime-')&&handle.kind==='file')result.temporaryFiles.push({bytes:(await handle.getFile()).size});
  }
  return result;
 },files);
 let protocol;
 const session=await page.context().newCDPSession(page);
 try{protocol=await session.send('Storage.getUsageAndQuota',{origin:new URL(page.url()).origin});}finally{await session.detach();}
 return {...api,protocol};
}
module.exports={browserStorageObservation};
