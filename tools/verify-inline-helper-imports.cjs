'use strict';
// External C inline definitions can leave imports that only fail when a scene
// loads. Check actual modules against source definitions before gameplay tests.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const [manifestPath, reportPath] = process.argv.slice(2);
if (!manifestPath) throw new Error('Usage: node verify-inline-helper-imports.cjs PACKAGING_JSON [REPORT_JSON]');
const root = path.resolve(__dirname, '../src/REL');
const helpers = new Map();
function scan(directory) {
  for (const entry of fs.readdirSync(directory, {withFileTypes: true})) {
    const file = path.join(directory, entry.name);
    if (entry.isDirectory()) scan(file);
    else if (entry.isFile() && entry.name.endsWith('.c')) {
      const source = fs.readFileSync(file, 'utf8');
      for (const match of source.matchAll(/^\s*(?:extern\s+)?inline\s+[^;{}\n]*?\b(\w+)\s*\([^;{}]*\)\s*\{/gm)) {
        const locations = helpers.get(match[1]) || [];
        locations.push(file); helpers.set(match[1], locations);
      }
    }
  }
}
scan(root);
const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
if (!manifest.modules?.length) throw new Error('Packaging manifest must contain modules');
const unresolved = [];
for (const item of manifest.modules) {
  const bytes = fs.readFileSync(item.source);
  if (crypto.createHash('sha256').update(bytes).digest('hex') !== item.sha256)
    throw new Error('Module changed since packaging audit: ' + item.module);
  const module = new WebAssembly.Module(bytes);
  for (const symbol of WebAssembly.Module.imports(module)) {
    if (helpers.has(symbol.name)) unresolved.push({module: item.module, symbol: symbol.name, definitions: helpers.get(symbol.name)});
  }
}
const report = {scope: 'source-external-inline-helper-imports', modulesChecked: manifest.modules.length,
  helpersScanned: [...helpers.keys()], unresolved, passed: unresolved.length === 0};
if (reportPath) fs.writeFileSync(reportPath, JSON.stringify(report, null, 2) + '\n');
console.log(JSON.stringify(report));
if (!report.passed) process.exitCode = 1;
