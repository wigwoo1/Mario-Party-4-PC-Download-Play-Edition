'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict');
const helpers=['SDL_GetEmscriptenJoystickVendor','SDL_GetEmscriptenJoystickProduct','SDL_IsEmscriptenJoystickXInput'];
const guard="if (!gamepad || typeof gamepad['id'] !== 'string') return 0; /* PartyBoard: a controller can disappear after its connection event. */";
function guardBrowserGamepadMetadata(source){
 let output=source,changed=0;
 for(const name of helpers){
  const pattern=new RegExp('(function '+name+'\\(device_index\\) \\{)([\\s\\S]*?)(\\n'+name+'\\.sig =)');
  const match=output.match(pattern);assert(match,'Unsupported SDL browser metadata helper: '+name);
  const lookup="let gamepad = navigator['getGamepads']()[device_index];";
  assert.equal(match[2].split(lookup).length,2,'Exactly one gamepad lookup required in '+name);
  if(match[2].includes(guard))continue;
  const body=match[2].replace(lookup,lookup+' '+guard);
  output=output.replace(pattern,()=>match[1]+body+match[3]);changed++;
 }
 return {source:output,changed};
}
if(require.main===module){
 const [file,...extra]=process.argv.slice(2);assert(file&&extra.length===0,'Usage: guard-browser-gamepad-metadata.cjs RUNTIME.mjs');
 const result=guardBrowserGamepadMetadata(fs.readFileSync(file,'utf8'));
 if(result.changed)fs.writeFileSync(file,result.source);
 console.log(JSON.stringify({browserGamepadMetadataGuards:helpers.length,changed:result.changed}));
}
module.exports={guardBrowserGamepadMetadata,helpers};
