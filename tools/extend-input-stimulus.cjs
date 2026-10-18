const assert=require('node:assert/strict');
const fs=require('node:fs');
const {compareTraces}=require('./compare-input-traces.cjs');

// Appended frames contain neutral input and preserve each pad's connection
// status. Their checkpoints are placeholders: script mode records new ones.
function extendStimulus(source,samples) {
  const {leftSamples:count}=compareTraces(source,source);
  assert.ok(count>0,'A stimulus needs an existing pad state');
  assert.ok(Number.isSafeInteger(samples)&&samples>=count&&samples<=1000000,'Invalid extended sample count');
  const result=Buffer.alloc(16+samples*68);
  source.copy(result);
  const previous=16+(count-1)*68;
  for(let index=count;index<samples;index++) {
    const offset=16+index*68;
    result.writeBigUInt64LE(BigInt(index),offset);
    for(let pad=0;pad<4;pad++)result.writeInt8(source.readInt8(previous+30+pad*11),offset+30+pad*11);
  }
  return result;
}
module.exports={extendStimulus};
if(require.main===module) {
  const result=extendStimulus(fs.readFileSync(process.argv[2]),Number(process.argv[4]));
  fs.writeFileSync(process.argv[3],result,{flag:'wx'});
  console.log(`Wrote ${(result.length-16)/68} stimulus samples; pad connection states preserved.`);
}
