'use strict';
const fs=require('node:fs'),net=require('node:net'),os=require('node:os');
function serverListenConfiguration(environment=process.env){
 const host=environment.PARTYBOARD_LISTEN_ADDRESS||'127.0.0.1';
 if(host!=='localhost'&&!net.isIP(host))throw Error('Listening address must be localhost or a literal IP address');
 const certificate=environment.PARTYBOARD_TLS_CERTIFICATE,key=environment.PARTYBOARD_TLS_KEY;
 if(Boolean(certificate)!==Boolean(key))throw Error('HTTPS needs both PARTYBOARD_TLS_CERTIFICATE and PARTYBOARD_TLS_KEY');
 const loopback=host==='localhost'||host==='::1'||(net.isIP(host)===4&&host.startsWith('127.'));
 if(!loopback&&!certificate)throw Error('Listening beyond this computer requires HTTPS certificate and key files for browser game support');
 function readPem(filename){
  const stat=fs.statSync(filename);
  if(!stat.isFile()||stat.size<1||stat.size>1024*1024)throw Error('HTTPS certificate/key must be nonempty PEM files under 1 MiB');
  return fs.readFileSync(filename);
 }
 return {host,loopback,scheme:certificate?'https':'http',tls:certificate?{cert:readPem(certificate),key:readPem(key)}:undefined};
}
function listeningUrls({host,scheme},port,interfaces=os.networkInterfaces()){
 let hosts=[host];
 if(host==='0.0.0.0'||host==='::')hosts=Object.values(interfaces).flat().filter(address=>address&&!address.internal&&(host==='::'||address.family==='IPv4')).map(address=>address.address);
 return [...new Set(hosts)].map(address=>scheme+'://'+(net.isIP(address)===6?'['+address.replace(/%/g,'%25')+']':address)+':'+port);
}
module.exports={serverListenConfiguration,listeningUrls};
