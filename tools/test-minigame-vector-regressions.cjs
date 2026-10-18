'use strict';
// Execute extracted production arithmetic; this does not validate rendered gameplay.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),{execFileSync}=require('node:child_process');
const root=path.resolve(__dirname,'../../..'),sdk=path.join(root,'work/emsdk'),python=path.join(sdk,'python/3.13.3_64bit/python.exe');
const env={...process.env,EM_CONFIG:path.join(sdk,'.emscripten'),PATH:path.dirname(python)+path.delimiter+process.env.PATH};
function compile(input,output,flags=[]){execFileSync(python,[path.join(sdk,'upstream/emscripten/emcc.py'),input,...flags,'-sENVIRONMENT=node','-o',output],{cwd:root,env,stdio:['ignore','pipe','pipe']});return execFileSync(process.execPath,[output],{cwd:root,encoding:'utf8'});}
execFileSync(process.execPath,[path.join(__dirname,'generate-m417-water-wave-fixture.cjs')],{cwd:root});
const original=compile('work/m417-water-wave-fixture.c','work/m417-water-original-regression.js');
assert.match(original,/startup zero=116 interior=0 face=0/);assert.match(original,/601 updates zero=69716 interior=0 face=0/);
const fixed=compile('work/m417-water-wave-fixture.c','work/m417-water-fixed-regression.js',['-DTARGET_PC']);
assert.match(fixed,/startup zero=0 interior=0 face=0/);assert.match(fixed,/601 updates zero=0 interior=0 face=0/);
execFileSync(process.execPath,[path.join(__dirname,'generate-m430-listener-fixture.cjs')],{cwd:root});compile('work/m430-listener-fixture.c','work/m430-listener-regression.js');
const report={scope:'Extracted production arithmetic and listener lifecycle; external ripple callback and graphics stubbed',original:original.trim(),fixed:fixed.trim(),listenerLifecycleAssertionsPassed:true,renderingVerified:false,fullMatchVerified:false};
fs.writeFileSync(path.join(root,'outputs/validation/minigame-vector-regression-20261005.json'),JSON.stringify(report,null,2)+'\n');console.log('PASS original failure reproduction, fixed startup/waves, and listener lifecycle.');
