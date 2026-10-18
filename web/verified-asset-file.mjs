// Large received assets need file backing rather than a second full copy in
// Chromium's Blob memory pool. The caller owns this temporary file's lifetime.
export async function createVerifiedAssetFile({storage=globalThis.navigator?.storage}={}) {
  if(typeof storage?.getDirectory!=='function')return undefined;
  const root=await storage.getDirectory();
  const name='partyboard-runtime-'+crypto.randomUUID()+'.ciso';
  const handle=await root.getFileHandle(name,{create:true});
  let writer;
  try{writer=await handle.createWritable();}
  catch(error){
    try{await root.removeEntry(name);}catch(cleanup){throw new AggregateError([error,cleanup],'Asset file allocation and cleanup failed.');}
    throw error;
  }
  let closed=false,removed=false,disposal;
  const dispose=()=>{
    if(removed)return Promise.resolve();
    if(disposal)return disposal;
    disposal=Promise.resolve().then(async()=>{
      if(!closed){await writer.abort();closed=true;}
      try{await root.removeEntry(name);}catch(error){if(error.name!=='NotFoundError')throw error;}
      removed=true;
    }).catch(error=>{disposal=undefined;throw error;});
    return disposal;
  };
  return {
    async write(bytes){if(closed)throw Error('Asset file is closed.');await writer.write(bytes);},
    async finish(){await writer.close();closed=true;return handle.getFile();},
    dispose,
  };
}
