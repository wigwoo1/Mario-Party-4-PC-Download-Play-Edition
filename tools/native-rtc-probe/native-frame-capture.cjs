'use strict';
const fs=require('node:fs');
const assert=require('node:assert/strict');
function inspectPpm(bytes){
 const header=/^P6\n(\d+) (\d+)\n255\n/.exec(bytes.subarray(0,80).toString('ascii'));
 assert.ok(header,'Native capture must be a binary RGB PPM');
 const width=Number(header[1]),height=Number(header[2]);
 assert.ok(width>0&&height>0&&width<=16384&&height<=16384,'Invalid capture dimensions');
 assert.equal(bytes.length,header[0].length+width*height*3,'Incomplete native capture');
 return {width,height,bytes:bytes.length};
}
async function requestNativeFrame(request,timeout=15000){
 const output=request+'.ppm';
 assert.ok(!fs.existsSync(request)&&!fs.existsSync(output),'Capture paths must be fresh');
 fs.writeFileSync(request,'capture\n',{flag:'wx'});
 const deadline=Date.now()+timeout;
 while(Date.now()<deadline){
  if(fs.existsSync(output)){
   // Mapping writes asynchronously; a partial file is not a finished capture.
   try{return {path:output,...inspectPpm(fs.readFileSync(output))};}catch{}
  }
  await new Promise(resolve=>setTimeout(resolve,100));
 }
 throw Error('Native rendered frame capture did not complete: '+request);
}
module.exports={inspectPpm,requestNativeFrame};
