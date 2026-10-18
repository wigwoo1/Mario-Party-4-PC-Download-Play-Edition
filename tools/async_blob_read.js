// Experimental WASI adapter: suspend once around the complete vectored read.
addToLibrary({
  fd_read__deps: ['$doReadv', '$SYSCALLS', '$Asyncify', '$FS'],
  fd_read__async: true,
  fd_read: (fd, iov, count, result) => {
    const stream = SYSCALLS.getStreamFromFD(fd);
    if (!stream.node.asyncBlob) {
      HEAPU32[result >> 2] = doReadv(stream, iov, count);
      return 0;
    }
    const stats = stream.node.asyncBlob;
    stats.pending = (stats.pending || 0) + 1;
    let operation;
    try {
      operation = Asyncify.handleAsync(async () => {
      const vectors = [];
      for (let i = 0; i < count; ++i)
        vectors.push([HEAPU32[(iov >> 2) + 2*i], HEAPU32[(iov >> 2) + 2*i + 1]]);
      stats.lastPending = {
        position: stream.position,
        vectorCount: vectors.length,
        requestedBytes: vectors.reduce((sum, vector) => sum + vector[1], 0),
      };
      let total = 0;
      try {
        for (const [destination, length] of vectors) {
          const bytes = new Uint8Array(await stream.node.contents.slice(stream.position, stream.position + length).arrayBuffer());
          HEAPU8.set(bytes, destination);
          stream.position += bytes.length;
          total += bytes.length;
          const stats = stream.node.asyncBlob;
          stats.reads++; stats.bytes += bytes.length;
          stats.largestRead = Math.max(stats.largestRead, bytes.length);
          if (bytes.length < length) break;
        }
        HEAPU32[result >> 2] = total;
        return 0;
      } catch (_) {
        HEAPU32[result >> 2] = total;
        stats.lastError = 'EIO while reading verified disc Blob';
        return 29; // WASI EIO
      }
      });
    } catch (error) {
      stats.pending--;
      throw error;
    }
    return Promise.resolve(operation).finally(() => { stats.pending--; });
  }
});
