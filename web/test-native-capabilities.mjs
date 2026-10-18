import { execFileSync } from 'node:child_process';
import assert from 'node:assert/strict';
import { encodeCapabilities, decodeCapabilities } from './controller-capabilities.mjs';
import {encodeInput, decodeInput} from './input-packet.mjs';

if (!process.argv[2]) throw new Error('Pass the compiled capability-vectors executable');
const lines = execFileSync(process.argv[2], [], {encoding: 'utf8', timeout: 10000}).trim().split(/\r?\n/);
assert.equal(lines.length, 6);
for (let i = 0; i < 4; i++) {
  assert.match(lines[i], /^[0-9a-f]{72}$/);
  const bytes = Uint8Array.from(Buffer.from(lines[i], 'hex'));
  const ack = i >= 2, large = i % 2 === 1;
  const expected = {session: Uint8Array.of(9, ...Array(15).fill(0)),
    revision: large ? 0xffffffffffffffffn : 7n, mask: large ? 0 : 0xa0000000};
  assert.deepEqual(decodeCapabilities(bytes, ack), expected);
  assert.deepEqual(encodeCapabilities(expected, ack), bytes);
  assert.equal(decodeCapabilities(bytes, !ack), null);
}
for (let i = 4; i < 6; i++) {
  assert.match(lines[i], /^[0-9a-f]{96}$/);
  const bytes = Uint8Array.from(Buffer.from(lines[i], 'hex'));
  const expected = {session: Uint8Array.of(9, ...Array(15).fill(0)),
    frame: i === 5 ? 0xffffffffffffffffn : 0n, checkpoint: 0x87654321, slot: i === 5 ? 3 : 0,
    pad: Uint8Array.of(0, 1, 128, 127, 192, 63, 255, 129, 17, 99, 253)};
  assert.deepEqual(decodeInput(bytes), expected);
  assert.deepEqual(encodeInput(expected), bytes);
}
console.log('C++/JavaScript interoperability passed: four capability and two input packets');
