const {test}=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {runtimePackageRoutes}=require('./runtime-package-routes.cjs');
function fixture(t){
 const workspace=path.resolve(__dirname,'../../../work');
 const directory=fs.mkdtempSync(path.join(workspace,'runtime-route-fixture-')),root=path.join(directory,'bundle');
 t.after(()=>{const target=path.resolve(directory);if(path.dirname(target)!==workspace||!path.basename(target).startsWith('runtime-route-fixture-'))throw Error('Invalid fixture cleanup target');fs.rmSync(target,{recursive:true,force:true});});
 fs.mkdirSync(path.join(root,'browser-runtime'),{recursive:true});fs.mkdirSync(path.join(root,'res'));
 fs.writeFileSync(path.join(root,'browser-runtime/contract.mjs'),'export default()=>{}');
 fs.writeFileSync(path.join(root,'browser-runtime/contract.wasm'),Buffer.from([0,97,115,109]));
 fs.writeFileSync(path.join(root,'dol.dll'),'compiled fixture');
 fs.writeFileSync(path.join(root,'res/Font Face.ttf'),'font fixture');
 fs.writeFileSync(path.join(directory,'outside.dll'),'outside bundle');
 const resource='native/res/'+Buffer.from('Font Face.ttf').toString('hex');
 const descriptor={configuration:{profile:'unlocked-no-save-v1',buildId:'a'.repeat(64),runtimeArtifacts:['native/dol.dll',resource]},files:[
  {name:'contract.mjs',path:'browser-runtime/contract.mjs'},{name:'contract.wasm',path:'browser-runtime/contract.wasm'},
  {name:'native/dol.dll',path:'dol.dll'},{name:resource,path:'res/Font Face.ttf'}
 ]};
 const filename=path.join(root,'native-session-package.json');
 const read=()=>{fs.writeFileSync(filename,JSON.stringify(descriptor));return runtimePackageRoutes(filename);};
 return {descriptor,read,root,filename,directory};
}
test('installed pair, native DLL and encoded resource have only explicit routes',t=>{
 const f=fixture(t),result=f.read();assert.equal(result.routes.size,4);assert.equal(result.buildId,'a'.repeat(64));
 assert.equal(result.routes.get('/contract.mjs'),path.join(f.root,'browser-runtime/contract.mjs'));
 assert.ok(!result.routes.has('/native-session-package.json'));assert.ok(!result.routes.has('/game.ciso'));
});
test('reject duplicate and traversing artifact names',t=>{
 const f=fixture(t);f.descriptor.files.push({...f.descriptor.files[2]});assert.throws(f.read,/canonical artifact/);
 f.descriptor.files.pop();f.descriptor.files[2].name='native/../outside.dll';assert.throws(f.read,/canonical artifact/);
});
test('reject an artifact outside the bundle or aliased to package metadata',t=>{
 const f=fixture(t);f.descriptor.files[2].path='../outside.dll';assert.throws(f.read,/bundle location/);
 f.descriptor.files[2].path='native-session-package.json';assert.throws(f.read,/bundle location/);
});
test('reject game files even if encoded as a resource',t=>{
 const f=fixture(t);f.descriptor.files[2].name='native/game.ciso';assert.throws(f.read,/Unsupported runtime artifact/);
 f.descriptor.files[2].name='native/res/'+Buffer.from('game.ciso').toString('hex');assert.throws(f.read,/Unsupported runtime resource/);
});
test('reject incomplete or inconsistent runtime manifests',t=>{
 const f=fixture(t);f.descriptor.configuration.runtimeArtifacts=[];assert.throws(f.read,/Supplemental artifact/);
 f.descriptor.configuration.runtimeArtifacts=['native/dol.dll',f.descriptor.files[3].name];
 f.descriptor.files.shift();assert.throws(f.read,/Complete browser runtime/);
});
