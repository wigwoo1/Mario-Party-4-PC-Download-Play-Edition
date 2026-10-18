import {CHUNK,validateManifest} from './transfer.mjs';

// Read-only adapter: mount verification reads the user's immutable File directly.
// No game bytes are uploaded or copied into browser storage for local play.
export function localAssetStore(asset) {
  const manifest={...validateManifest(asset?.manifest)},file=asset?.file;
  if(!(file instanceof Blob)||file.size!==manifest.size)throw Error('Matching local asset required.');
  const snapshot=new Blob([file]);
  return {
    immutableBlob:snapshot,
    async manifest(id){if(id!==manifest.id)throw Error('Unknown local asset.');return {...manifest};},
    async get(id,index){
      if(id!==manifest.id||!Number.isInteger(index)||index<0||index>=manifest.count)
        throw Error('Invalid local asset chunk.');
      return {bytes:await snapshot.slice(index*CHUNK,Math.min((index+1)*CHUNK,manifest.size)).arrayBuffer()};
    },
  };
}
