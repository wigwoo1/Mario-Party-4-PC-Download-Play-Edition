'use strict';
const fs = require('node:fs'), path = require('node:path'), http = require('node:http'), assert = require('node:assert/strict');
const {chromium} = require(process.env.PLAYWRIGHT_MODULE || 'playwright');
const [input, output] = process.argv.slice(2);
assert.ok(input && output);
assert.equal(fs.existsSync(output), false, 'Preserve prior evidence');
fs.mkdirSync(output, {recursive: true});
for (const name of ['contract.mjs', 'contract.wasm']) fs.copyFileSync(path.join(input, name), path.join(output, name), fs.constants.COPYFILE_EXCL);
const logs = [], errors = [];
let browser, server;
(async () => {
  server = http.createServer((request, response) => {
    const name = new URL(request.url, 'http://localhost').pathname.slice(1);
    if (!name) { response.setHeader('Content-Type', 'text/html'); response.end('<!doctype html>'); return; }
    if (!['contract.mjs', 'contract.wasm'].includes(name)) { response.writeHead(404).end(); return; }
    response.setHeader('Content-Type', name.endsWith('.wasm') ? 'application/wasm' : 'text/javascript');
    fs.createReadStream(path.join(output, name)).pipe(response);
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  browser = await chromium.launch({channel: 'msedge', headless: true, args: ['--mute-audio']});
  const page = await browser.newPage();
  page.on('console', message => logs.push(message.text()));
  page.on('pageerror', error => errors.push(error.message));
  await page.goto('http://127.0.0.1:' + server.address().port);
  await page.evaluate(() => {
    globalThis.done = false;
    import('/contract.mjs').then(({default: create}) => create({
      print: text => { console.log(text); if (text.startsWith('PIPELINE_READINESS ')) globalThis.done = true; },
      printErr: text => console.error(text),
      onAbort: () => { globalThis.done = true; },
    })).catch(error => { console.error(String(error)); globalThis.done = true; });
  });
  await page.waitForFunction(() => globalThis.done, null, {timeout: 60000});
  const line = logs.find(text => text.startsWith('PIPELINE_READINESS '));
  assert.ok(line, 'Compiled production cache must execute');
  const result = JSON.parse(line.slice('PIPELINE_READINESS '.length));
  assert.equal(errors.length, 0);
  assert.equal(result.requested, 8);
  assert.equal(result.created, 8);
  assert.equal(result.unavailableAfterDrain, 0);
  const status = result.unavailableAtDraw === 0 ? 'VERIFIED pipeline availability' : 'REPRODUCED queued pipelines unavailable at draw';
  fs.writeFileSync(path.join(output, 'observation.json'), JSON.stringify({status, scope: 'Real Edge WebGPU and compiled production cache scheduling; not proof of the portrait root cause', result, logs, errors}, null, 2) + '\n');
  console.log(JSON.stringify({status, result, evidence: path.join(output, 'observation.json')}));
  if (result.unavailableAtDraw) process.exitCode = 1;
})().catch(error => {
  fs.writeFileSync(path.join(output, 'failure.json'), JSON.stringify({error: error.stack, logs, errors}, null, 2));
  console.error(error.message); process.exitCode = 1;
}).finally(async () => { await browser?.close(); if (server) await new Promise(resolve => server.close(resolve)); });
