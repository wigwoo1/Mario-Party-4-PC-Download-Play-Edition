'use strict';
const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),os=require('node:os');
const {serverListenConfiguration,listeningUrls}=require('./server-listen.cjs');
test('default remains loopback HTTP and no certificate is read',()=>{
 const c=serverListenConfiguration({});assert.equal(c.host,'127.0.0.1');assert.equal(c.scheme,'http');assert.equal(c.loopback,true);assert.equal(c.tls,undefined);
 assert.deepEqual(listeningUrls(c,8784),['http://127.0.0.1:8784']);
});
test('remote plaintext binding is rejected, including wildcard addresses',()=>{
 for(const host of ['0.0.0.0','::','192.168.1.10'])assert.throws(()=>serverListenConfiguration({PARTYBOARD_LISTEN_ADDRESS:host}),/requires HTTPS/);
 for(const host of ['localhost','::1','127.0.0.2'])assert.equal(serverListenConfiguration({PARTYBOARD_LISTEN_ADDRESS:host}).loopback,true);
});
test('reject incomplete TLS configuration and non-IP listening names',()=>{
 assert.throws(()=>serverListenConfiguration({PARTYBOARD_TLS_CERTIFICATE:'missing'}),/both/);
 assert.throws(()=>serverListenConfiguration({PARTYBOARD_TLS_KEY:'missing'}),/both/);
 for(const host of ['example.com','https://127.0.0.1','127.0.0.1:8784','999.1.1.1'])assert.throws(()=>serverListenConfiguration({PARTYBOARD_LISTEN_ADDRESS:host}),/literal IP/);
});
test('TLS configuration reads bounded external files without returning their paths',t=>{
 const temporary=fs.mkdtempSync(path.join(os.tmpdir(),'partyboard-tls-'));
 t.after(()=>{if(path.dirname(temporary)!==os.tmpdir()||!path.basename(temporary).startsWith('partyboard-tls-'))throw Error('Unexpected cleanup directory');fs.rmSync(temporary,{recursive:true,force:true});});
 const certificate=path.join(temporary,'cert.pem'),key=path.join(temporary,'key.pem');
 fs.writeFileSync(certificate,'fixture cert');fs.writeFileSync(key,'fixture key');
 const env={PARTYBOARD_LISTEN_ADDRESS:'0.0.0.0',PARTYBOARD_TLS_CERTIFICATE:certificate,PARTYBOARD_TLS_KEY:key};
 const config=serverListenConfiguration(env);assert.equal(config.scheme,'https');assert.equal(config.loopback,false);assert.equal(config.tls.key.toString(),'fixture key');
 fs.writeFileSync(key,Buffer.alloc(1024*1024+1));assert.throws(()=>serverListenConfiguration(env),/under 1 MiB/);
 fs.writeFileSync(key,'');assert.throws(()=>serverListenConfiguration(env),/nonempty/);
});
test('wildcard startup URLs enumerate only matching real network addresses',()=>{
 const interfaces={wifi:[{address:'192.168.1.8',family:'IPv4',internal:false},{address:'fe80::1%wifi',family:'IPv6',internal:false}],loopback:[{address:'127.0.0.1',family:'IPv4',internal:true}]};
 assert.deepEqual(listeningUrls({host:'0.0.0.0',scheme:'https'},8932,interfaces),['https://192.168.1.8:8932']);
 assert.deepEqual(listeningUrls({host:'::',scheme:'https'},8932,interfaces),['https://192.168.1.8:8932','https://[fe80::1%25wifi]:8932']);
 assert.deepEqual(listeningUrls({host:'::1',scheme:'https'},8932),['https://[::1]:8932']);
});
