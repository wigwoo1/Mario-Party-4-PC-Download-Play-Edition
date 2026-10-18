import test from 'node:test';
import assert from 'node:assert/strict';
import {encodeInput, decodeInput} from './input-packet.mjs';
const value = {session: Uint8Array.of(1, ...Array(15).fill(0)), frame: 0x123456789abcdef0n,
  checkpoint: 0x87654321, slot: 3, pad: Uint8Array.of(0, 1, 128, 127, 192, 63, 255, 129, 17, 99, 253)};
test('native input layout preserves exact controller bytes and 64-bit frames', () => {
  const bytes = encodeInput(value);
  assert.equal(Buffer.from(bytes).toString('hex'),
    '504249500103000001000000000000000000000000000000f0debc9a78563412214365870001807fc03fff811163fd00');
  assert.deepEqual(decodeInput(bytes), value);
  const backing = new Uint8Array(60); backing.set(bytes, 5);
  assert.deepEqual(decodeInput(backing.subarray(5, 53)), value);
  const decoded = decodeInput(bytes); bytes.fill(0);
  assert.deepEqual(decoded, value); // No mutable aliases to transport buffers.
});
test('invalid input packet shapes, versions and reserved bytes rejected', () => {
  const bytes = encodeInput(value);
  for (let n = 0; n < 48; n++) assert.equal(decodeInput(bytes.slice(0, n)), null);
  assert.equal(decodeInput(new Uint8Array(49)), null);
  for (const offset of [0, 4, 6, 7, 47]) {
    const bad = bytes.slice(); bad[offset] ^= 2; assert.equal(decodeInput(bad), null);
  }
  for (const slot of [-1, 4, 0.5]) assert.throws(() => encodeInput({...value, slot}));
  for (const frame of [-1n, 1n << 64n, 1]) assert.throws(() => encodeInput({...value, frame}));
  assert.throws(() => encodeInput({...value, pad: new Uint8Array(12)}));
  assert.throws(() => encodeInput({...value, checkpoint: -1}));
});
