// Isolated HTTP origin for real two-browser asset transfer/control acceptance.
const http=require('node:http'),fs=require('node:fs'),path=require('node:path'),{spawn}=require('node:child_process');
if(!process.env.PARTYBOARD_TEST_TRANSFER_FILE)throw Error('Explicit local game file required');
if(!process.env.PARTYBOARD_DOWNLOAD_RUNTIME_DIRECTORY)throw Error('Explicit browser runtime directory required');
const server=http.createServer((req,res)=>{
 const pathname=new URL(req.url,'http://localhost').pathname;
 const runtimeFile=['/contract.mjs','/contract.wasm'].includes(pathname);
 const root=runtimeFile?path.resolve(process.env.PARTYBOARD_DOWNLOAD_RUNTIME_DIRECTORY):__dirname;
 const file=path.resolve(root,pathname==='/'?'index.html':'.'+pathname);
 if(!file.startsWith(root+path.sep)){res.writeHead(403);res.end();return;}
 const type=path.extname(file);res.setHeader('Content-Type',({'.wasm':'application/wasm','.mjs':'text/javascript','.js':'text/javascript','.html':'text/html','.css':'text/css','.opus':'audio/ogg'})[type]||'application/octet-stream');
 fs.createReadStream(file).on('error',error=>{console.error('Diagnostic asset read failed: '+pathname+' '+error.code);if(!res.headersSent)res.writeHead(404);res.end();}).pipe(res);
});
server.listen(0,'127.0.0.1',()=>{
 const child=spawn(process.execPath,[path.join(__dirname,'browser-test.cjs')],{stdio:'inherit',env:{...process.env,PARTYBOARD_TEST_URL:`http://127.0.0.1:${server.address().port}`,PARTYBOARD_TEST_DOWNLOAD_CONTROLS:'1'}});
 child.on('error',error=>{console.error(error);server.close();process.exitCode=1;});
 child.on('exit',code=>{server.close();process.exitCode=code??1;});
});
