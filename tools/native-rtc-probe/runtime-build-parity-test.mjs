import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import path from 'node:path';
import {spawnSync} from 'node:child_process';
import {fingerprintRuntimeFiles} from '../../web/runtime-build.mjs';
import {validateSessionConfiguration} from '../../web/session-config.mjs';
const executable=path.resolve(process.env.PARTYBOARD_RUNTIME_BUILD_CODEC);
const directory=path.resolve('work/runtime-build-parity-fixtures');
await fs.mkdir(directory,{recursive:true});
const fixtures=[{name:'native/partyboard.exe',bytes:Uint8Array.from([0,255,128,13,10,0])},
 {name:'contract.wasm',bytes:Uint8Array.from({length:131077},(_,i)=>i%251)},
 {name:'contract.mjs',bytes:new TextEncoder().encode('export default function(){}\n')}];
const files=[];
for(let i=0;i<fixtures.length;i++){const fixture=fixtures[i],file=path.join(directory,String(i));await fs.writeFile(file,fixture.bytes);files.push({name:fixture.name,path:file});}
const native=input=>spawnSync(executable,[],{input:JSON.stringify(input),encoding:'utf8'});
const browser=await fingerprintRuntimeFiles(fixtures.map(({name,bytes})=>({name,blob:new Blob([bytes])})));
const measured=native(files);assert.equal(measured.status,0,measured.stderr);assert.deepEqual(JSON.parse(measured.stdout),browser);
assert.deepEqual(JSON.parse(native([...files].reverse()).stdout),browser);
assert.deepEqual(JSON.parse(native({files,expectedBuildId:browser.buildId}).stdout),browser);
const session='1'.repeat(32),assetId='a'.repeat(64);
const freshInput={files,expectedBuildId:browser.buildId,assetId,newHostSession:true};
const fresh=[];
for(let i=0;i<2;i++){
 const result=native(freshInput);assert.equal(result.status,0,result.stderr);
 const configuration=JSON.parse(result.stdout).configuration;
 assert.deepEqual(validateSessionConfiguration(configuration,assetId),configuration);
 assert.equal(configuration.buildId,browser.buildId);fresh.push(configuration);
}
assert.notEqual(fresh[0].session,fresh[1].session);
for(const override of [{session},{seed:0}])assert.equal(native({...freshInput,...override}).status,1);
for(const absent of ['contract.mjs','contract.wasm']){
 const incomplete=files.filter(file=>file.name!==absent);
 const pin=JSON.parse(native(incomplete).stdout).buildId;
 const rejected=native({files:incomplete,expectedBuildId:pin,session,assetId,seed:0});
 assert.equal(rejected.status,1);assert.match(rejected.stderr,/requires browser wrapper and Wasm/);
}
const digest=async bytes=>new Uint8Array(await crypto.subtle.digest('SHA-256',bytes));
const assetHash=Array.from(await digest(await digest(fixtures[0].bytes)),byte=>byte.toString(16).padStart(2,'0')).join('');
const assetInput={files,expectedBuildId:browser.buildId,session,assetId:assetHash,seed:0,assetPath:files[0].path};
const measuredAsset=native(assetInput);assert.equal(measuredAsset.status,0,measuredAsset.stderr);
assert.equal(JSON.parse(measuredAsset.stdout).configuration.assetId,assetHash);
assert.equal(native({...assetInput,assetId:'0'.repeat(64)}).status,1);
for(const seed of [0,4294967295]){
 const result=native({files,expectedBuildId:browser.buildId,session,assetId,seed});
 assert.equal(result.status,0,result.stderr);
 const {configuration}=JSON.parse(result.stdout);
 assert.deepEqual(validateSessionConfiguration(configuration,assetId),configuration);
 assert.equal(configuration.buildId,browser.buildId);assert.equal(configuration.seed,seed);
}
for(const override of [{session:'0'.repeat(32)},{assetId:'invalid'},{seed:-1},{seed:4294967296},{seed:1.5}])
 assert.equal(native({files,expectedBuildId:browser.buildId,session,assetId,seed:0,...override}).status,1);
for(const expectedBuildId of ['',null,'X'.repeat(64),'0'.repeat(64)])assert.equal(native({files,expectedBuildId}).status,1);
for(const input of [[],[...files,files[0]],[{name:'../bad',path:files[0].path}],[{name:'native/./bad',path:files[0].path}],[{name:'bad name',path:files[0].path}]])assert.equal(native(input).status,1);
const empty=path.join(directory,'empty');await fs.writeFile(empty,'');assert.equal(native([{name:'empty',path:empty}]).status,1);
await fs.appendFile(files[0].path,Uint8Array.of(17));assert.notEqual(JSON.parse(native(files).stdout).buildId,browser.buildId);
// Accept the changed package pin so this rejection specifically exercises asset
// identity drift rather than failing earlier on the runtime package measurement.
const changedBuild=JSON.parse(native(files).stdout).buildId;
assert.equal(native({...assetInput,expectedBuildId:changedBuild}).status,1);
assert.equal(native({files,expectedBuildId:browser.buildId}).status,1);
const report={scope:'Native MbedTLS and production browser canonical artifact fingerprint interoperability; supplied fixtures only',
 buildId:browser.buildId,canonicalManifestIdentical:true,orderIndependent:true,invalidNamesDuplicatesAndEmptyRejected:true,changedArtifactChangesIdentity:true,
 loadedRuntimeAttestationVerified:false,crossPlatformCompatibilityVerified:false,productionStartVerified:false};
await fs.writeFile('outputs/validation/native-browser-runtime-build-parity-20261005.json',JSON.stringify(report,null,2)+'\n');
console.log('PASS native/browser canonical runtime manifest and build identity parity.');
