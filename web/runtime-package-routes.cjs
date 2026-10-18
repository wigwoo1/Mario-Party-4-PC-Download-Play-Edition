'use strict';
const fs=require('node:fs'),path=require('node:path');
// Only a locally installed bundle descriptor may add HTTP routes. Peer messages
// never supply paths; the descriptor itself and game assets get no route.
function runtimePackageRoutes(filename){
 const descriptorPath=fs.realpathSync(filename),root=path.dirname(descriptorPath);
 if(fs.statSync(descriptorPath).size>256*1024)throw Error('Bounded runtime descriptor required');
 const descriptor=JSON.parse(fs.readFileSync(descriptorPath,'utf8'));
 if(!Array.isArray(descriptor.files)||descriptor.files.length<2||descriptor.files.length>256||
    !/^[a-f0-9]{64}$/.test(descriptor.configuration?.buildId)||descriptor.configuration?.profile!=='unlocked-no-save-v1')throw Error('Measured runtime package required');
 const routes=new Map(),names=new Set(),supplemental=[];
 for(const entry of descriptor.files){
  const name=entry.name;
  if(typeof name!=='string'||!/^\w[\w.-]*(?:\/[\w.-]+)*$/.test(name)||name.split('/').some(part=>part==='.'||part==='..')||names.has(name.toLowerCase()))throw Error('Unique canonical artifact name required');
  names.add(name.toLowerCase());
  let extension,expectedRelative;
  if(name==='contract.mjs'||name==='contract.wasm'){extension=path.extname(name);expectedRelative=path.join('browser-runtime',name);}
  else if(/^native\/res\/(?:[a-f0-9]{2})+$/.test(name)){
   const encoded=name.slice(11),resource=Buffer.from(encoded,'hex').toString('utf8');
   if(Buffer.from(resource).toString('hex')!==encoded||resource.split('/').some(part=>part==='.'||part==='..'))throw Error('Canonical resource name required');
   extension=path.extname(resource).toLowerCase();
   if(!['.ttf','.otf','.rgba8','.png','.svg','.json','.rml','.rcss'].includes(extension))throw Error('Unsupported runtime resource');
   expectedRelative=path.join('res',resource);
  }else if(/^native\/(?:[\w.-]+\/)*[\w.-]+\.(?:dll|exe|opus|ogg)$/i.test(name)){extension=path.extname(name).toLowerCase();expectedRelative=path.normalize(name.slice(7));}
  else throw Error('Unsupported runtime artifact route: '+name);
  if(typeof entry.path!=='string'||!entry.path)throw Error('Local bundle artifact path required');
  const file=fs.realpathSync(path.resolve(root,entry.path)),relative=path.relative(root,file),stat=fs.statSync(file);
  const fold=value=>process.platform==='win32'?value.toLowerCase():value;
  if(relative==='..'||relative.startsWith('..'+path.sep)||path.isAbsolute(relative)||fold(relative)!==fold(expectedRelative)||!stat.isFile()||stat.size<1||stat.size>1024**3||path.extname(file).toLowerCase()!==extension)throw Error('Runtime artifact must match its installed bundle location');
  routes.set('/'+name,file);
  if(name!=='contract.mjs'&&name!=='contract.wasm')supplemental.push(name);
 }
 if(!routes.has('/contract.mjs')||!routes.has('/contract.wasm'))throw Error('Complete browser runtime pair required');
 const declared=descriptor.configuration.runtimeArtifacts;
 if(!Array.isArray(declared)||JSON.stringify([...declared].sort())!==JSON.stringify(supplemental.sort()))throw Error('Supplemental artifact list differs from bundle');
 return {routes,buildId:descriptor.configuration.buildId};
}
module.exports={runtimePackageRoutes};
