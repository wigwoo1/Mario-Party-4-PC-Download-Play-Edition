import {verifiedAssetBlob} from './runtime-asset.mjs';
import {mountAsyncBlob} from './async-blob-fs.mjs';

// Production handoff boundary. The factory must initialize without starting
// game_main. The caller owns shader preparation, synchronized start and stop.
// This helper does not report shader readiness or start the game.
export async function prepareMountedRuntime(store, id, createRuntime, {
  signal, progress, shutdown, mount = mountAsyncBlob,
} = {}) {
  if (typeof createRuntime !== 'function' || typeof shutdown !== 'function')
    throw Error('Runtime factory and shutdown callback are required.');
  const asset = await verifiedAssetBlob(store, id, {signal, progress});
  let module, mounted, disposed = false, disposal;
  const dispose = () => {
    if (disposed) return Promise.resolve();
    if (disposal) return disposal;
    // Register cleanup ownership before shutdown can synchronously reenter it.
    disposal = Promise.resolve().then(async () => {
      // A failed shutdown must retain the mount: an Asyncify read may still
      // own a descriptor. Callers can retry disposal after fixing shutdown.
      if (module) await shutdown(module);
      module?.partyboardReleaseRuntimeArtifacts?.();
      mounted?.dispose();
      await asset.dispose?.();
      disposed = true;
      module = undefined;
      mounted = undefined;
    });
    disposal = disposal.catch(error => {disposal = undefined; throw error;});
    return disposal;
  };
  try {
    signal?.throwIfAborted();
    module = await createRuntime({noInitialRun:true,signal});
    signal?.throwIfAborted();
    mounted = mount(module, asset.blob);
    signal?.throwIfAborted();
    return {
      assetId:asset.manifest.id,
      get module() {return module;},
      get stats() {return mounted;},
      dispose,
    };
  } catch (error) {
    try {await dispose();}
    catch (cleanupError) {
      throw new AggregateError([error, cleanupError], 'Runtime preparation and cleanup failed.');
    }
    throw error;
  }
}
