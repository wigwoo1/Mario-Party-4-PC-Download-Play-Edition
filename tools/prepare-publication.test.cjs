'use strict';
const test=require('node:test'),assert=require('node:assert/strict');
const {allowed,safeRelative,secretMatches}=require('./prepare-publication.cjs');
test('publication excludes game images, credentials, builds and personal profiles',()=>{
 for(const name of ['game.ciso','orig/game.iso','disc.gcm','game.rvz','runtime/game.exe','secret.pem','signing.key','.env','.env.local','src/.git/config','work/a.c','profiles/user.json','node_modules/package/a.js','build/game.c'])assert.equal(allowed(name),false,name);
});
test('publication retains project and modified dependency source and notices',()=>{
 for(const name of ['src/game/main.c','extern/aurora/lib/webgpu/gpu.cpp','extern/musyx/LICENSE','licenses/dependency-notices.json','web/app.mjs','tools/prepare-publication.cjs','res/logo.png'])assert.equal(allowed(name),true,name);
});
test('publication rejects traversal, Windows drive-relative paths and network roots',()=>{
 for(const name of ['../a','src/../../key','src\\..\\key','C:secret','C:/secret','\\\\host\\share','/root','src/./key','a\0b',''])assert.equal(safeRelative(name),false,name);
});
test('credential scan rejects recognizable tokens and private keys without returning their values',()=>{
 for(const value of ['ghp_'+'a'.repeat(36),'github_pat_'+'z'.repeat(40),'AKIA'+'A'.repeat(16),'-----BEGIN '+'PRIVATE KEY-----'])assert.equal(secretMatches(Buffer.from(value)),true);
 assert.equal(secretMatches(Buffer.from('sha256: '+'a'.repeat(64))),false);
 assert.equal(secretMatches(Buffer.from('const password = process.env.EXAMPLE_PASSWORD;')),false);
});
