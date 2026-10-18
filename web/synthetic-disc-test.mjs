// Generated fixture only; contains no game resources or executed game code.
export function syntheticCiso() {
  const bytes=new Uint8Array(65536), header=bytes.subarray(0,32768), disc=bytes.subarray(32768);
  const view=new DataView(disc.buffer,disc.byteOffset,disc.byteLength);
  const text=(offset,value)=>disc.set(new TextEncoder().encode(value),offset);
  header.set(new TextEncoder().encode('CISO')); header[5]=0x80; header[8]=1;
  text(0,'GMPE01'); disc[7]=1; view.setUint32(0x1c,0xc2339f3d);
  view.setUint32(0x420,0x3000); view.setUint32(0x424,0x4000);
  view.setUint32(0x428,40); view.setUint32(0x42c,40);
  view.setUint32(0x3000,0x100); view.setUint32(0x3048,0x80003100);
  view.setUint32(0x3090,4); view.setUint32(0x30e0,0x80003100); view.setUint32(0x3100,0x4e800020);
  view.setUint32(0x4000,0x01000000); view.setUint32(0x4008,2);
  view.setUint32(0x400c,1); view.setUint32(0x4010,0x5000); view.setUint32(0x4014,9);
  text(0x4019,'payload.bin'); text(0x5000,'test-data');
  return bytes;
}
export function syntheticLargeCiso() {
  const small=syntheticCiso(), header=small.slice(0,32768);
  new DataView(header.buffer).setUint32(4,2*1024*1024,true);
  header.fill(1,8,8+64);
  const first=new Blob([small.subarray(32768),new Uint8Array(2*1024*1024-32768)]);
  const zero=new Blob([new Uint8Array(2*1024*1024)]);
  return new Blob([header,first,...Array(63).fill(zero)]);
}
