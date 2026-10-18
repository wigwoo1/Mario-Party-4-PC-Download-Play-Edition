import test from 'node:test';
import assert from 'node:assert/strict';
import {createVerifiedAssetFile} from './verified-asset-file.mjs';

function fixture(){
  const events=[],blob=new Blob(['verified']);let removeFails=false;
  const writer={write:async bytes=>events.push(['write',bytes.byteLength]),close:async()=>events.push('close'),abort:async()=>events.push('abort')};
  const root={getFileHandle:async()=>({createWritable:async()=>writer,getFile:async()=>blob}),removeEntry:async()=>{events.push('remove');if(removeFails)throw Error('Busy');}};
  return {events,blob,writer,root,storage:{getDirectory:async()=>root},setRemoveFails:value=>removeFails=value};
}
test('file snapshot stays alive until explicit disposal and concurrent cleanup runs once',async()=>{
  const f=fixture(),file=await createVerifiedAssetFile(f);
  await file.write(new Uint8Array(4));assert.equal(await file.finish(),f.blob);
  assert.deepEqual(f.events,[['write',4],'close']);
  await Promise.all([file.dispose(),file.dispose()]);await file.dispose();
  assert.deepEqual(f.events,[['write',4],'close','remove']);
});
test('unfinished files abort before removal and failed removal can retry',async()=>{
  const f=fixture(),file=await createVerifiedAssetFile(f);f.setRemoveFails(true);
  await assert.rejects(file.dispose(),/Busy/);f.setRemoveFails(false);await file.dispose();
  assert.deepEqual(f.events,['abort','remove','remove']);
});
test('writer allocation failure removes its newly created file',async()=>{
  const f=fixture();f.root.getFileHandle=async()=>({createWritable:async()=>{throw Error('Quota');}});
  await assert.rejects(createVerifiedAssetFile(f),/Quota/);assert.deepEqual(f.events,['remove']);
});
test('unavailable browser file storage leaves the compatibility path available',async()=>{
  assert.equal(await createVerifiedAssetFile({storage:{}}),undefined);
});
