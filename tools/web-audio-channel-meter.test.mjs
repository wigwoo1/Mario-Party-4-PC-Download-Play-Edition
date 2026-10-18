import test from 'node:test';
import assert from 'node:assert/strict';
import {attachAudioChannelMeter} from './web-audio-channel-meter.mjs';
test('reads completed original PCM without changing data, callback receiver or return value',()=>{
  const pcm=[Float32Array.from([0,0]),Float32Array.from([0,0])];let receiver,argument;
  const original=function(event){receiver=this;argument=event;pcm[0].set([.25,-.5]);pcm[1].set([-1,1]);return 7;};
  const node={onaudioprocess:original},event={outputBuffer:{length:2,numberOfChannels:2,getChannelData:index=>pcm[index]}};
  const meter=attachAudioChannelMeter(node);assert.equal(node.onaudioprocess(event),7);
  assert.equal(receiver,node);assert.equal(argument,event);
  assert.deepEqual([...pcm[0]],[.25,-.5]);assert.deepEqual([...pcm[1]],[-1,1]);
  assert.deepEqual(meter.snapshot(),{callbacks:1,frames:2,energy:[.75,2,0,0,0,0],channels:[2],nonFinite:0});
  const snapshot=meter.snapshot();snapshot.energy[0]=999;assert.equal(meter.snapshot().energy[0],.75);
  meter.reset();assert.equal(meter.snapshot().callbacks,0);assert.deepEqual(meter.snapshot().energy,Array(6).fill(0));
  assert.equal(meter.restore(),true);assert.equal(node.onaudioprocess,original);assert.equal(meter.restore(),false);
});
test('preserves original callback errors and never replaces another owner on restore',()=>{
  assert.throws(()=>attachAudioChannelMeter({}),/callback/);
  const node={onaudioprocess(){throw Error('original');}},meter=attachAudioChannelMeter(node);
  assert.throws(()=>node.onaudioprocess({}),/original/);assert.equal(meter.snapshot().callbacks,0);
  const replacement=()=>{};node.onaudioprocess=replacement;assert.equal(meter.restore(),false);assert.equal(node.onaudioprocess,replacement);
});
