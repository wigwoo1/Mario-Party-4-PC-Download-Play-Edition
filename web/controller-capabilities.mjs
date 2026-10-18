// Portable codec only; not connected to the asset-transfer channel or game runtime.
// Matches controller_routes.hpp. Revisions remain BigInt to preserve all 64 bits.
export function encodeCapabilities({ session, revision, mask }, acknowledgement = false) {
  if (!(session instanceof Uint8Array) || session.length !== 16 || !session.some(Boolean)
      || typeof revision !== 'bigint' || revision <= 0n || revision > 0xffffffffffffffffn
      || !Number.isInteger(mask) || mask < 0 || mask > 0xffffffff || (mask & 0x0fffffff)
      || typeof acknowledgement !== 'boolean') throw new TypeError('Invalid capability message');
  const bytes = new Uint8Array(36);
  bytes.set([80, 66, 67, acknowledgement ? 65 : 80, 1]);
  bytes.set(session, 8);
  const view = new DataView(bytes.buffer);
  view.setBigUint64(24, revision, true);
  view.setUint32(32, mask, true);
  return bytes;
}

export function decodeCapabilities(bytes, acknowledgement = false) {
  if (!(bytes instanceof Uint8Array) || bytes.length !== 36 || typeof acknowledgement !== 'boolean'
      || bytes[0] !== 80 || bytes[1] !== 66 || bytes[2] !== 67
      || bytes[3] !== (acknowledgement ? 65 : 80) || bytes[4] !== 1
      || bytes[5] || bytes[6] || bytes[7]) return null;
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const result = { session: bytes.slice(8, 24), revision: view.getBigUint64(24, true),
    mask: view.getUint32(32, true) };
  if (!result.session.some(Boolean) || !result.revision || (result.mask & 0x0fffffff)) return null;
  return result;
}
