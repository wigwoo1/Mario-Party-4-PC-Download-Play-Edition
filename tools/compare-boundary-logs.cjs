const fs=require('node:fs');
const assert=require('node:assert/strict');
function readBoundaryLog(file){
 const log=fs.readFileSync(file,'utf8');
 assert.ok(log.includes('PASS: JavaScript awaited real engine'),'Engine run must pass');
 const records=[...log.matchAll(/Actual PAD boundary: (\{[^\r\n]+\})/g)].map(match=>JSON.parse(match[1]));
 assert.ok(records.length>=2,'At least two actual PAD boundaries required');
 records.forEach((record,index)=>{
  assert.equal(record.poll,index,'Contiguous poll sequence required');
  for(const field of ['checkpoint','frand','rand8'])assert.ok(Number.isInteger(record[field])&&record[field]>=0&&record[field]<=0xffffffff,'Invalid boundary '+field);
 });
 return records;
}
function compareBoundaryLogs(left,right){const a=readBoundaryLog(left),b=readBoundaryLog(right);assert.deepEqual(a,b,'Independent engine boundary states differ');return {matched:true,boundaries:a.length,records:a,scope:'Selected state and both RNG streams; fresh engines with synthetic peers, not playable netplay.'};}
module.exports={compareBoundaryLogs};
if(require.main===module){const result=compareBoundaryLogs(process.argv[2],process.argv[3]);if(process.argv[4])fs.writeFileSync(process.argv[4],JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(result));}
