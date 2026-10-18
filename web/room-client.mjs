const roomPattern=/^[A-Za-z0-9_-]{22}$/,tokenPattern=/^[A-Za-z0-9_-]{43}$/;
export function roomJoinLink(pageURL,{room,joinKey}){
 if(!roomPattern.test(room)||!tokenPattern.test(joinKey))throw Error('Invalid lobby link.');
 const url=new URL(pageURL);url.hash=new URLSearchParams({room,key:joinKey}).toString();return url.href;
}
export function readRoomJoinLink(pageURL){
 const url=new URL(pageURL),params=new URLSearchParams(url.hash.slice(1)),room=params.get('room'),joinKey=params.get('key');
 if(room===null&&joinKey===null)return null;
 if(!roomPattern.test(room??'')||!tokenPattern.test(joinKey??''))throw Error('This lobby link is incomplete or invalid.');
 return {room,joinKey};
}
export function createRoomClient({baseURL,fetch=globalThis.fetch,signal,crypto=globalThis.crypto}={}){
 const endpoint=new URL('/api/lobbies',baseURL),aborter=new AbortController();let identity,cursor=0,watching=false;
 const activeSignal=signal?AbortSignal.any([signal,aborter.signal]):aborter.signal;
 async function request(route,method='GET',body,timeoutMs=15000){
  const headers={};if(identity)headers.Authorization='Bearer '+identity.token;
  if(body)headers['Content-Type']='application/json';
  const response=await fetch(endpoint.href+route,{method,headers,body:body?JSON.stringify(body):undefined,signal:AbortSignal.any([activeSignal,AbortSignal.timeout(timeoutMs)]),cache:'no-store',credentials:'omit',redirect:'error'});
  const result=await response.json();if(!response.ok){const error=Error(result.error??'Lobby connection failed.');error.status=response.status;throw error;}
  if(result.version!==1)throw Error('Unsupported lobby connection service.');return result;
 }
 function adopt(value){if(identity)throw Error('This lobby connection is already assigned.');if(!roomPattern.test(value.room)||!tokenPattern.test(value.token)||!/^host$|^guest[1-9][0-9]{0,8}$/.test(value.peer))throw Error('Invalid lobby connection response.');identity=value;return {...identity};}
 async function pause(ms){await new Promise((resolve,reject)=>{
  activeSignal.throwIfAborted();const finish=()=>{clearTimeout(timer);activeSignal.removeEventListener('abort',cancel);};
  const timer=setTimeout(()=>{finish();resolve();},ms),cancel=()=>{finish();reject(activeSignal.reason);};activeSignal.addEventListener('abort',cancel,{once:true});
 });}
 return {
  get identity(){return identity?{...identity}:null;},
  async host(){if(identity)throw Error('Lobby already assigned.');return adopt(await request('','POST',{version:1}));},
  async join(link){if(identity)throw Error('Lobby already assigned.');if(!roomPattern.test(link?.room??'')||!tokenPattern.test(link?.joinKey??''))throw Error('Invalid lobby link.');return adopt(await request('/'+link.room+'/join','POST',{version:1,joinKey:link.joinKey}));},
  async send(to,kind,payload){if(!identity)throw Error('Connect to the lobby first.');return request('/'+identity.room+'/signals','POST',{version:1,to,kind,payload,id:crypto.randomUUID().replaceAll('-','')});},
  async removeGuest(peer){if(identity?.peer!=='host'||!/^guest[1-9][0-9]{0,8}$/.test(peer))throw Error('Host and guest identity required.');return request('/'+identity.room+'/players/'+peer,'DELETE');},
  async seal(){if(identity?.peer!=='host')throw Error('Host lobby required.');return request('/'+identity.room+'/seal','POST',{version:1});},
  async watch(onEvent){
   if(!identity||watching||typeof onEvent!=='function')throw Error('Assigned lobby event handler required.');watching=true;let failureAt;
   try{while(!activeSignal.aborted){
    let result;
    try{result=await request('/'+identity.room+'/events?after='+cursor);failureAt=undefined;}
    catch(error){if(activeSignal.aborted)throw error;if(error.status&&error.status<500&&error.status!==429)throw error;failureAt??=Date.now();if(Date.now()-failureAt>60000)throw Error('Lobby connection service is unavailable.');await pause(1000);continue;}
    if(!Array.isArray(result.events)||result.events.length>64||!Number.isSafeInteger(result.cursor)||result.cursor<cursor)throw Error('Invalid lobby event response.');
    for(const event of result.events){if(!Number.isSafeInteger(event.sequence)||event.sequence<=cursor||event.sequence>result.cursor)throw Error('Invalid lobby event order.');await onEvent(event);cursor=event.sequence;}
    if(cursor!==result.cursor)throw Error('Incomplete lobby event response.');
    await pause(result.events.length?0:750);
   }}finally{watching=false;}
  },
  async close(){
   // Local Stop must succeed if the host already closed the room, or the
   // service is offline. Its bounded expiry is the revocation fallback.
   try{if(identity&&!activeSignal.aborted)await request('/'+identity.room,'DELETE',undefined,1500);}catch{}
   finally{aborter.abort();identity=undefined;}
  },
 };
}
