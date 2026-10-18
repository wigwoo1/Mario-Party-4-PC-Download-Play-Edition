// Readiness covers the verified known pipeline definitions, not every scene.
export function knownShaderReadiness(runtime){
 const seed=runtime?.pipelineSeed,receipt=runtime?.rendererPreparation,module=runtime?.module;
 if(runtime?.engine?.phase!=='Idle'||module?._PartyBoard_GameFrameCount?.()!==0||
  runtime.engine.preparedCheckpoint()!==runtime.checkpoint||receipt?.checkpoint!==runtime.checkpoint||
  seed?.scope!=='seeded-pipelines-only'||!Number.isSafeInteger(seed.size)||seed.size<100||
  !/^[a-f0-9]{64}$/.test(seed.sha256)||receipt.scope!=='discovered-pipelines-only'||
  !Number.isSafeInteger(receipt.created)||receipt.created<1||receipt.queued!==0||
  module?._PartyBoardBrowserPipelineCount?.(0)!==0||module._PartyBoardBrowserPipelineCount(1)!==receipt.created)
  throw Error('Verified known shader cache must finish preparation on the unchanged frame-zero runtime.');
 return Object.freeze({scope:'known-pipelines-only',fullCoverage:false,sha256:seed.sha256,created:receipt.created});
}
