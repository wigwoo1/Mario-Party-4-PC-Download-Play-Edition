'use strict';

// Execute the actual side module: checking C source alone missed a stale Wasm
// module. This isolates menu decisions; it does not execute rendering, process
// switching, the complete results screen, or an online match.
const fs = require('node:fs');
const assert = require('node:assert/strict');
const crypto = require('node:crypto');

function observeResultsMenu(bytes) {
  const module = new WebAssembly.Module(bytes);
  const cases = [];
  for (const port of [-1, 0, 1, 2, 3]) {
    const memory = new WebAssembly.Memory({initial: 256});
    const imports = {env: {}, 'GOT.mem': {}, 'GOT.func': {}};
    const memoryBase = 0x10000;
    let address = 0x400000, ticks = 0, soundCalls = 0;
    for (const entry of WebAssembly.Module.imports(module)) {
      assert.ok(Object.hasOwn(imports, entry.module), 'Unexpected import namespace');
      let value;
      if (entry.kind === 'memory') value = memory;
      else if (entry.kind === 'table') value = new WebAssembly.Table({initial: 4096, element: 'anyfunc'});
      else if (entry.kind === 'global') {
        const initial = entry.name === '__stack_pointer' ? 0xe00000
          : entry.name === '__memory_base' ? memoryBase
          : entry.name === '__table_base' ? 1
          : entry.module.startsWith('GOT') ? (address += 65536) : 0;
        value = new WebAssembly.Global({value: 'i32',
          mutable: !['__memory_base', '__table_base'].includes(entry.name)}, initial);
      } else if (entry.kind === 'function') {
        if (entry.name === 'HuPrcVSleep') value = () => {
          assert.ok(++ticks <= 350, 'Results-menu decision exceeded its bounded wait');
          if (port >= 0 && ticks === 3) {
            new DataView(memory.buffer).setUint16(imports['GOT.mem'].HuPadBtnDown.value + port * 2, 0x1000, true);
          }
        };
        else if (entry.name === 'HuAudFXPlay') value = () => { soundCalls++; return 0; };
        else value = () => { throw Error('Unexpected dependency in isolated results-menu decision: ' + entry.name); };
      } else throw Error('Unexpected import kind: ' + entry.kind);
      imports[entry.module][entry.name] = value;
    }
    const instance = new WebAssembly.Instance(module, imports);
    instance.exports.__wasm_apply_data_relocs();
    // Exported data symbols in a side module are relative to __memory_base.
    const state = instance.exports.lbl_1_bss_1A0C.value + memoryBase;
    const view = new DataView(memory.buffer);
    view.setInt32(state + 0x14, port < 0 ? -1 : port, true);
    view.setInt32(state + 0x04, 0, true);
    view.setInt32(state + 0x38 + 0x14, Math.max(0, port), true);
    const result = instance.exports.fn_1_16924();
    assert.equal(ticks, port < 0 ? 301 : 3);
    assert.equal(soundCalls, port < 0 ? 0 : 1);
    cases.push({port, result, ticks, soundCalls});
  }
  return {
    scope: 'Actual Wasm side-module decision with bounded input/audio stubs; not full results execution or multiplayer return',
    moduleSha256: crypto.createHash('sha256').update(bytes).digest('hex'), cases};
}

function auditResultsMenu(bytes) {
  const report = observeResultsMenu(bytes);
  for (const {port, result} of report.cases) assert.equal(result, 1,
    'Compiled results-menu must return the leave-results action for ' + (port < 0 ? 'CPU auto advance' : 'Start on port ' + port));
  return {status: 'VERIFIED compiled results-menu Start/CPU-auto decisions', ...report};
}

if (require.main === module) {
  const [file, report] = process.argv.slice(2);
  assert.ok(file, 'Pass mstory3Dll.wasm and optionally a report path');
  const result = auditResultsMenu(fs.readFileSync(file));
  if (report) fs.writeFileSync(report, JSON.stringify(result, null, 2) + '\n', {flag: 'wx'});
  console.log(JSON.stringify(result));
}
module.exports = {auditResultsMenu, observeResultsMenu};
