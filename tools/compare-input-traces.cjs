const assert=require('node:assert/strict');
const fs=require('node:fs');
function compareTraces(left,right) {
  for(const trace of [left,right]) {
    assert.ok(trace.length>=16 && (trace.length-16)%68===0,'Truncated trace');
    assert.equal(trace.subarray(0,8).toString('hex'),'5042545241434503','Requires trace version 3');
    assert.equal(trace.readUInt32LE(12),0,'Invalid reserved header bytes');
    for(let offset=16,index=0;offset<trace.length;offset+=68,index++)
      assert.equal(trace.readBigUInt64LE(offset),BigInt(index),'Invalid sample sequence');
  }
  const samples=Math.min((left.length-16)/68,(right.length-16)/68);
  let firstInputDifference=null,firstCheckpointDifference=null;
  const checkpointFields={frand:8,rand8:12,rumble:16,selectedState:64};
  const checkpointDifferences=Object.fromEntries(Object.keys(checkpointFields).map(name=>[name,null]));
  for(let index=0;index<samples;index++) {
    const offset=16+index*68;
    if(firstInputDifference===null && !left.subarray(offset+20,offset+64).equals(right.subarray(offset+20,offset+64)))firstInputDifference=index;
    if(firstCheckpointDifference===null && (!left.subarray(offset+8,offset+20).equals(right.subarray(offset+8,offset+20)) || left.readUInt32LE(offset+64)!==right.readUInt32LE(offset+64)))firstCheckpointDifference=index;
    for(const [name,fieldOffset] of Object.entries(checkpointFields)) {
      if(checkpointDifferences[name]===null && left.readUInt32LE(offset+fieldOffset)!==right.readUInt32LE(offset+fieldOffset))
        checkpointDifferences[name]={sample:index,left:left.readUInt32LE(offset+fieldOffset),right:right.readUInt32LE(offset+fieldOffset)};
    }
  }
  return {samples,leftSamples:(left.length-16)/68,rightSamples:(right.length-16)/68,sameSeed:left.readUInt32LE(8)===right.readUInt32LE(8),firstInputDifference,firstCheckpointDifference,checkpointDifferences};
}
module.exports={compareTraces};
if(require.main===module)console.log(JSON.stringify(compareTraces(fs.readFileSync(process.argv[2]),fs.readFileSync(process.argv[3])),null,2));
