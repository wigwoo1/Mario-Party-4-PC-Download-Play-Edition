const test=require('node:test'),assert=require('node:assert/strict');
const {parseLocalAudioLayouts,localAudioChannels}=require('./local-audio-layouts.cjs');
test('preserves existing defaults and maps distinct local layouts to assigned slots',()=>{
 assert.equal(parseLocalAudioLayouts(),null);assert.equal(localAudioChannels(null,'guest1'),undefined);
 const layouts=parseLocalAudioLayouts('[2,6,2,6]');assert.ok(Object.isFrozen(layouts));
 assert.deepEqual(['host','guest1','guest2','guest3'].map(peer=>localAudioChannels(layouts,peer)),[2,6,2,6]);
 assert.throws(()=>localAudioChannels(layouts,'guest4'),/original peer/);
});
test('rejects missing slots or unsupported values without guessing a peer layout',()=>{
 for(const value of ['null','{}','[]','[2,6]','[2,6,2,8]','[2,6,"2",6]','[2,6,2,6,2]'])
  assert.throws(()=>parseLocalAudioLayouts(value));
});
