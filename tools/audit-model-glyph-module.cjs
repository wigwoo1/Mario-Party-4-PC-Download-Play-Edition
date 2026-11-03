'use strict';
// Execute only the real compiled m418 label-routing prefix, stopping at its
// glyph dependency. This verifies ownership/labels, not model rendering.
const fs=require('node:fs'),assert=require('node:assert/strict'),crypto=require('node:crypto');
function observeModelGlyphRouting(bytes){
 const module=new WebAssembly.Module(bytes),cases=[];
 for(const label of [0,1,2,3])for(const owner of [-1,0,1,2,3]){
  const memory=new WebAssembly.Memory({initial:256}),imports={env:{},'GOT.mem':{},'GOT.func':{}},base=65536,calls=[];let address=0x400000;
  for(const entry of WebAssembly.Module.imports(module)){
   assert.ok(Object.hasOwn(imports,entry.module),'Unexpected namespace');let value;
   if(entry.kind==='memory')value=memory;
   else if(entry.kind==='table')value=new WebAssembly.Table({initial:4096,element:'anyfunc'});
   else if(entry.kind==='global'){const initial=entry.name==='__stack_pointer'?0xe00000:entry.name==='__memory_base'?base:entry.name==='__table_base'?1:entry.module.startsWith('GOT')?(address+=65536):0;value=new WebAssembly.Global({value:'i32',mutable:!['__memory_base','__table_base'].includes(entry.name)},initial);}
   else if(entry.kind==='function')value=entry.name==='Hu3DModelControlGlyphSet'?(model,name,entry,port)=>{const b=new Uint8Array(memory.buffer);let end=name;while(end<name+32&&b[end])end++;calls.push({model,bitmap:Buffer.from(b.subarray(name,end)).toString(),entry,port});throw Error('captured-glyph-routing');}:()=>{throw Error('unexpected-dependency:'+entry.name);};
   else throw Error('Unexpected import kind');imports[entry.module][entry.name]=value;
  }
  const instance=new WebAssembly.Instance(module,imports);instance.exports.__wasm_apply_data_relocs();assert.equal(typeof instance.exports.fn_1_E74,'function');const view=new DataView(memory.buffer),object=0x300000,models=0x301000;
  view.setUint32(object+0x40,models,true);view.setInt16(models+2,73,true);view.setUint32(object+0x4c,label,true);
  view.setInt32(instance.exports.lbl_1_bss_3EC.value+base,0,true);const player=instance.exports.lbl_1_bss_26C.value+base;view.setInt32(player+0xc,owner<0?1:0,true);view.setInt32(player+0x18,owner<0?3:owner,true);
  let stoppedAt;try{instance.exports.fn_1_E74(object);}catch(error){stoppedAt=error.message;}
  cases.push({label,owner,calls,stoppedAt});
 }
 return {sha256:crypto.createHash('sha256').update(bytes).digest('hex'),cases,scope:'Real compiled m418 label/ownership prefix only; no renderer/gameplay execution'};
}
function auditModelGlyphRouting(bytes){const result=observeModelGlyphRouting(bytes);for(const c of result.cases){assert.equal(c.stoppedAt,'captured-glyph-routing','Compiled m418 must call model glyph routing');assert.deepEqual(c.calls,[{model:73,bitmap:['s3B','s3A','s3Y','s3X'][c.label],entry:[3,2,5,4][c.label],port:c.owner}]);}return result;}
if(require.main===module)console.log(JSON.stringify(auditModelGlyphRouting(fs.readFileSync(process.argv[2]))));
module.exports={observeModelGlyphRouting,auditModelGlyphRouting};
