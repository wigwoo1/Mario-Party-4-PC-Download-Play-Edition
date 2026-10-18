'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
let expire,cleared=0;
const context={require,module:{exports:{}},console,
 setTimeout(callback,delay){assert.equal(delay,120000);expire=callback;return 7;},
 clearTimeout(handle){assert.equal(handle,7);cleared++;}};
vm.createContext(context);
vm.runInContext(fs.readFileSync(__dirname+'/drive-production-full-match.cjs','utf8')+'\nglobalThis.read=readGameState;globalThis.readInputs=readInputObservations;',context);
(async()=>{
 const state={frame:123};
 assert.equal(await context.read({evaluate:()=>Promise.resolve(state)},0),state);
 assert.equal(cleared,1);
 const pending=context.read({evaluate:()=>new Promise(()=>{})},1);
 expire();
 await assert.rejects(pending,/Peer 1.*120 seconds/);
 assert.equal(cleared,2);
 await assert.rejects(context.read({evaluate:()=>Promise.reject(new Error('browser closed'))},0),/browser closed/);
 assert.equal(cleared,3);
 const records=[{frame:'180'}];
 assert.equal(await context.readInputs({evaluate:()=>Promise.resolve(records)},2),records);
 const stalledInputs=context.readInputs({evaluate:()=>new Promise(()=>{})},3);
 expire();
 await assert.rejects(stalledInputs,/Peer 3 final input.*120 seconds/);
 assert.equal(cleared,5);
 console.log('PASS full-match observation: successful state, nonresponsive peer deadline, browser rejection and timer cleanup.');
})().catch(error=>{console.error(error);process.exitCode=1;});
