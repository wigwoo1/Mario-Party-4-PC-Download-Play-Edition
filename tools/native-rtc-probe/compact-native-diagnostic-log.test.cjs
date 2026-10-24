const {test}=require('node:test'),assert=require('node:assert/strict');
const {compactNativeDiagnosticLine}=require('./compact-native-diagnostic-log.cjs');
test('full observation stdout shrinks while original driver data remains intact',()=>{
 const event={type:'full-match-observation',requestId:'match-22',frame:510,observations:Array.from({length:256},(_,i)=>({frame:String(255+i),checkpoint:123,frand:44,rand8:9,pads:Array(48).fill(128)}))};
 const before=JSON.stringify(event),line='Native frontend event: '+before,compact=compactNativeDiagnosticLine(line),summary=JSON.parse(compact.slice('Native frontend event: '.length));
 assert.equal(JSON.stringify(event),before);assert.equal(summary.observationCount,256);assert.equal(summary.firstFrame,'255');assert.equal(summary.lastFrame,'510');assert.equal(summary.frame,510);assert.equal(summary.requestId,'match-22');assert.equal(summary.fullObservationDeliveredToDriver,true);assert(compact.length<line.length/100);
});
test('errors, preparation receipts, audio, unknown events and malformed input are preserved verbatim',()=>{
 for(const line of ['MusyX owner initialization failed','Native bundled pipeline seed: {"expectedDefinitions":1335}','Native frontend event: {"type":"failure","error":"bad"}','Native frontend event: {broken','Native frontend event: {"type":"full-match-observation","frame":3}','other text'])assert.equal(compactNativeDiagnosticLine(line),line);
});
test('both native role prefixes and empty observation windows remain auditable',()=>{
 for(const prefix of ['Native frontend event: ','Native external Join event: ']){const summary=JSON.parse(compactNativeDiagnosticLine(prefix+JSON.stringify({type:'full-match-observation',frame:0,observations:[]})).slice(prefix.length));assert.equal(summary.observationCount,0);assert.equal(summary.frame,0);}
});
