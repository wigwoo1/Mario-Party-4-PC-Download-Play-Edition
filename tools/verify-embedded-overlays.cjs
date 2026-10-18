'use strict';

// Audit actual linked bytes, since replacing an older side module can preserve
// its timestamp and leave Ninja's existing executable unchanged.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const [buildDirectory, reportPath, configuredRuntimePath] = process.argv.slice(2);
if (!buildDirectory) throw new Error('Usage: node verify-embedded-overlays.cjs BUILD_DIRECTORY [REPORT_JSON] [RUNTIME_WASM]');
const hash = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const runtimePath = configuredRuntimePath ? path.resolve(configuredRuntimePath) : path.resolve(buildDirectory, 'browser-runtime/contract.wasm');
const runtime = fs.readFileSync(runtimePath);
const options = fs.readFileSync(path.resolve(buildDirectory, 'setup-link-options.rsp'), 'utf8');
const modules = [];
for (const line of options.split(/\r?\n/)) {
  const match = /^"--embed-file=(.+)@\/[^"\r\n]+"$/.exec(line);
  if (!match || !match[1].endsWith('.wasm')) continue;
  const source = path.resolve(match[1]);
  const bytes = fs.readFileSync(source);
  modules.push({ module: path.basename(source), source, bytes: bytes.length,
    sha256: hash(bytes), embeddedOffset: runtime.indexOf(bytes) });
}
if (!modules.length) throw new Error('No embedded Wasm modules found in link options');
const report = { runtimePath, runtimeSha256: hash(runtime), modules,
  allEmbedded: modules.every(module => module.embeddedOffset >= 0) };
if (reportPath) fs.writeFileSync(reportPath, JSON.stringify(report, null, 2) + '\n');
for (const module of modules.filter(module => module.embeddedOffset < 0)) {
  console.error(`Missing current embedded bytes: ${module.module} (${module.source})`);
}
console.log(`${modules.length} configured modules; all embedded: ${report.allEmbedded}; runtime SHA256: ${report.runtimeSha256}`);
if (!report.allEmbedded) process.exitCode = 1;
