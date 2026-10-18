import {test} from 'node:test';
import assert from 'node:assert/strict';
import {mountAsyncBlob} from './async-blob-fs.mjs';
function fixture(){
 const calls=[],FS={streams:[],mkdir(){calls.push('mkdir');},mount(type){this.root=type.mount();},unmount(){calls.push('unmount');},rmdir(){calls.push('rmdir');}};
 const WORKERFS={DIR_MODE:1,FILE_MODE:2,stream_ops:{},createNode(parent,name,mode,dev,contents){const node={parent,name,contents};if(parent)parent.child=node;return node;}};
 return {FS,WORKERFS,Asyncify:{},calls};
}
test('open disc blocks disposal, closed disc permits idempotent cleanup',()=>{
 const m=fixture(),stats=mountAsyncBlob(m,new Blob(['disc']));
 m.FS.streams.push({node:m.FS.root.child});
 assert.throws(()=>stats.dispose(),/Close disc/);assert.deepEqual(m.calls,['mkdir']);
 m.FS.streams=[];stats.dispose();stats.dispose();
 assert.deepEqual(m.calls,['mkdir','unmount','rmdir']);
 assert.deepEqual(JSON.parse(JSON.stringify(stats)),{reads:0,bytes:0,largestRead:0,pending:0});
});
test('failed mount releases only its newly created directory',()=>{
 const m=fixture();m.FS.mount=()=>{throw Error('mount failed');};
 assert.throws(()=>mountAsyncBlob(m,new Blob()),/mount failed/);
 assert.deepEqual(m.calls,['mkdir','rmdir']);
 const existing=fixture();existing.FS.mkdir=()=>{throw Error('already exists');};
 assert.throws(()=>mountAsyncBlob(existing,new Blob()),/already exists/);
 assert.deepEqual(existing.calls,[]);
});
test('directory cleanup failure can be retried without double unmount',()=>{
 const m=fixture(),stats=mountAsyncBlob(m,new Blob());let first=true;
 m.FS.rmdir=()=>{if(first){first=false;throw Error('busy');}m.calls.push('rmdir');};
 assert.throws(()=>stats.dispose(),/busy/);stats.dispose();
 assert.deepEqual(m.calls,['mkdir','unmount','rmdir']);
});
