'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto'),{promisify}=require('node:util'),{execFile}=require('node:child_process');
const {requestNativeFrame}=require('./native-frame-capture.cjs');
const run=promisify(execFile),pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));
function createNativePngCapture({request,python,requestFrame=requestNativeFrame}){
 assert.ok(typeof request==='string'&&path.isAbsolute(request),'Owned absolute native request path required');
 assert.ok(typeof python==='string'&&path.isAbsolute(python)&&fs.statSync(python).isFile(),'Explicit Python/Pillow runtime required');
 let active=false;
 return async filename=>{
  assert.ok(!active,'Native captures must not overlap');
  assert.ok(path.isAbsolute(filename)&&filename.endsWith('.png'),'Absolute PNG destination required');
  const raw=filename.slice(0,-4)+'.ppm';assert.ok(!fs.existsSync(filename)&&!fs.existsSync(raw),'Capture destinations must be fresh');
  active=true;
  try{
   const frame=await requestFrame(request,15000);
   for(let attempt=0;;attempt++)try{fs.renameSync(frame.path,raw);break;}catch(error){if(attempt>=20)throw error;await pause(10);}
   // Pillow verifies every RGB pixel survives conversion. The source is the
   // renderer's resolved game image, never the desktop or another window.
   await run(python,['-c','from PIL import Image; import sys; image=Image.open(sys.argv[1]); assert image.format=="PPM"; target=open(sys.argv[2],"xb"); image.save(target,format="PNG"); target.close(); assert Image.open(sys.argv[2]).convert("RGB").tobytes()==image.convert("RGB").tobytes()',raw,filename],{windowsHide:true,timeout:15000});
   return {path:filename,rawPath:raw,width:frame.width,height:frame.height,pixelsUnchanged:true,sha256:crypto.createHash('sha256').update(fs.readFileSync(filename)).digest('hex')};
  }finally{active=false;}
 };
}
module.exports={createNativePngCapture};
