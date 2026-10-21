'use strict';
const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const {guardBrowserGamepadMetadata,helpers}=require('./guard-browser-gamepad-metadata.cjs');
const [runtime]=process.argv.slice(2);assert(runtime,'Pass an actual generated browser runtime for the regression');
const original=fs.readFileSync(runtime,'utf8'),patched=guardBrowserGamepadMetadata(original);assert.equal(patched.changed,3);
function functions(source,navigator){return helpers.map(name=>{
 const pattern=new RegExp('(function '+name+'\\(device_index\\) \\{[\\s\\S]*?\\})\\r?\\n'+name+'\\.sig =');
 const match=source.match(pattern);assert(match,name);return vm.runInNewContext('('+match[1]+')',{navigator});
});}
let beforeFailures=0,checks=0;
for(const fn of functions(original,{getGamepads:()=>[null]})){assert.throws(()=>fn(0),/id/);beforeFailures++;}
for(const slots of [[null],[],new Array(4),[undefined],[{id:null}],[{id:4}]]){
 for(const fn of functions(patched.source,{getGamepads:()=>slots})){assert.equal(fn(0),0);checks++;}
}
for(const id of ['Wireless Controller (STANDARD GAMEPAD Vendor: 054c Product: 09cc)','046d-c216-Logitech Dual Action','Xbox 360 Controller (XInput STANDARD GAMEPAD)','Generic USB Gamepad','']){
 const navigator={getGamepads:()=>[{id}]},before=functions(original,navigator),after=functions(patched.source,navigator);
 for(let i=0;i<helpers.length;i++){assert.equal(after[i](0),before[i](0));checks++;}
}
assert.equal(guardBrowserGamepadMetadata(patched.source).changed,0);checks++;
assert.throws(()=>guardBrowserGamepadMetadata(original.replace('function SDL_GetEmscriptenJoystickVendor','function unsupportedVendor')),/Unsupported SDL/);checks++;
console.log(JSON.stringify({status:'VERIFIED actual emitted SDL helper regression',beforeNullFailures:beforeFailures,afterChecks:checks,validControllerParsingUnchanged:true,patchIdempotent:true,unknownSdkFormatRejected:true}));
