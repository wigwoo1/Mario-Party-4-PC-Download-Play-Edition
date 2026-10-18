import test from 'node:test';
import assert from 'node:assert/strict';
import {downloadProgressReporter} from './download-progress-reporter.mjs';

test('a fast real-size chunk burst sends bounded summaries and both completion states',()=>{
  let time=0;const sent=[],total=599818816,chunk=16344;
  const reporter=downloadProgressReporter((bytes,verified)=>sent.push({bytes,verified}),{now:()=>time});
  for(let bytes=0;bytes<total;bytes+=chunk){reporter.report(bytes,total);time+=0.1;}
  reporter.report(total,total);reporter.report(total,total,true);reporter.report(total,total,true);
  assert.ok(sent.length<=Math.ceil(time/250)+2);
  assert.deepEqual(sent.slice(-2),[{bytes:total,verified:false},{bytes:total,verified:true}]);
  assert.ok(sent.every((entry,index)=>index===0||entry.bytes>=sent[index-1].bytes));
});
test('disconnected sends do not consume progress; a new session resets the limit',()=>{
  let connected=false,time=0;const sent=[];
  const reporter=downloadProgressReporter((bytes,verified)=>{
    if(!connected)return false;sent.push({bytes,verified});
  },{now:()=>time});
  assert.equal(reporter.report(20,100),false);connected=true;
  assert.equal(reporter.report(20,100),true);
  assert.equal(reporter.report(40,100),false);time=250;
  assert.equal(reporter.report(40,100),true);
  reporter.reset();assert.equal(reporter.report(0,100),true);
  assert.deepEqual(sent.map(entry=>entry.bytes),[20,40,0]);
});
