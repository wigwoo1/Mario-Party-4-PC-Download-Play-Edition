const test=require('node:test'),assert=require('node:assert/strict');
test('automatic preparation waits for actual asset/configuration/seed receipts and renews readiness for changed membership',async()=>{
 const {automaticPreparation}=await import('./automatic-preparation.mjs');
 let value={canAccept:true,membership:2},calls=[];
 const owner=automaticPreparation({state:()=>({...value}),accept:async()=>calls.push('accept'),prepare:async()=>{calls.push('prepare');value.prepared=true;},ready:()=>calls.push('ready'),error:e=>{throw e;}});
 await owner.refresh();await owner.refresh();assert.deepEqual(calls,['accept']);
 value.assetsVerified=value.configuration=true;await owner.refresh();assert.deepEqual(calls,['accept']);
 value.seedVerified=true;await owner.refresh();assert.deepEqual(calls,['accept','prepare']);
 value.assignedVerified=true;await owner.refresh();await owner.refresh();assert.deepEqual(calls,['accept','prepare','ready']);
 value.ready=true;value.membership=3;await owner.refresh();assert.equal(calls.length,3);
 value.ready=false;await owner.refresh();assert.equal(calls.length,4);
 value.running=true;value.membership=4;await owner.refresh();assert.equal(calls.length,4);owner.close();
});
test('preparation failure is reported once and never becomes ready or retries in a loop',async()=>{
 const {automaticPreparation}=await import('./automatic-preparation.mjs');let attempts=0,errors=[];
 const owner=automaticPreparation({state:()=>({assetsVerified:true,configuration:true,seedVerified:true,assignedVerified:true}),prepare:async()=>{attempts++;throw Error('invalid shader cache');},ready:()=>assert.fail('must not mark ready'),error:e=>errors.push(e.message)});
 await owner.refresh();await owner.refresh();assert.equal(attempts,1);assert.deepEqual(errors,['invalid shader cache']);
});
test('closing while preparation is pending prevents stale readiness',async()=>{
 const {automaticPreparation}=await import('./automatic-preparation.mjs');let release;
 const owner=automaticPreparation({state:()=>({assetsVerified:true,configuration:true,seedVerified:true,assignedVerified:true}),prepare:()=>new Promise(r=>release=r),ready:()=>assert.fail('stale readiness'),error:e=>{throw e;}});
 const pending=owner.refresh();await Promise.resolve();owner.close();release();await pending;
});
