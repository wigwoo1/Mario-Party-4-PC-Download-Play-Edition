'use strict';
const test=require('node:test'),assert=require('node:assert/strict');
const {publicIpv4,ipOrigin,tunnelOrigin,automaticHosting}=require('./automatic-hosting.cjs');
test('IP hostname accepts public canonical IPv4 and excludes LAN, carrier NAT and reserved addresses',()=>{
 assert.equal(ipOrigin('8.8.4.4'),'https://partyboard.8-8-4-4.sslip.io');
 for(const ip of ['0.1.2.3','10.0.0.198','127.0.0.1','100.64.0.1','100.127.255.255','169.254.2.3','172.31.2.3','192.168.1.1','192.0.2.1','198.18.0.1','198.51.100.1','203.0.113.1','224.1.1.1','255.255.255.255','1.02.3.4','1.2.3','::1','8.8.8.8/path'])assert.equal(publicIpv4(ip),false,ip);
});
test('fallback parser excludes lookalike domains and extracts a split-output complete hostname',()=>{
 assert.equal(tunnelOrigin('log https://quiet-green-dog.trycloudflare.com |'),'https://quiet-green-dog.trycloudflare.com');
 for(const value of ['http://quiet.trycloudflare.com','https://quiet.trycloudflare.com.evil.test','https://quiet.trycloudflare.com-evil'])assert.equal(tunnelOrigin(value),null);
});
function fixture({failure}={}){
 const events=[];const resource=name=>({async close(){events.push('close:'+name);}});
 const service={...resource('service'),async publish(origin){events.push('publish:'+origin);if(failure==='publish')throw Error('publish failed');}};
 return {events,adapter:{notice:()=>events.push('fallback-notice'),service:async()=>service,publicAddress:async()=>{if(failure==='address')throw Error('no IPv4');return '8.8.4.4';},
 direct:async()=>{events.push('direct');if(failure==='direct')throw Error('port occupied');return resource('direct');},
 fallback:async()=>{events.push('fallback');if(failure==='tool')throw Error('missing tool');return {...resource('fallback'),origin:'https://test-room.trycloudflare.com'};},
 verify:async origin=>{events.push('verify:'+origin);if(failure==='verify-direct'&&!origin.endsWith('trycloudflare.com'))throw Error('untrusted certificate');if(failure==='verify-all')throw Error('wrong instance');}}};
}
test('direct is published only after verification; cleanup is owned and idempotent',async()=>{
 const {events,adapter}=fixture(),hosting=await automaticHosting(adapter);
 assert.equal(hosting.mode,'direct');assert.deepEqual(events,['direct','verify:https://partyboard.8-8-4-4.sslip.io','publish:https://partyboard.8-8-4-4.sslip.io']);
 await hosting.close();await hosting.close();assert.deepEqual(events.slice(-2),['close:direct','close:service']);
});
for(const failure of ['address','direct','verify-direct'])test(failure+' automatically falls back without publishing an unverified IP address',async()=>{
 const {events,adapter}=fixture({failure}),hosting=await automaticHosting(adapter);
 assert.equal(hosting.mode,'fallback');assert.equal(events.filter(row=>row.startsWith('publish:')).length,1);
 assert.equal(events.at(-1),'publish:https://test-room.trycloudflare.com');
 if(failure==='verify-direct')assert.ok(events.indexOf('close:direct')<events.indexOf('fallback'));
 await hosting.close();assert.deepEqual(events.slice(-2),['close:fallback','close:service']);
});
for(const failure of ['verify-all','publish'])test(failure+' closes acquired resources and never reports success',async()=>{
 const {events,adapter}=fixture({failure});await assert.rejects(automaticHosting(adapter));assert.equal(events.at(-1),'close:service');
});
test('missing fallback tool still closes the local service',async()=>{
 const {events,adapter}=fixture({failure:'address'});adapter.fallback=async()=>{throw Error('missing tool');};
 await assert.rejects(automaticHosting(adapter),/missing tool/);assert.equal(events.at(-1),'close:service');
});
test('cancelling startup closes the acquired service and never starts fallback or publishes',async()=>{
 const controller=new AbortController(),{events,adapter}=fixture();adapter.signal=controller.signal;
 adapter.publicAddress=async()=>{controller.abort();return '8.8.4.4';};
 await assert.rejects(automaticHosting(adapter),/cancelled/);
 assert.deepEqual(events,['close:service']);
});
