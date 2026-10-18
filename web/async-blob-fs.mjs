// Experimental read-only adapter for the pinned Emscripten runtime.
// Requires tools/async_blob_read.js and Asyncify instrumentation of fd_read.
export function mountAsyncBlob(module, blob, directory = '/assets') {
  const {FS, WORKERFS, Asyncify} = module;
  if (!(blob instanceof Blob) || !FS || !WORKERFS || !Asyncify)
    throw new Error('Async Blob filesystem requires a Blob and Asyncify-enabled runtime');
  const stats = {reads: 0, bytes: 0, largestRead: 0, pending: 0};
  FS.mkdir(directory);
  let rootNode;
  try { FS.mount({mount() {
    const root = WORKERFS.createNode(null, '/', WORKERFS.DIR_MODE, 0);
    rootNode = root;
    const file = WORKERFS.createNode(root, 'verified-disc.ciso', WORKERFS.FILE_MODE, 0, blob);
    file.asyncBlob = stats;
    // All Blob reads must use the complete-vectored-read WASI adapter.
    file.stream_ops = {...WORKERFS.stream_ops, read() {
      throw new Error('Async Blob read requires the custom WASI fd_read adapter');
    }};
    return root;
  }}, {}, directory); }
  catch (error) { FS.rmdir(directory); throw error; }
  let mounted = true, removed = false;
  // Caller must stop the runtime and close its disc descriptors first. Never
  // invalidate a suspended Asyncify read by unmounting an open file.
  Object.defineProperty(stats, 'dispose', {value() {
    if (removed) return;
    if (mounted) {
      if (!Array.isArray(FS.streams)) throw Error('Cannot inspect open disc streams');
      if (FS.streams.some(stream => stream?.node?.parent === rootNode))
        throw Error('Close disc streams before unmounting');
      FS.unmount(directory);
      mounted = false;
    }
    FS.rmdir(directory);
    removed = true;
    rootNode = null;
  }});
  return stats;
}

