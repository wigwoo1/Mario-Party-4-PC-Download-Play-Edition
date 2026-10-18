'use strict';
const {test}=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
test('m443 camera sprite storage covers its setup loop and all fixed accesses',()=>{
 const source=fs.readFileSync(path.join(__dirname,'../src/REL/m443Dll/main.c'),'utf8');
 const capacity=Number(source.match(/s16 lbl_1_bss_10\[(\d+)\]/)[1]);
 const initializer=source.match(/for \(var_r30 = 0; var_r30 < (\d+); var_r30\+\+\)\s*\{\s*lbl_1_bss_10\[var_r30\] = espEntry/);
 assert(initializer,'Actual camera sprite initializer must be covered');
 assert(capacity>=Number(initializer[1]),'Every generated camera sprite needs its own handle slot');
 const uses=source.replace(/s16 lbl_1_bss_10\[\d+\];/,'');
 for(const access of uses.matchAll(/lbl_1_bss_10\[(\d+)\]/g))assert(Number(access[1])<capacity,'Fixed sprite access exceeds storage');
});
