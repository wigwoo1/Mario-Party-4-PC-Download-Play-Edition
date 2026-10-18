// Versioned, bounded asset transport. Independent of the unfinished WASM runtime.
export const CHUNK = 16 * 1024 - 40;
export const WINDOW = 16;
export const MAX_BYTES = 2 * 1024 ** 3;
const hex = bytes => Array.from(bytes, b => b.toString(16).padStart(2, '0')).join('');
export async function digest(data) {
  return new Uint8Array(await crypto.subtle.digest('SHA-256', data));
}
export async function prepare(file, progress = () => {}, signal) {
  if (!Number.isSafeInteger(file.size) || file.size < 1 || file.size > MAX_BYTES)
    throw Error('Select a file between 1 byte and 2 GiB.');
  const count = Math.ceil(file.size / CHUNK);
  const hashes = new Uint8Array(count * 32);
  // Bound temporary reads to eight transport chunks. Store results by original
  // index so completion order cannot change the manifest or resumable identity.
  for (let first = 0; first < count; first += 8) {
    signal?.throwIfAborted();
    const batch = await Promise.all(Array.from({length:Math.min(8,count-first)},async (_,offset) => {
      const index=first+offset;
      const bytes=await file.slice(index*CHUNK,(index+1)*CHUNK).arrayBuffer();
      signal?.throwIfAborted();
      return digest(bytes);
    }));
    signal?.throwIfAborted();
    batch.forEach((hash,offset)=>hashes.set(hash,(first+offset)*32));
    if (first % 256 === 0) progress(first / count);
  }
  const id=hex(await digest(hashes));
  signal?.throwIfAborted();
  progress(1);
  signal?.throwIfAborted();
  return { manifest: {type:'manifest', version:1, id,
    size:file.size, chunk:CHUNK, count, name:(file.name || 'asset').slice(0, 160)}, hashes, file };
}
export function validateManifest(m) {
  if (!m || m.type !== 'manifest' || m.version !== 1 || m.chunk !== CHUNK ||
      !Number.isSafeInteger(m.size) || m.size < 1 || m.size > MAX_BYTES ||
      m.count !== Math.ceil(m.size / CHUNK) || !/^[a-f0-9]{64}$/.test(m.id) ||
      typeof m.name !== 'string' || m.name.length > 160) throw Error('Invalid asset manifest.');
  return m;
}
export function encodeChunk(index, hash, bytes) {
  const packet = new Uint8Array(40 + bytes.byteLength);
  const view = new DataView(packet.buffer);
  view.setUint32(0, index); view.setUint32(4, bytes.byteLength);
  packet.set(hash, 8); packet.set(new Uint8Array(bytes), 40);
  return packet.buffer;
}
export async function decodeChunk(packet, manifest, expected) {
  if (!(packet instanceof ArrayBuffer) || packet.byteLength < 40 || packet.byteLength > CHUNK + 40)
    throw Error('Invalid chunk packet.');
  const view = new DataView(packet);
  const index = view.getUint32(0), size = view.getUint32(4);
  if (index !== expected || index >= manifest.count || size !== packet.byteLength - 40 ||
      size !== Math.min(CHUNK, manifest.size - index * CHUNK)) throw Error('Unexpected chunk range.');
  const bytes = packet.slice(40), hash = await digest(bytes);
  if (hex(hash) !== hex(new Uint8Array(packet, 8, 32))) throw Error('Chunk integrity check failed.');
  return {index, bytes, hash};
}
function parseControl(data) {
  if (typeof data !== 'string' || data.length > 2048) throw Error('Invalid control message.');
  return JSON.parse(data);
}
function wire(channel, handler, onError) {
  channel.binaryType = 'arraybuffer';
  let chain = Promise.resolve(), failed = false, pending = 0;
  channel.addEventListener('message', event => {
    if (failed) return;
    if (++pending > WINDOW + 2) {
      failed = true; channel.close(); onError(Error('Peer exceeded the receive window.')); return;
    }
    chain = chain.then(() => { if (!failed) return handler(event.data); }).finally(() => { pending--; }).catch(error => {
      failed = true; channel.close(); onError(error);
    });
  });
}
export function host(channel, asset, onError = console.error, onComplete = () => {}, onProgress = () => {}) {
  let next = null, stopped = false;
  const sendManifest = () => {if(!stopped&&channel.readyState==='open')channel.send(JSON.stringify(asset.manifest));};
  if (channel.readyState === 'open') sendManifest();
  else channel.addEventListener('open', sendManifest, {once:true});
  channel.addEventListener('close', () => { stopped = true; });
  wire(channel, async data => {
    if (stopped) return;
    const message = parseControl(data);
    if (message.type === 'complete') {
      if (message.id !== asset.manifest.id || (next !== null && next !== asset.manifest.count))
        throw Error('Invalid completion acknowledgement.');
      onProgress(asset.manifest.size, asset.manifest.size);
      if (stopped) return;
      onComplete(); return;
    }
    if (message.type !== 'get' || !Number.isInteger(message.start) || message.start < 0 ||
        message.start >= asset.manifest.count || !Number.isInteger(message.count) ||
        message.count < 1 || message.count > WINDOW ||
        message.start + message.count > asset.manifest.count ||
        (next !== null && message.start !== next)) throw Error('Invalid transfer request.');
    // The next request acknowledges a verified, persisted prefix on the guest.
    onProgress(Math.min(message.start * CHUNK, asset.manifest.size), asset.manifest.size);
    next = message.start + message.count;
    for (let i = message.start; i < next; i++) {
      if (stopped) return;
      const bytes = await asset.file.slice(i * CHUNK, (i + 1) * CHUNK).arrayBuffer();
      if (stopped) return;
      channel.send(encodeChunk(i, asset.hashes.subarray(i * 32, i * 32 + 32), bytes));
    }
  }, onError);
}
// Store contract: get(id,index), put(id,index,{bytes,hash}), complete(manifest).
// Each put must commit before resolving, making interrupted transfers resumable.
export function guest(channel, store, callbacks = {}) {
  let manifest, accepted = false, next = 0, end = 0, hashes, stopped = false;
  const progress = callbacks.progress || (() => {});
  channel.addEventListener('close', () => { stopped = true; });
  const request = () => {
    if (stopped) return;
    end = Math.min(next + WINDOW, manifest.count);
    channel.send(JSON.stringify({type:'get', start:next, count:end-next}));
  };
  const finish = async () => {
    if (hex(await digest(hashes)) !== manifest.id) throw Error('Whole-asset integrity check failed.');
    if (stopped) return;
    await store.complete(manifest);
    if (stopped) return;
    channel.send(JSON.stringify({type:'complete',id:manifest.id}));
    callbacks.complete?.(manifest);
  };
  const accept = async () => {
    if (stopped) return;
    if (!manifest || accepted) throw Error('No pending asset offer.');
    accepted = true;
    hashes = new Uint8Array(manifest.count * 32);
    // Recheck cached bytes rather than trusting saved integrity flags.
    for (; next < manifest.count; next++) {
      if (stopped) return;
      const cached = await store.get(manifest.id, next);
      if (stopped) return;
      if (!cached) break;
      const expected = Math.min(CHUNK, manifest.size - next * CHUNK);
      if (cached.bytes.byteLength !== expected) break;
      const actual = await digest(cached.bytes);
      if (stopped) return;
      if (hex(actual) !== hex(cached.hash)) break;
      hashes.set(cached.hash, next * 32);
    }
    if (stopped) return;
    progress(Math.min(next * CHUNK, manifest.size), manifest.size);
    if (next === manifest.count) await finish(); else request();
  };
  wire(channel, async data => {
    if (stopped) return;
    if (typeof data === 'string') {
      if (manifest) throw Error('Duplicate asset offer.');
      manifest = validateManifest(parseControl(data));
      callbacks.offer?.(manifest);
      return;
    }
    if (!accepted || stopped || next >= end) throw Error('Unrequested asset data.');
    const chunk = await decodeChunk(data, manifest, next);
    if (stopped) return;
    await store.put(manifest.id, next, chunk);
    if (stopped) return;
    hashes.set(chunk.hash, next * 32);
    next++;
    progress(Math.min(next * CHUNK, manifest.size), manifest.size);
    if (next === manifest.count) await finish(); else if (next === end) request();
  }, callbacks.error || console.error);
  return { accept, cancel() { stopped = true; channel.close(); } };
}

