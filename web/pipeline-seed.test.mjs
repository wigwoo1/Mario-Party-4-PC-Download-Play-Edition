import test from 'node:test';import assert from 'node:assert/strict';import {installPipelineSeed} from './pipeline-seed.mjs';
test('pipeline seed verifies immutable SQLite bytes before publishing and removes once',async()=>{
 const bytes=new Uint8Array(100);bytes.set(new TextEncoder().encode('SQLite format 3\0'));
 const sha256=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)),b=>b.toString(16).padStart(2,'0')).join('');
 const calls=[],module={FS:{writeFile:(path,data)=>calls.push(['write',path,data.length]),unlink:path=>calls.push(['unlink',path])}};
 await assert.rejects(installPipelineSeed(module,{blob:new Blob([bytes]),sha256:'0'.repeat(64)}),/integrity/);assert.equal(calls.length,0);
 const seed=await installPipelineSeed(module,{blob:new Blob([bytes]),sha256});assert.equal(seed.scope,'seeded-pipelines-only');seed.dispose();seed.dispose();assert.equal(calls.length,2);
 const abort=new AbortController();abort.abort();await assert.rejects(installPipelineSeed(module,{blob:new Blob([bytes]),sha256},{signal:abort.signal}));assert.equal(calls.length,2);
});
test('closed renderer cache capture owns bytes and claims only observed scenes',async()=>{
 const {captureClosedPipelineSeed}=await import('./pipeline-seed.mjs');
 const bytes=new Uint8Array(100);bytes.set(new TextEncoder().encode('SQLite format 3\0'));
 const captured=await captureClosedPipelineSeed({FS:{readFile:()=>bytes}});bytes.fill(255);
 assert.equal(captured.scope,'observed-scenes-only');assert.equal((new Uint8Array(await captured.blob.arrayBuffer()))[0],83);
 assert.match(captured.sha256,/^[a-f0-9]{64}$/);
});
