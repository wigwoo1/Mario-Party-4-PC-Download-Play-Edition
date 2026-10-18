import fs from 'node:fs';import path from 'node:path';import assert from 'node:assert/strict';
import {validateManifest,decodeChunk,digest} from '../../web/transfer.mjs';
const directory=process.argv[2];const manifest=validateManifest(JSON.parse(fs.readFileSync(path.join(directory,'manifest.json'),'utf8')));
const hashes=new Uint8Array(manifest.count*32);let total=0;
for(let index=0;index<manifest.count;index++){
 const stored=fs.readFileSync(path.join(directory,index+'.bin'));
 const buffer=stored.buffer.slice(stored.byteOffset,stored.byteOffset+stored.byteLength);
 const decoded=await decodeChunk(buffer,manifest,index);
 for(const [offset,byte] of new Uint8Array(decoded.bytes).entries())assert.equal(byte,((total+offset)*23)&255);
 hashes.set(decoded.hash,index*32);total+=decoded.bytes.byteLength;
 const corrupt=buffer.slice(0);new Uint8Array(corrupt)[40]^=1;
 await assert.rejects(()=>decodeChunk(corrupt,manifest,index),/integrity/);
 await assert.rejects(()=>decodeChunk(buffer,manifest,index+1),/range/);
}
assert.equal(total,50000);assert.equal(Buffer.from(await digest(hashes)).toString('hex'),manifest.id);
console.log('PASS actual browser transfer decoder accepts native chunk headers, SHA256 and whole-asset identity; rejects corruption/order. Codec fixture only.');
