const hash=value=>typeof value==='string'&&/^[a-f0-9]{64}$/.test(value);
export function validateSessionConfiguration(message,assetId) {
  let runtimeArtifacts;
  if(message?.runtimeArtifacts!==undefined){
    const names=new Set(['contract.mjs','contract.wasm']);
    if(!hash(message.buildId)||!Array.isArray(message.runtimeArtifacts)||message.runtimeArtifacts.length>254)
      throw Error('Invalid shared runtime artifact configuration.');
    runtimeArtifacts=Object.freeze(message.runtimeArtifacts.map(name=>{
      if(typeof name!=='string'||name.length>512||!/^[a-zA-Z0-9_.-]+(?:\/[a-zA-Z0-9_.-]+)*$/.test(name)||
        name.split('/').some(part=>part==='.'||part==='..')||names.has(name))throw Error('Invalid shared runtime artifact configuration.');
      names.add(name);return name;
    }));
  }
  if(!message||message.type!=='session-configuration'||message.version!==1||
    message.profile!=='unlocked-no-save-v1'||!hash(message.assetId)||
    (assetId!==undefined&&message.assetId!==assetId)||
    (message.buildId!==undefined&&!hash(message.buildId))||
    typeof message.session!=='string'||!/^[a-f0-9]{32}$/.test(message.session)||/^0+$/.test(message.session)||
    !Number.isInteger(message.seed)||message.seed<0||message.seed>0xffffffff)
    throw Error('Invalid session configuration.');
  return Object.freeze({type:message.type,version:1,profile:message.profile,
    assetId:message.assetId,session:message.session,seed:message.seed,
    ...(message.buildId!==undefined?{buildId:message.buildId}:{}),
    ...(runtimeArtifacts!==undefined?{runtimeArtifacts}:{})});
}
export function createSessionConfiguration(assetId,random=crypto,buildId=undefined,runtimeArtifacts=undefined) {
  const session=random.getRandomValues(new Uint8Array(16));
  const seed=random.getRandomValues(new Uint32Array(1))[0];
  return validateSessionConfiguration({type:'session-configuration',version:1,
    profile:'unlocked-no-save-v1',assetId,seed,...(buildId!==undefined?{buildId}:{}),...(runtimeArtifacts!==undefined?{runtimeArtifacts}:{}),
    session:Array.from(session,b=>b.toString(16).padStart(2,'0')).join('')},assetId);
}
export function receiveSessionConfiguration(data,{assetId,current}={}) {
  if(typeof data!=='string'||data.length>65536)throw Error('Invalid session configuration packet.');
  const next=validateSessionConfiguration(JSON.parse(data),assetId);
  if(current&&JSON.stringify(next)!==JSON.stringify(current))throw Error('Session configuration changed.');
  return current||next;
}
