import {CHUNK, digest, validateManifest} from './transfer.mjs';
import {createVerifiedAssetFile} from './verified-asset-file.mjs';

// Explicit pre-mount boundary. A completed transfer marker is necessary but
// never sufficient: storage may have changed since transfer verification.
export async function verifiedAssetBlob(store, id, {signal, progress = () => {}, createFile=createVerifiedAssetFile} = {}) {
  signal?.throwIfAborted();
  if (typeof id !== 'string' || !/^[a-f0-9]{64}$/.test(id)) throw Error('Invalid asset identity.');
  const saved = await store.manifest(id);
  signal?.throwIfAborted();
  if (!saved) throw Error('Asset download is not complete.');
  const manifest = {...validateManifest(saved)};
  if (manifest.id !== id) throw Error('Stored asset identity mismatch.');
  // A local File already has immutable backing storage. Verify that exact
  // snapshot and mount it, avoiding a second full asset of copied Blob parts.
  const snapshot=store.immutableBlob;
  if (snapshot !== undefined && (!(snapshot instanceof Blob) || snapshot.size !== manifest.size))
    throw Error('Matching immutable asset snapshot required.');
  const hashes = new Uint8Array(manifest.count * 32), parts = [];
  const file=!snapshot&&manifest.size>=32*1024*1024 ? await createFile() : undefined;
  try {
  signal?.throwIfAborted();
  let bytesRead = 0;
  // Bound outstanding hashes and raw snapshots to four chunks. Storage reads
  // remain sequential so cancellation while a read is pending starts no more.
  for (let start = 0; start < manifest.count; start += 4) {
    const batch = [];
    for (let index = start; index < Math.min(start + 4, manifest.count); ++index) {
      signal?.throwIfAborted();
      const chunk = snapshot
        ? {bytes:await snapshot.slice(index * CHUNK, Math.min((index + 1) * CHUNK, manifest.size)).arrayBuffer()}
        : await store.get(id, index);
      signal?.throwIfAborted();
      const expected = Math.min(CHUNK, manifest.size - index * CHUNK);
      if (!(chunk?.bytes instanceof ArrayBuffer) || chunk.bytes.byteLength !== expected)
        throw Error('Asset storage is incomplete or damaged.');
      const bytes = chunk.bytes.slice(0);
      batch.push({index, bytes});
      // Keep immutable Blob parts, rather than a whole asset of raw buffers.
      if (!snapshot && !file) parts.push(new Blob([bytes]));
    }
    const digests = await Promise.all(batch.map(chunk => digest(chunk.bytes)));
    signal?.throwIfAborted();
    for (let offset = 0; offset < batch.length; ++offset) {
      hashes.set(digests[offset], batch[offset].index * 32);
      if(file)await file.write(batch[offset].bytes);
      signal?.throwIfAborted();
      bytesRead += batch[offset].bytes.byteLength;
      progress(bytesRead, manifest.size);
      signal?.throwIfAborted();
    }
  }
  const actual = Array.from(await digest(hashes), b => b.toString(16).padStart(2, '0')).join('');
  signal?.throwIfAborted();
  if (actual !== id) throw Error('Stored asset integrity check failed.');
  const blob=snapshot || (file ? await file.finish() : new Blob(parts, {type:'application/octet-stream'}));
  signal?.throwIfAborted();
  return {manifest, blob, ...(file ? {dispose:file.dispose} : {})};
  } catch(error) {
    if(file)try{await file.dispose();}catch(cleanup){throw new AggregateError([error,cleanup],'Asset verification and file cleanup failed.');}
    throw error;
  }
}
