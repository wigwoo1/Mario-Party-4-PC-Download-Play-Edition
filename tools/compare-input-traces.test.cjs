const {test}=require('node:test');
const assert=require('node:assert/strict');
const {compareTraces}=require('./compare-input-traces.cjs');
function trace(samples=4) {
  const bytes=Buffer.alloc(16+68*samples);
  Buffer.from('5042545241434503','hex').copy(bytes);
  bytes.writeUInt32LE(1,8);
  for(let i=0;i<samples;i++)bytes.writeBigUInt64LE(BigInt(i),16+i*68);
  return bytes;
}
test('reports each checkpoint component independently from controller input',()=>{
  const left=trace(),right=trace();
  for(const [sample,offset,value] of [[0,8,42],[1,12,43],[2,16,44],[3,64,45]])
    right.writeUInt32LE(value,16+sample*68+offset);
  const result=compareTraces(left,right);
  assert.equal(result.firstInputDifference,null);
  assert.equal(result.firstCheckpointDifference,0);
  assert.deepEqual(result.checkpointDifferences,{
    frand:{sample:0,left:0,right:42},rand8:{sample:1,left:0,right:43},
    rumble:{sample:2,left:0,right:44},selectedState:{sample:3,left:0,right:45},
  });
});
test('input-only changes leave all checkpoint diagnostics clear',()=>{
  const left=trace(),right=trace();right[16+2*68+20]=1;
  const result=compareTraces(left,right);
  assert.equal(result.firstInputDifference,2);assert.equal(result.firstCheckpointDifference,null);
  assert.ok(Object.values(result.checkpointDifferences).every(value=>value===null));
});
test('unequal lengths compare only complete common samples and reject bad sequences',()=>{
  const result=compareTraces(trace(2),trace(4));
  assert.equal(result.samples,2);assert.equal(result.rightSamples,4);
  const invalid=trace();invalid.writeBigUInt64LE(9n,16+68);
  assert.throws(()=>compareTraces(trace(),invalid),/sample sequence/);
  assert.throws(()=>compareTraces(trace(),trace().subarray(0,-1)),/Truncated/);
});
