// Initialize before CARDInit. The session owner supplies the verified asset ID
// and the exact configured raw-card path; this does not authorize a network peer.
export async function prepareHostCard({store, assetId, slot, path, FS, role}) {
  if (role !== 'host') throw Error('Only the host may attach persistent card storage.');
  if (typeof path !== 'string' || !path.startsWith('/')) throw Error('Expected an absolute card path.');
  const saved = await store.load(assetId, slot);
  let revision = saved?.revision ?? 0;
  if (saved) FS.writeFile(path, saved.bytes);
  let pending = Promise.resolve(), failure, closed = false;
  return {
    // Call only after the game finishes both payload and metadata writes.
    // Copy synchronously: subsequent game writes must not alter this snapshot.
    completedSave() {
      if (closed) throw Error('Card persistence is closed.');
      if (failure) return Promise.reject(failure);
      let snapshot;
      try { snapshot = FS.readFile(path).slice(); }
      catch (error) { failure = error; return Promise.reject(error); }
      const operation = pending.then(async () => {
        if (failure) throw failure;
        const result = await store.commit(assetId, slot, snapshot, {expectedRevision: revision, role});
        revision = result.revision;
        return result;
      });
      // Observe rejection internally while also returning it to the UI. A failed
      // commit stops later writes rather than silently overwriting a conflict.
      pending = operation.catch(error => { failure = error; });
      return operation;
    },
    async flush() {
      await pending;
      if (failure) throw failure;
      return revision;
    },
    async close() {
      closed = true;
      await this.flush();
    }
  };
}

// Await this before creating the Emscripten module, then pass preRun and
// onCardSaveCompleted in its options. Storage is caller-owned and stays open.
export async function prepareHostSaveSession(options) {
  let runtime, restored, failure, pending = 0, closed = false;
  const adapter = await prepareHostCard({...options, FS: {
    writeFile: (_path, bytes) => { restored = bytes.slice(); },
    readFile: path => {
      if (!runtime) throw Error('Card runtime has not started.');
      return runtime.FS.readFile(path);
    }
  }});
  const completions = new Set();
  return {
    preRun(module) {
      if (closed || runtime) throw Error('Card session cannot be attached twice or after closing.');
      if (restored) module.FS.writeFile(options.path, restored);
      restored = undefined;
      runtime = module;
    },
    onCardSaveCompleted(slot) {
      // This callback is invoked by C: contain errors and surface them through
      // status/flush instead of unwinding through the game scheduler.
      if (failure) return;
      try {
        if (closed || !runtime) throw Error('Card session is not running.');
        if (slot !== options.slot) throw Error('Unexpected save slot.');
        const operation = adapter.completedSave();
        pending++;
        const observed = operation.catch(error => { failure = error; }).finally(() => {
          pending--;
          completions.delete(observed);
        });
        completions.add(observed);
      } catch (error) { failure = error; }
    },
    get status() {
      return {state: failure ? 'error' : closed ? 'closed' : pending ? 'saving' : runtime ? 'ready' : 'prepared',
        pending, error: failure ? String(failure.message || failure) : null};
    },
    async flush() {
      await Promise.all([...completions]);
      if (failure) throw failure;
      return adapter.flush();
    },
    async close() {
      closed = true;
      await Promise.all([...completions]);
      await adapter.close();
      if (failure) throw failure;
    }
  };
}

// The lobby must agree on asset identity and snapshot hash before calling this.
// Hash validation detects damage/mismatch; it does not authenticate the host.
// Guests receive no persistent store handle: all game writes stay in MEMFS.
export async function prepareGuestSaveSession({bytes, sha256, assetId, expectedAssetId, slot, path}) {
  if (typeof assetId !== 'string' || !/^[a-f0-9]{64}$/.test(assetId) || assetId !== expectedAssetId)
    throw Error('Guest save asset identity mismatch.');
  if (!Number.isInteger(slot) || slot < 0 || slot > 1 || typeof path !== 'string' || !path.startsWith('/'))
    throw Error('Invalid guest card destination.');
  if (!(bytes instanceof Uint8Array) || bytes.length < 8192 || bytes.length > 32 * 1024 * 1024 || bytes.length % 8192)
    throw Error('Invalid guest card size.');
  let snapshot = bytes.slice();
  const digest = Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256', snapshot)), n => n.toString(16).padStart(2, '0')).join('');
  if (digest !== sha256) throw Error('Guest save integrity check failed.');
  let attached = false, closed = false, failure;
  return {
    preRun(module) {
      if (attached || closed) throw Error('Guest card session cannot be reused.');
      module.FS.writeFile(path, snapshot);
      snapshot = undefined;
      attached = true;
    },
    onCardSaveCompleted(savedSlot) {
      if (!attached || closed || savedSlot !== slot) failure = Error('Unexpected guest save callback.');
      // Deliberately no persistence: the game has already written its session FS.
    },
    get status() {
      return {state: failure ? 'error' : closed ? 'closed' : attached ? 'ready' : 'prepared',
        pending: 0, persistent: false, error: failure?.message ?? null};
    },
    async flush() { if (failure) throw failure; },
    async close() { closed = true; snapshot = undefined; if (failure) throw failure; }
  };
}
