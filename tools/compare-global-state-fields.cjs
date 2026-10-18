const fs=require('node:fs'),assert=require('node:assert/strict');
const [nativePath,browserPath,reportPath]=process.argv.slice(2);
if(!nativePath||!browserPath||!reportPath)throw Error('Two actual logs and report path required');
function terminal(path){
  const prefix='Global state snapshot: ';
  const captures=fs.readFileSync(path,'utf8').split(/\r?\n/).filter(line=>line.startsWith(prefix))
    .map(line=>JSON.parse(line.slice(prefix.length)));
  const result=captures.at(-1);
  assert.ok(result?.frame>0&&Array.isArray(result.fields),'Actual terminal field capture missing');
  return result;
}
const native=terminal(nativePath),browser=terminal(browserPath);
assert.equal(native.frame,browser.frame,'Capture frames differ');
assert.equal(native.fields.length,browser.fields.length,'Checkpoint schema differs');
const differences=[];
native.fields.forEach(([name,value],index)=>{
  const [otherName,otherValue]=browser.fields[index];
  assert.equal(name,otherName,`Checkpoint field schema differs at ${index}`);
  if(value!==otherValue)differences.push({index,name,native:value,browser:otherValue});
});
fs.writeFileSync(reportPath,JSON.stringify({passed:!differences.length,frame:native.frame,
  fields:native.fields.length,nativePath,browserPath,differences},null,2));
console.log(JSON.stringify({frame:native.frame,fields:native.fields.length,differences}));
assert.equal(differences.length,0,'Actual global fields differ; see report');
