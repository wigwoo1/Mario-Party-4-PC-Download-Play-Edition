'use strict';
const http=require('node:http'),net=require('node:net'),fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),{spawn}=require('node:child_process');
const executable=path.resolve(process.argv[2]||'work/native-rtc-probe-build/partyboard_router_mapping_http_probe.exe');
const reportPath=process.argv[3]||'outputs/validation/native-router-http-20261016.json';
const service='urn:schemas-upnp-org:service:WANIPConnection:1';
const owner='Partyboard-0123456789abcdef0123456789abcdef';
const envelope=body=>'<?xml version="1.0"?><s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"><s:Body>'+body+'</s:Body></s:Envelope>';
const errorBody=code=>envelope('<s:Fault><faultcode>s:Client</faultcode><faultstring>UPnPError</faultstring><detail><UPnPError xmlns="urn:schemas-upnp-org:control-1-0"><errorCode>'+code+'</errorCode><errorDescription>Fixture error</errorDescription></UPnPError></detail></s:Fault>');
const element=(body,name)=>{const value=body.match(new RegExp('<'+name+'>([^<]*)</'+name+'>'));return value?value[1]:null;};
async function scenario(name){
 let mapping=name==='conflict'?{address:'192.168.1.23',port:45678,owner:'Another application',lease:600}:null;
 let queries=0;const requests=[],errors=[];
 const server=http.createServer((request,response)=>{
  let body='';request.setEncoding('utf8');request.on('data',chunk=>{body+=chunk;});request.on('end',()=>{try{
   const action=String(request.headers.soapaction||'').match(/#([^\"]+)/)?.[1];
   assert.ok(['GetSpecificPortMappingEntry','AddPortMapping','DeletePortMapping'].includes(action));
   assert.equal(element(body,'NewProtocol'),'UDP');assert.equal(element(body,'NewExternalPort'),'45678');assert.equal(element(body,'NewRemoteHost'),'');
   requests.push({action,lease:element(body,'NewLeaseDuration'),owner:element(body,'NewPortMappingDescription')});
   const send=(status,text)=>{response.writeHead(status,{'Content-Type':'text/xml','Connection':'close'});response.end(text);};
   const ok=content=>send(200,envelope('<u:'+action+'Response xmlns:u="'+service+'">'+content+'</u:'+action+'Response>'));
   const fault=code=>send(500,errorBody(code));
   if(action==='GetSpecificPortMappingEntry'){
    ++queries;
    if(['stall','cancel','outer-deadline','worker-cancel'].includes(name))return;
    if(name==='trickle'){
     response.writeHead(200,{'Content-Type':'text/xml','Content-Length':'1000000'});
     const interval=setInterval(()=>response.write(' '),20);response.once('close',()=>clearInterval(interval));return;
    }
    if(name==='oversized')return send(200,envelope('<padding>'+' '.repeat(300000)+'</padding>'));
    if(name==='query-error')return fault(501);
    if(name==='malformed-response')return ok('<NewInternalClient>192.168.1.23</NewInternalClient><NewInternalPort>45678</NewInternalPort>');
    if(queries===3&&['foreign-on-renew','foreign-on-close'].includes(name))mapping={...mapping,owner:'Another instance'};
    if(!mapping)return fault(714);
    const lease=name==='missing-lease'?'':'<NewLeaseDuration>'+mapping.lease+'</NewLeaseDuration>';
    return ok('<NewInternalClient>'+mapping.address+'</NewInternalClient><NewInternalPort>'+mapping.port+'</NewInternalPort><NewEnabled>1</NewEnabled><NewPortMappingDescription>'+mapping.owner+'</NewPortMappingDescription>'+lease);
   }
   if(action==='AddPortMapping'){
    assert.equal(element(body,'NewInternalClient'),'192.168.1.23');assert.equal(element(body,'NewInternalPort'),'45678');
    const requestedOwner=element(body,'NewPortMappingDescription');
    assert.equal(element(body,'NewLeaseDuration'),'600');
    if(name.startsWith('worker-'))assert.match(requestedOwner,/^Partyboard-[0-9a-f]{32}$/);else assert.equal(requestedOwner,owner);
    assert.ok(!mapping||mapping.owner===requestedOwner,'Attempted to replace another owner');
    if(name==='permanent-only')return fault(725);
    if(name==='add-error')return fault(501);
    mapping={address:'192.168.1.23',port:45678,owner:requestedOwner,lease:name==='permanent'?0:600};
    if(name==='lost-add-reply'){response.destroy();return;}
    return ok('');
   }
   if(name.startsWith('worker-'))assert.match(mapping?.owner||'',/^Partyboard-[0-9a-f]{32}$/);
   else assert.equal(mapping?.owner,owner,'Attempted to delete another owner');
   if(name==='worker-cleanup-stall')return;
   mapping=null;ok('');
  }catch(error){errors.push(error);response.destroy();}});
 });
 await new Promise((resolve,reject)=>{server.once('error',reject);server.listen(0,'127.0.0.1',resolve);});
 const port=server.address().port;
 let stdout='',stderr='',exit;
 try{
  exit=await new Promise((resolve,reject)=>{
   const child=spawn(executable,['http://127.0.0.1:'+port+'/control',name],{windowsHide:true,stdio:['ignore','pipe','pipe']});
   const timer=setTimeout(()=>{child.kill();reject(Error('Router adapter test exceeded 30 seconds: '+name));},30000);
   child.stdout.on('data',chunk=>stdout+=chunk);child.stderr.on('data',chunk=>stderr+=chunk);
   child.once('error',error=>{clearTimeout(timer);reject(error);});child.once('exit',(code,signal)=>{clearTimeout(timer);resolve({code,signal});});
  });
  if(errors.length)throw errors[0];assert.equal(exit.code,0,stdout+'\n'+stderr);
  const adds=requests.filter(item=>item.action==='AddPortMapping').length,deletes=requests.filter(item=>item.action==='DeletePortMapping').length;
  const expected={normal:[2,1],conflict:[0,0],'foreign-on-renew':[1,0],'foreign-on-close':[1,0],permanent:[1,1],'missing-lease':[1,1],'query-error':[0,0],'malformed-response':[0,0],'permanent-only':[1,0],'add-error':[1,0],'lost-add-reply':[1,1],stall:[0,0],trickle:[0,0],oversized:[0,0],cancel:[0,0],'outer-deadline':[0,0],'dns-blocked':[0,0],'worker-shared':[1,1],'worker-cancel':[0,0],'worker-cleanup-stall':[1,1]}[name];
  assert.deepEqual([adds,deletes],expected);
  const foreign=['conflict','foreign-on-renew','foreign-on-close'].includes(name);
  if(foreign)assert.ok(mapping&&mapping.owner!==owner);
  else if(name==='worker-cleanup-stall')assert.ok(mapping&&mapping.lease===600);
  else assert.equal(mapping,null);
  if(name==='dns-blocked')assert.equal(requests.length,0);
  if(name==='worker-cancel')assert.ok(requests.length>0);
  const elapsed=stdout.match(/elapsedMs=(\d+)/),close=stdout.match(/closeMs=(\d+); cleanupVerified=(\d)/);
  console.log(stdout.trim());return {scenario:name,exitCode:exit.code,adds,deletes,foreignMappingPreserved:foreign,
   ...(elapsed?{elapsedMs:Number(elapsed[1])}:{}),...(close?{closeMs:Number(close[1]),cleanupVerified:close[2]==='1'}:{}),requests};
 }finally{
  server.closeAllConnections();await new Promise(resolve=>server.close(resolve));
  await new Promise((resolve,reject)=>{const socket=net.connect({host:'127.0.0.1',port});socket.once('connect',()=>{socket.destroy();reject(Error('Fixture server still open'));});socket.once('error',error=>error.code==='ECONNREFUSED'?resolve():reject(error));});
 }
}
(async()=>{
 const results=[];for(const name of ['normal','conflict','foreign-on-renew','foreign-on-close','permanent','missing-lease','query-error','malformed-response','permanent-only','add-error','lost-add-reply','stall','trickle','oversized','cancel','outer-deadline','dns-blocked','worker-shared','worker-cancel','worker-cleanup-stall'])results.push(await scenario(name));
 const report={status:'VERIFIED',scope:'Actual pinned MiniUPnPc SOAP requests against a loopback gateway fixture; no real-router or game-transport mapping claim',sourceCommit:'bf4215a7574f88aa55859db9db00e3ae58cf42d6',scenarios:results,allFixtureServersClosed:true,physicalRouterContacted:false,productionTransportIntegrated:false};
 fs.writeFileSync(reportPath,JSON.stringify(report,null,2)+'\n');console.log('PASS all '+results.length+' loopback SOAP router scenarios');
})().catch(error=>{console.error(error.stack);process.exitCode=1;});
