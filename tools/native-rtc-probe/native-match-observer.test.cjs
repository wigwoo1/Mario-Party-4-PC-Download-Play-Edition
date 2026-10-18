'use strict';
const assert=require('node:assert/strict');
const {createNativeMatchObserver}=require('./native-match-observer.cjs');
(async()=>{
 const sent=[],observer=createNativeMatchObserver({send:packet=>sent.push(packet)});
 const first=observer.observe(),second=observer.observe();await Promise.resolve();await Promise.resolve();
 const response=index=>({type:'full-match-observation',requestId:sent[index].requestId,frame:index,
  progress:{turn:1,maxTurn:20,board:0,player:0,overlay:89,event:0},observations:[]});
 assert(observer.receive(response(1)));assert.equal((await second).frame,1);
 assert(!observer.receive(response(1)));assert(observer.receive(response(0)));assert.equal((await first).frame,0);
 const invalid=observer.observe();await Promise.resolve();await Promise.resolve();
 observer.receive({...response(2),frame:null});await assert.rejects(invalid,/Invalid/);
 const pending=observer.observe();observer.close();await assert.rejects(pending,/closed/);await assert.rejects(observer.observe(),/closed/);
 const timeout=createNativeMatchObserver({send:()=>{},timeoutMs:5});await assert.rejects(timeout.observe(),/timed out/);timeout.close();
 const failed=createNativeMatchObserver({send:()=>{throw Error('write failed');}});await assert.rejects(failed.observe(),/write failed/);failed.close();
 console.log('PASS native observer: correlated out-of-order replies, stale rejection, validation, close, timeout and send failure');
})().catch(error=>{console.error(error);process.exitCode=1;});
