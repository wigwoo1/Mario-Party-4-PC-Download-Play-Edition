const fs=require('node:fs'),crypto=require('node:crypto');
function digest(file){return new Promise((resolve,reject)=>{
 const hash=crypto.createHash('sha256'),stream=fs.createReadStream(file);
 stream.on('error',reject);stream.on('data',chunk=>hash.update(chunk));stream.on('end',()=>resolve(hash.digest('hex')));
});}
async function verifyDigest(file,expected){
 if(!/^[a-f0-9]{64}$/.test(expected))throw Error('Valid expected SHA-256 required');
 if(await digest(file)!==expected)throw Error('Local file digest mismatch');
}
module.exports={digest,verifyDigest};
