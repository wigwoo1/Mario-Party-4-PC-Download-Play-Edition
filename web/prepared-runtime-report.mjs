import {validateSessionConfiguration} from './session-config.mjs';
const uint=value=>Number.isInteger(value)&&value>=0&&value<=0xffffffff;
export function preparedRuntimeReport(runtime,configuration){
  validateSessionConfiguration(configuration);
  if(runtime?.engine?.phase!=='Idle'||runtime.assetId!==configuration.assetId||
    runtime.engine.preparedCheckpoint()!==runtime.checkpoint||runtime.module?._PartyBoard_GameFrameCount()!==0)
    throw Error('Prepared frame-zero lobby runtime required.');
  return validatePreparedRuntimeReport({type:'runtime-prepared',version:1,
    session:configuration.session,assetId:configuration.assetId,seed:configuration.seed,
    buildId:runtime.buildId,checkpoint:runtime.checkpoint,frame:0,
    pipelineSeed:runtime.pipelineSeed?.sha256??null,
    shaderScope:runtime.rendererPreparation?.scope},configuration);
}
export function validatePreparedRuntimeReport(message,configuration){
  validateSessionConfiguration(configuration);
  if(!message||message.type!=='runtime-prepared'||message.version!==1||
    message.session!==configuration.session||message.assetId!==configuration.assetId||
    message.seed!==configuration.seed||typeof message.buildId!=='string'||!/^[a-f0-9]{64}$/.test(message.buildId)||
    (configuration.buildId!==undefined&&message.buildId!==configuration.buildId)||
    !uint(message.checkpoint)||(message.pipelineSeed!=null&&(typeof message.pipelineSeed!=='string'||!/^[a-f0-9]{64}$/.test(message.pipelineSeed)))||message.frame!==0||message.shaderScope!=='discovered-pipelines-only')
    throw Error('Invalid prepared runtime report.');
  return Object.freeze({type:'runtime-prepared',version:1,session:message.session,
    assetId:message.assetId,seed:message.seed,buildId:message.buildId,
    checkpoint:message.checkpoint,frame:0,pipelineSeed:message.pipelineSeed??null,shaderScope:message.shaderScope});
}
export function receivePreparedRuntimeReport(data,configuration,current){
  if(typeof data!=='string'||data.length>2048)throw Error('Invalid prepared runtime packet.');
  const report=validatePreparedRuntimeReport(JSON.parse(data),configuration);
  if(current&&JSON.stringify(current)!==JSON.stringify(report))throw Error('Prepared runtime changed.');
  return current||report;
}
export function comparePreparedRuntimeReports(host,guest){
  return host.buildId===guest.buildId&&host.checkpoint===guest.checkpoint&&
    (host.pipelineSeed??null)===(guest.pipelineSeed??null)&&host.assetId===guest.assetId&&host.session===guest.session&&host.seed===guest.seed;
}
