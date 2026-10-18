import test from 'node:test';
import assert from 'node:assert/strict';
import {configureAudioDestination} from './browser-audio-destination.mjs';
test('selects six supported output channels after SDL creates its context',()=>{
  const destination={channelCount:2,maxChannelCount:8},module={SDL3:{audioContext:{destination}}};
  assert.deepEqual(configureAudioDestination(module,6),{requested:6,available:true,applied:6,maximum:8});
  assert.equal(destination.channelCount,6);
  assert.equal(configureAudioDestination(module,2).applied,2);
});
test('stereo-only devices keep stereo and lack of device is not a game failure',()=>{
  const destination={channelCount:2,maxChannelCount:2},module={SDL3:{audioContext:{destination}}};
  assert.equal(configureAudioDestination(module,6).applied,2);
  assert.deepEqual(configureAudioDestination({},6),{requested:6,available:false,applied:0});
  assert.throws(()=>configureAudioDestination({},4),/speaker layout/);
});
test('read-only or fixed destinations retain actual channels without failing gameplay',()=>{
  const destination={get channelCount(){return 2;},set channelCount(_){throw Error('unsupported');},maxChannelCount:6};
  assert.equal(configureAudioDestination({SDL3:{audioContext:{destination}}},6).rejected,true);
  assert.equal(destination.channelCount,2);
  const fixed={channelCount:2,maxChannelCount:0};
  assert.equal(configureAudioDestination({SDL3:{audioContext:{destination:fixed}}},6).applied,2);
});
