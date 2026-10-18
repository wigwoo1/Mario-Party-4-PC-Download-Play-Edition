'use strict';
const fs=require('node:fs'),assert=require('node:assert/strict');
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');
const {waitForBrowserCondition}=require('./browser-condition.cjs');
(async()=>{
 const browser=await chromium.launch({channel:process.env.PARTYBOARD_TEST_BROWSER_CHANNEL||'chrome',headless:true,args:['--mute-audio']});
 try{
  const page=await browser.newPage();await page.goto('about:blank');
  // Record upstream behavior without requiring a particular dependency bug to
  // remain present after a future Playwright upgrade.
  const probeStarted=performance.now();let upstreamReturnsFalseEarly=false;
  try{const handle=await page.waitForFunction(async()=>false,null,{timeout:250});upstreamReturnsFalseEarly=await handle.jsonValue()===false;await handle.dispose();}
  catch(error){if(error.name!=='TimeoutError')throw error;}
  const upstreamProbeMs=performance.now()-probeStarted;
  await page.evaluate(()=>{globalThis.conditionReady=false;setTimeout(()=>conditionReady=true,300);});
  const waitStarted=performance.now();
  await waitForBrowserCondition(page,async()=>{await Promise.resolve();return conditionReady;});
  const resolvedWaitMs=performance.now()-waitStarted;assert.ok(resolvedWaitMs>=250,'Must wait for readiness rather than the Promise object');
  await assert.rejects(waitForBrowserCondition(page,async()=>false,null,{timeout:150,interval:20}),/deadline/);
  await assert.rejects(waitForBrowserCondition(page,()=>new Promise(()=>{}),null,{timeout:150}),/deadline/);
  await assert.rejects(waitForBrowserCondition(page,()=>{throw Error('fixture predicate error');}),/fixture predicate error/);
  await assert.rejects(waitForBrowserCondition(page,()=>({ready:false})),/boolean/);
  const report={verified:true,upstreamReturnsFalseEarly,upstreamProbeMs,resolvedWaitMs,
   resolvedReadinessVerified:true,falseTimeoutVerified:true,unresolvedPromiseTimeoutVerified:true,
   predicateErrorsPreserved:true,nonBooleanRejected:true,gameExecuted:false,dependencyModified:false};
  if(process.argv[2])fs.writeFileSync(process.argv[2],JSON.stringify(report,null,2)+'\n');
  console.log(JSON.stringify(report));
 }finally{await browser.close();}
})().catch(error=>{console.error(error.stack);process.exitCode=1;});
