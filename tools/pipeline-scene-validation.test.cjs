const test=require('node:test'),assert=require('node:assert/strict');
const {validatePipelineSceneVisits}=require('./pipeline-scene-validation.cjs');
const visit=overlay=>({overlay,firstFrame:10,lastFrame:20,samples:11});
test('results overlay alone cannot certify selected minigame execution',()=>{
 assert.throws(()=>validatePipelineSceneVisits({84:visit(84)},{minigameRun:true,selectedMinigame:{overlay:27}}),/not observed/);
 assert.equal(validatePipelineSceneVisits({27:visit(27),84:visit(84)},{minigameRun:true,selectedMinigame:{overlay:27}}),true);
});
test('missing observer and malformed visits reject capture',()=>{
 for(const visits of [{},{27:{...visit(27),samples:0}},{27:{...visit(27),lastFrame:9}},{27:visit(28)}])assert.throws(()=>validatePipelineSceneVisits(visits));
});
