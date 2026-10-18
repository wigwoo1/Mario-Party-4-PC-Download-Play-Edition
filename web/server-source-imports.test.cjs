const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const {createRequire}=require('node:module');
test('production app/platform/audio static imports are all explicitly served',()=>{
 const server=path.join(__dirname,'serve.cjs'),source=fs.readFileSync(server,'utf8');
 const boundary=source.indexOf('const handleRequest=');assert.ok(boundary>0);
 const routes=new Function('require','__dirname','process',source.slice(0,boundary)+'\nreturn files;')(
   createRequire(server),__dirname,{env:{}});
 const pending=['/app.mjs','/browser-platform.mjs','/audio-layout.mjs'],checked=new Set();
 while(pending.length){
  const route=pending.pop();if(checked.has(route))continue;checked.add(route);
  const target=routes.get(route);assert.ok(target,'Missing production module route: '+route);
  const filename=path.isAbsolute(target)?target:path.join(__dirname,target);
  const code=fs.readFileSync(filename,'utf8');
  for(const match of code.matchAll(/\b(?:import|export)\s+(?:[^;'"\n]*?\s+from\s*)?['"](\.\.?\/[^'"\n]+\.mjs)['"]/g))
   pending.push(path.posix.normalize(path.posix.join(path.posix.dirname(route),match[1])));
 }
 assert.ok(checked.has('/browser-audio-destination.mjs'));
 assert.ok(checked.size>20,'Check the real application dependency graph');
});
