const {test}=require('node:test');
const assert=require('node:assert/strict');
const {extendStimulus}=require('./extend-input-stimulus.cjs');
function fixture() {
  const b=Buffer.alloc(84);Buffer.from('5042545241434503','hex').copy(b);b.writeUInt32LE(1,8);
  for(let pad=0;pad<4;pad++)b.writeInt8(pad===0?0:-1,46+pad*11);
  b.writeUInt16LE(0x100,36);b.writeInt8(64,38);
  return b;
}
test('extension keeps connected human pad and disconnected unused pads without held input',()=>{
  const source=fixture(),result=extendStimulus(source,3);
  assert.deepEqual(result.subarray(0,source.length),source);
  for(let i=1;i<3;i++) {
    const o=16+i*68;assert.equal(result.readBigUInt64LE(o),BigInt(i));
    for(let p=0;p<4;p++) {
      assert.equal(result.readInt8(o+30+p*11),p===0?0:-1);
      assert.ok(result.subarray(o+20+p*11,o+30+p*11).every(x=>x===0));
    }
  }
});
test('invalid counts and malformed input are rejected',()=>{
  for(const count of [0,1.5,NaN,1000001])assert.throws(()=>extendStimulus(fixture(),count));
  assert.throws(()=>extendStimulus(Buffer.alloc(16),2));
  assert.throws(()=>extendStimulus(fixture().subarray(0,83),2));
});
