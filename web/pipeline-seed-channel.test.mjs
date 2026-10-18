import test from 'node:test';import assert from 'node:assert/strict';import {sendPipelineSeed,receivePipelineSeed} from './pipeline-seed-channel.mjs';
test('pipeline seed chunks reassemble with integrity and corrupted bytes reject',async()=>{
 for(const corrupt of [false,true]){
 const bytes=new Uint8Array(40000).fill(7),sha256=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)),b=>b.toString(16).padStart(2,'0')).join('');
 const receiver=new EventTarget();const result=new Promise(resolve=>receivePipelineSeed(receiver,{complete:seed=>resolve(seed),error:resolve}));
 const sender={readyState:'open',bufferedAmount:0,send(data){if(corrupt&&data instanceof ArrayBuffer)new Uint8Array(data)[0]=8;const event=new Event('message');event.data=data;receiver.dispatchEvent(event);}};
 await sendPipelineSeed(sender,{blob:new Blob([bytes]),sha256});const actual=await result;
 if(corrupt)assert.match(actual.message,/integrity/);else assert.equal(actual.blob.size,40000);
 }
});
