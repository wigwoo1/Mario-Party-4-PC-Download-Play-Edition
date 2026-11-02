'use strict';
const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),os=require('node:os'),path=require('node:path'),{execFileSync}=require('node:child_process');
const {createNativePngCapture}=require('./native-png-capture.cjs'),{inspectPpm}=require('./native-frame-capture.cjs');
const python=process.env.PARTYBOARD_TEST_PYTHON;
function fixture(){const directory=fs.mkdtempSync(path.join(os.tmpdir(),'partyboard-native-png-')),request=path.join(directory,'render.request');let calls=0;
 return {directory,request,get calls(){return calls;},requestFrame:async()=>{calls++;const file=request+'.ppm';fs.writeFileSync(file,Buffer.concat([Buffer.from('P6\n2 1\n255\n'),Buffer.from([255,0,0,0,128,255])]),{flag:'wx'});return {path:file,...inspectPpm(fs.readFileSync(file))};},cleanup(){assert.equal(path.dirname(directory),os.tmpdir());assert.ok(path.basename(directory).startsWith('partyboard-native-png-'));fs.rmSync(directory,{recursive:true,force:true});}};
}
test('native resolved-image captures preserve actual RGB pixels and free the single request output for reuse',async()=>{
 assert.ok(python,'Set explicit PARTYBOARD_TEST_PYTHON with Pillow');const f=fixture();try{
  const capture=createNativePngCapture({request:f.request,python,requestFrame:f.requestFrame});
  for(const name of ['rankings','menu']){const file=path.join(f.directory,name+'.png'),result=await capture(file);assert.equal(result.pixelsUnchanged,true);assert.equal(result.width,2);assert.equal(result.height,1);assert.ok(!fs.existsSync(f.request+'.ppm'));const pixels=execFileSync(python,['-c','from PIL import Image;import sys;print(Image.open(sys.argv[1]).convert("RGB").tobytes().hex())',file],{encoding:'utf8',windowsHide:true}).trim();assert.equal(pixels,'ff00000080ff');}
  assert.equal(f.calls,2);
 }finally{f.cleanup();}
});
test('native capture refuses to overwrite an existing image and rejects overlapping requests',async()=>{
 assert.ok(python);const f=fixture();try{
  let release;const pending=new Promise(resolve=>{release=resolve;});const capture=createNativePngCapture({request:f.request,python,requestFrame:async()=>{await pending;return f.requestFrame();}});
  const file=path.join(f.directory,'image.png'),first=capture(file);await assert.rejects(capture(path.join(f.directory,'other.png')),/must not overlap/);release();await first;
  const before=fs.readFileSync(file);await assert.rejects(capture(file),/must be fresh/);assert.deepEqual(fs.readFileSync(file),before);
 }finally{f.cleanup();}
});
