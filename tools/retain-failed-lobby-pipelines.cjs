'use strict';
const fs=require('node:fs');
const path=require('node:path');
const crypto=require('node:crypto');
// Run after failure stacks, diagnostics and pixels have been retained.
// A stalled engine cannot supply a closed cache; preserve that failure too.
async function retainFailedLobbyPipelines(pages,directory){
  const results=await Promise.all(pages.map(async(page,peer)=>{
    let timer;
    try{
      const captured=await Promise.race([page.evaluate(async()=>{
        document.querySelector('#stop')?.click();
        for(let attempt=0;attempt<40;attempt++){
          const seed=globalThis.partyBoardApp?.lastCapturedLobbyPipelineSeed();
          if(seed)return {scope:seed.scope,sha256:seed.sha256,
            bytes:Array.from(new Uint8Array(await seed.blob.arrayBuffer()))};
          await new Promise(resolve=>setTimeout(resolve,250));
        }
        throw Error('Cooperative shutdown did not produce a closed pipeline cache.');
      }),new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error('Closed pipeline capture timed out.')),15000);})]);
      const bytes=Buffer.from(captured.bytes);
      if(captured.scope!=='observed-scenes-only'||bytes.length<100||bytes.length>32*1024**2
          ||!bytes.subarray(0,16).equals(Buffer.from('SQLite format 3\0'))
          ||crypto.createHash('sha256').update(bytes).digest('hex')!==captured.sha256)
        throw Error('Closed pipeline cache integrity or scope rejected.');
      const filename='browser-failed-closed-pipelines-peer'+peer+'.db';
      fs.writeFileSync(path.join(directory,filename),bytes,{flag:'wx'});
      return {peer,result:'captured',filename,sha256:captured.sha256,bytes:bytes.length,
        scope:captured.scope,fullCoverage:false,matchCompleted:false};
    }catch(error){return {peer,result:'unavailable',error:String(error)};}
    finally{clearTimeout(timer);}
  }));
  fs.writeFileSync(path.join(directory,'browser-failed-closed-pipelines.json'),JSON.stringify(results,null,2)+'\n');
  return results;
}
module.exports={retainFailedLobbyPipelines};
