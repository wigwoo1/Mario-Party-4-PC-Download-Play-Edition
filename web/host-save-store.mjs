// Host-owned saves only. Guest session snapshots must not be committed here.
const MAX_BYTES = 32 * 1024 * 1024;
const hash = async bytes => Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256', bytes)), n => n.toString(16).padStart(2, '0')).join('');
function identity(assetId, slot) {
  if (typeof assetId !== 'string' || !/^[a-f0-9]{64}$/.test(assetId) || !Number.isInteger(slot) || slot < 0 || slot > 1)
    throw Error('Invalid save identity.');
  return `${assetId}:${slot}`;
}
function checkedBytes(bytes) {
  if (!(bytes instanceof Uint8Array) || bytes.length < 8192 || bytes.length > MAX_BYTES || bytes.length % 8192)
    throw Error('Invalid card snapshot size.');
  return bytes.slice();
}
export async function openHostSaveStore({name = 'partyboard-host-saves-v1'} = {}) {
  const db = await new Promise((resolve, reject) => {
    const request = indexedDB.open(name, 1);
    request.onupgradeneeded = () => request.result.createObjectStore('cards', {keyPath: 'key'});
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
    request.onblocked = () => reject(Error('Save storage is blocked by another page.'));
  });
  db.onversionchange = () => db.close();
  return {
    close: () => db.close(),
    async load(assetId, slot) {
      const key = identity(assetId, slot);
      const record = await new Promise((resolve, reject) => {
        const transaction = db.transaction('cards', 'readonly');
        const request = transaction.objectStore('cards').get(key);
        transaction.oncomplete = () => resolve(request.result);
        transaction.onabort = () => reject(transaction.error || Error('Save read aborted.'));
      });
      if (!record) return null;
      if (record.version !== 1 || record.key !== key || !Number.isSafeInteger(record.revision) || record.revision < 1)
        throw Error('Invalid stored save metadata.');
      const bytes = checkedBytes(record.bytes);
      if (await hash(bytes) !== record.sha256) throw Error('Stored save integrity check failed.');
      return {bytes, revision: record.revision, sha256: record.sha256};
    },
    async commit(assetId, slot, bytes, {expectedRevision, role} = {}) {
      // Caller policy guard, not network authentication. The session layer
      // must establish who owns the save before invoking this API.
      if (role !== 'host') throw Error('Only the host may persist a session save.');
      const key = identity(assetId, slot), snapshot = checkedBytes(bytes);
      if (!Number.isSafeInteger(expectedRevision) || expectedRevision < 0 || expectedRevision >= Number.MAX_SAFE_INTEGER)
        throw Error('Invalid save revision.');
      const sha256 = await hash(snapshot);
      const revision = expectedRevision + 1;
      await new Promise((resolve, reject) => {
        const transaction = db.transaction('cards', 'readwrite');
        const cards = transaction.objectStore('cards');
        let failure;
        cards.get(key).onsuccess = event => {
          const previous = event.target.result;
          if ((previous?.revision ?? 0) !== expectedRevision) {
            failure = Error('Save changed in another session; refusing stale overwrite.');
            transaction.abort();
            return;
          }
          cards.put({key, version: 1, revision, sha256, bytes: snapshot});
        };
        transaction.oncomplete = resolve;
        transaction.onabort = () => reject(failure || transaction.error || Error('Save write aborted.'));
      });
      return {revision, sha256};
    }
  };
}
