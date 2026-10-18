import test from 'node:test';
import assert from 'node:assert/strict';
import {classifyConnectionRoute,readConnectionRoute,monitorConnectionRoutes} from './connection-route.mjs';
function stats(local='host',remote='srflx'){
 return new Map([['transport',{type:'transport',selectedCandidatePairId:'pair'}],['pair',{type:'candidate-pair',state:'succeeded',localCandidateId:'local',remoteCandidateId:'remote'}],['local',{type:'local-candidate',candidateType:local}],['remote',{type:'remote-candidate',candidateType:remote}]]);
}
test('only selected known candidate types can establish direct or relay labels',()=>{
 for(const local of ['host','srflx','prflx'])for(const remote of ['host','srflx','prflx'])assert.equal(classifyConnectionRoute('connected',stats(local,remote)),'direct');
 for(const [local,remote] of [['relay','host'],['host','relay'],['relay','relay'],['relay','unknown']])assert.equal(classifyConnectionRoute('connected',stats(local,remote)),'relayed');
 assert.equal(classifyConnectionRoute('connected',stats('host','unknown')),'unknown');
 const reports=stats();reports.delete('transport');assert.equal(classifyConnectionRoute('connected',reports),'unknown');
 reports.get('pair').nominated=true;assert.equal(classifyConnectionRoute('connected',reports),'direct');
 reports.set('other',{type:'candidate-pair',state:'succeeded',nominated:true});assert.equal(classifyConnectionRoute('connected',reports),'unknown');
});
test('failed, interrupted, connecting and closed override old pair statistics',()=>{
 for(const [state,label] of [['failed','unreachable'],['disconnected','interrupted'],['new','connecting'],['connecting','connecting'],['closed','closed']])assert.equal(classifyConnectionRoute(state,stats()),label);
 const reports=stats();reports.get('pair').state='in-progress';assert.equal(classifyConnectionRoute('connected',reports),'unknown');
 const ambiguous=stats();ambiguous.set('second-transport',{type:'transport',selectedCandidatePairId:'other'});assert.equal(classifyConnectionRoute('connected',ambiguous),'unknown');
});
test('state changes during an outstanding stats request cannot report stale direct connectivity',async()=>{
 let complete;const connection={connectionState:'connected',getStats:()=>new Promise(resolve=>complete=resolve)};
 const reading=readConnectionRoute(connection);connection.connectionState='closed';complete(stats());assert.equal(await reading,'closed');
 assert.equal(await readConnectionRoute({connectionState:'connected',getStats:async()=>{throw Error('stats unavailable');}}),'unknown');
});
test('replacement connections and stop discard pending statistics even for the same peer id',async()=>{
 let complete,tick;const changed=[];
 const old={connectionState:'connected',getStats:()=>new Promise(resolve=>complete=resolve)};
 const replacement={connectionState:'connected',getStats:async()=>stats('relay','host')};
 let entries=[{peer:'guest1',label:'Guest 1',connection:old}];
 const monitor=monitorConnectionRoutes({current:()=>entries,changed:routes=>changed.push(routes),schedule:callback=>(tick=callback,1),cancel:()=>{}});
 entries=[{peer:'guest1',label:'Guest 1',connection:replacement}];complete(stats());
 await new Promise(resolve=>setImmediate(resolve));assert.deepEqual(changed,[]);
 await tick();assert.equal(changed.at(-1)[0].route,'relayed');
 entries=[];await monitor.refresh();assert.deepEqual(changed.at(-1),[]);
 monitor.close();await tick();assert.equal(changed.length,2);
});
