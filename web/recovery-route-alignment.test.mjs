import test from 'node:test';import assert from 'node:assert/strict';
import {alignRecoveryRoutes} from './recovery-route-alignment.mjs';
test('two sweeps align earlier lagging routes when the last guest starts ahead',async()=>{
 let host=7;const frames={guest1:7,guest2:7,guest3:8},calls=[];
 await alignRecoveryRoutes({peers:Object.keys(frames),current:()=>true,reconcile:async(peer,sweep)=>{
  calls.push([peer,sweep]);const target=Math.max(host,frames[peer]);assert.ok(target-host<=1&&target-frames[peer]<=1);host=frames[peer]=target;
 }});
 assert.equal(host,8);assert.deepEqual(Object.values(frames),[8,8,8]);assert.equal(calls.length,6);
});
test('loss or failed verification stops alignment and never visits later routes',async()=>{
 for(const failing of [false,true]){
  let current=true,calls=0;
  await assert.rejects(alignRecoveryRoutes({peers:['guest1','guest2','guest3'],current:()=>current,reconcile:async()=>{
   calls++;if(failing)throw Error('State differs');current=false;
  }}),failing?/State differs/:/stale/);assert.equal(calls,1);
 }
});
test('route count and identity must match a unique assigned player list',async()=>{
 for(const peers of [[],['a','a'],['a','b','c','d'],['']])await assert.rejects(alignRecoveryRoutes({peers,current:()=>true,reconcile:async()=>{}}),/Assigned/);
});
