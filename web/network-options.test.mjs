import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { iceServerConfiguration } from './ice-servers.mjs';

const page = readFileSync(new URL('./index.html', import.meta.url), 'utf8');
const app = readFileSync(new URL('./app.mjs', import.meta.url), 'utf8');

test('lobby keeps STUN optional and explains when TURN is needed', () => {
  const input = page.match(/<input id="stun" value="([^"]*)" placeholder="([^"]+)"/);
  assert.ok(input, 'editable STUN URL field is required');
  assert.equal(input[1], '');
  assert.equal(input[2], 'stun:stun.l.google.com:19302');
  assert.deepEqual(iceServerConfiguration({ stun: input[1] }), []);
  assert.match(page, /for example, stun:stun\.l\.google\.com:19302/);
  assert.match(page, /STUN does not relay game traffic/);
  assert.match(page, /Optional TURN relay URL/);
  assert.match(page, /Credentials stay local/);
  assert.match(app, /Connection setup timed out\. Check the network options for a reachable STUN server or TURN relay/);
});
