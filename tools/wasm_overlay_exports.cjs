// Retain engine definitions needed by a runtime-loaded side module without
// making that side module a resident startup dependency.
const fs=require('node:fs');
const moduleInfo=new WebAssembly.Module(fs.readFileSync(process.argv[2]));
const names=[...new Set(WebAssembly.Module.imports(moduleInfo)
  .filter(item=>item.kind==='function'||item.module==='GOT.mem'||item.module==='GOT.func')
  .map(item=>item.name))];
if(names.some(name=>!/^[A-Za-z0-9_.$]+$/.test(name)))throw Error('Unsupported overlay import name');
process.stdout.write(names.map(name=>'-Wl,--export-if-defined='+name).join(';'));
