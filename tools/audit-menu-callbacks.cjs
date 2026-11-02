'use strict';
// Audit compiled Wasm, so corrected C source cannot hide an older menu binary.
const fs=require('node:fs'),assert=require('node:assert/strict');
function auditMenuCallbacks(bytes){
 const module=new WebAssembly.Module(bytes),sections=new Map();
 function cursor(data,start=0){let p=start;return {u(){let v=0,s=0,c;do{c=data[p++];v|=(c&127)<<s;s+=7;}while(c&128);return v;},byte(){return data[p++];},str(){const n=this.u(),v=Buffer.from(data.subarray(p,p+n)).toString();p+=n;return v;},get p(){return p;},set p(v){p=v;}};}
 const c=cursor(bytes,8);
 while(c.p<bytes.length){const id=c.byte(),length=c.u(),start=c.p;sections.set(id,bytes.subarray(start,start+length));c.p=start+length;}
 const t=cursor(sections.get(1)),types=[];
 for(let n=t.u();n;n--){assert.equal(t.byte(),0x60);const params=Array.from({length:t.u()},()=>t.byte()),results=Array.from({length:t.u()},()=>t.byte());types.push({params,results});}
 const f=cursor(sections.get(3)),functions=Array.from({length:f.u()},()=>f.u());
 const imports=WebAssembly.Module.imports(module).filter(entry=>entry.kind==='function').length,names=new Map();
 for(const section of WebAssembly.Module.customSections(module,'name')){
  const data=new Uint8Array(section),s=cursor(data);
  while(s.p<data.length){const id=s.byte(),length=s.u(),end=s.p+length;if(id===1)for(let n=s.u();n;n--){const index=s.u();names.set(s.str(),index);}s.p=end;}
 }
 const checks=[];
 for(const name of ['PlayerCountUpdate','TeamSelectionUpdate','DifficultySelectionUpdate','CpuCharacterUpdate']){
  const index=names.get(name);assert.ok(index!==undefined,'Missing compiled typed callback: '+name);
  const type=types[functions[index-imports]];assert.deepEqual(type,{params:[0x7f,0x7f],results:[]},'Callback must accept exactly two pointers: '+name);
  checks.push({name,index,signature:'void(i32,i32)'});
 }
 return {status:'VERIFIED compiled callback signatures only',checks};
}
if(require.main===module){const [file,report]=process.argv.slice(2);const result=auditMenuCallbacks(fs.readFileSync(file));if(report)fs.writeFileSync(report,JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));}
module.exports={auditMenuCallbacks};
