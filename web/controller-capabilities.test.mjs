import test from 'node:test';
import assert from 'node:assert/strict';
import { encodeCapabilities, decodeCapabilities } from './controller-capabilities.mjs';

test('native wire layout, ownership revision and distinct acknowledgement', () => {
  const value = {session: Uint8Array.of(9, ...Array(15).fill(0)), revision: 7n, mask: 0xa0000000};
  const bytes = encodeCapabilities(value);
  assert.equal(Buffer.from(bytes).toString('hex'),
    '5042435001000000090000000000000000000000000000000700000000000000000000a0');
  assert.deepEqual(decodeCapabilities(bytes), value);
  const ack = encodeCapabilities(value, true);
  assert.equal(ack[3], 65);
  assert.deepEqual(decodeCapabilities(ack, true), value);
  assert.equal(decodeCapabilities(bytes, true), null);
  assert.equal(decodeCapabilities(ack), null);
  const backing = new Uint8Array(50); backing.set(bytes, 5);
  assert.deepEqual(decodeCapabilities(backing.subarray(5, 41)), value);
  const large = {...value, revision: 0xffffffffffffffffn, mask: 0};
  assert.deepEqual(decodeCapabilities(encodeCapabilities(large)), large);
});

test('malformed packets and invalid encoding inputs rejected', () => {
  const value = {session: Uint8Array.of(1, ...Array(15).fill(0)), revision: 1n, mask: 0};
  const bytes = encodeCapabilities(value);
  for (let size = 0; size < 36; size++) assert.equal(decodeCapabilities(bytes.slice(0, size)), null);
  assert.equal(decodeCapabilities(new Uint8Array(37)), null);
  for (const offset of [0, 4, 5, 6, 7, 32]) {
    const bad = bytes.slice(); bad[offset] ^= 2;
    assert.equal(decodeCapabilities(bad), null);
  }
  for (const revision of [0n, -1n, 1n << 64n, 1])
    assert.throws(() => encodeCapabilities({...value, revision}));
  for (const mask of [-1, 1, 0x100000000, 0.5, NaN])
    assert.throws(() => encodeCapabilities({...value, mask}));
  assert.throws(() => encodeCapabilities({...value, session: new Uint8Array(16)}));
});
