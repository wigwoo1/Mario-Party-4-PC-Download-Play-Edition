import test from 'node:test';
import assert from 'node:assert/strict';
import { iceServerConfiguration } from './ice-servers.mjs';

test('ICE settings can be empty or contain a STUN server', () => {
  assert.deepEqual(iceServerConfiguration(), []);
  assert.deepEqual(iceServerConfiguration({ stun: ' stun:stun.example.test:3478 ' }), [
    { urls: 'stun:stun.example.test:3478' },
  ]);
});

test('configured TURN relay credentials are attached only to local ICE config', () => {
  assert.deepEqual(iceServerConfiguration({
    stun: 'stuns:stun.example.test:5349',
    turn: 'turns:relay.example.test:5349?transport=tcp',
    username: ' user ', credential: ' secret ',
  }), [
    { urls: 'stuns:stun.example.test:5349' },
    { urls: 'turns:relay.example.test:5349?transport=tcp', username: 'user', credential: 'secret' },
  ]);
});

test('TURN URL and both credentials are required as a set', () => {
  assert.throws(() => iceServerConfiguration({ turn: 'turn:relay.example.test:3478' }), /both TURN/);
  assert.throws(() => iceServerConfiguration({ turn: 'turn:relay.example.test:3478', username: 'u' }), /both TURN/);
  assert.throws(() => iceServerConfiguration({ username: 'u', credential: 'p' }), /Add a TURN/);
  assert.throws(() => iceServerConfiguration({ stun: 'stun:ok.test', turn: 'stun:wrong.test', username: 'u', credential: 'p' }), /Use a TURN URL/);
  assert.throws(() => iceServerConfiguration({ stun: 'turn:wrong.test' }), /Use a STUN URL/);
});

test('ICE settings reject malformed URLs and unsafe credentials', () => {
  assert.throws(() => iceServerConfiguration({ stun: 'https://stun.example.test' }), /valid STUN or TURN/);
  assert.throws(() => iceServerConfiguration({ turn: 'turn:relay.test', username: 'a\nb', credential: 'p' }), /credentials are invalid/);
  assert.throws(() => iceServerConfiguration({ turn: 'turn:relay.test', username: 'u'.repeat(257), credential: 'p' }), /credentials are invalid/);
});
