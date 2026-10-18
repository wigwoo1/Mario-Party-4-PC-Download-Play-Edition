// Compare independently captured actual engine frames; no network claim.
const fs=require('node:fs'),assert=require('node:assert/strict');
const [nativePath,browserPath,reportPath]=process.argv.slice(2);
if(!nativePath||!browserPath||!reportPath)throw Error('Native log, browser log and report path required');
const nativeText=fs.readFileSync(nativePath,'utf8'),browserText=fs.readFileSync(browserPath,'utf8');
const native=[...nativeText.matchAll(/Native online frame probe: frame=(\d+) checkpoint=(\d+) frand=(\d+) rand8=(\d+)/g)]
  .map(match=>Object.fromEntries(['frame','checkpoint','frand','rand8'].map((key,i)=>[key,Number(match[i+1])])));
const line=browserText.split(/\r?\n/).find(value=>value.startsWith('Session diagnostic result: '));
assert.ok(line,'Browser diagnostic result missing');
const browser=JSON.parse(line.slice('Session diagnostic result: '.length));
assert.ok(native.length>1,'Native simulation captures missing');
assert.ok(Array.isArray(browser.probeStates),'Browser simulation captures missing');
assert.equal(browser.cleanup,true);assert.equal(browser.released,true);assert.equal(browser.inputReleased,true);
assert.ok(nativeText.includes(`Native online frame probe complete: accepted=${native.length-1} frame=${native.length-1} detached=1`),'Native bounded stop/cleanup missing');
assert.equal(browser.committedFrames,native.length-1);
for(let i=0;i<native.length;i++)assert.equal(native[i].frame,i,'Native capture sequence incomplete');
const differences=[];
for(let i=0;i<Math.max(native.length,browser.probeStates.length);i++){
  const a=native[i],b=browser.probeStates[i];
  if(!a||!b||Object.keys(a).some(key=>a[key]!==b[key]))differences.push({index:i,native:a,browser:b});
}
fs.writeFileSync(reportPath,JSON.stringify({passed:differences.length===0,
  scope:'Actual independently executed native/browser startup frames with diagnostic logical-port inputs; no P2P or complete gameplay claim',
  nativePath,browserPath,frames:native.length-1,native,browser:browser.probeStates,differences},null,2));
assert.equal(differences.length,0,'Actual native/browser frame states differ; see report');
console.log(`PASS actual native/browser states match through ${native.length-1} startup frames`);