export async function browserStore() {
  const db = await new Promise((resolve, reject) => {
    const request = indexedDB.open('partyboard-assets-v1', 1);
    request.onupgradeneeded = () => {
      request.result.createObjectStore('chunks');
      request.result.createObjectStore('manifests');
    };
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
  const run = (name, mode, operation) => new Promise((resolve, reject) => {
    const tx = db.transaction(name, mode), request = operation(tx.objectStore(name));
    tx.oncomplete = () => resolve(request?.result);
    tx.onerror = tx.onabort = () => reject(tx.error || Error('Browser storage transaction failed.'));
  });
  return {
    get: (id,index) => run('chunks','readonly',s=>s.get([id,index])),
    put: (id,index,value) => run('chunks','readwrite',s=>s.put(value,[id,index])),
    complete: m => run('manifests','readwrite',s=>s.put(m,m.id)),
    manifest: id => run('manifests','readonly',s=>s.get(id)),
    remove: id => {
      if(typeof id!=='string'||!/^[a-f0-9]{64}$/.test(id))return Promise.reject(TypeError('Valid downloaded game identifier required.'));
      return new Promise((resolve,reject)=>{
        const tx=db.transaction(['chunks','manifests'],'readwrite');
        tx.oncomplete=()=>resolve();
        tx.onerror=tx.onabort=()=>reject(tx.error||Error('Download removal failed.'));
        try{
          tx.objectStore('chunks').delete(IDBKeyRange.bound([id,0],[id,Number.MAX_SAFE_INTEGER]));
          tx.objectStore('manifests').delete(id);
        }catch(error){tx.abort();reject(error);}
      });
    },
    close: () => db.close(),
  };
}
