// Native PBIP v1 layout. Ownership, frame windows and state agreement belong to
// the session's lockstep queue; successful decoding does not authorize input.
export function encodeInput({session, frame, checkpoint, slot, pad}) {
  if (!(session instanceof Uint8Array) || session.length !== 16
      || typeof frame !== 'bigint' || frame < 0n || frame > 0xffffffffffffffffn
      || !Number.isInteger(checkpoint) || checkpoint < 0 || checkpoint > 0xffffffff
      || !Number.isInteger(slot) || slot < 0 || slot > 3
      || !(pad instanceof Uint8Array) || pad.length !== 11) throw new TypeError('Invalid input packet');
  const bytes = new Uint8Array(48);
  bytes.set([80, 66, 73, 80, 1, slot]); bytes.set(session, 8); bytes.set(pad, 36);
  const view = new DataView(bytes.buffer);
  view.setBigUint64(24, frame, true); view.setUint32(32, checkpoint, true);
  return bytes;
}
export function decodeInput(bytes) {
  if (!(bytes instanceof Uint8Array) || bytes.length !== 48
      || bytes[0] !== 80 || bytes[1] !== 66 || bytes[2] !== 73 || bytes[3] !== 80
      || bytes[4] !== 1 || bytes[5] > 3 || bytes[6] || bytes[7] || bytes[47]) return null;
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  return {session: bytes.slice(8, 24), frame: view.getBigUint64(24, true),
    checkpoint: view.getUint32(32, true), slot: bytes[5], pad: bytes.slice(36, 47)};
}
