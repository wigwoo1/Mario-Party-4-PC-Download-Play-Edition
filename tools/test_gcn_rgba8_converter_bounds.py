"""Exercise the production GCN RGBA8 converter with guarded output memory.

Run from a scratch directory with work/ and outputs/validation/ writable.
Set CLANG_EXE and NODE_EXE when those tools are not on PATH. The converter
and mip sizing are extracted unchanged; the GPU upload path is not modeled.
"""
import pathlib, subprocess, json, hashlib, sys, os, shutil
root=pathlib.Path(__file__).resolve().parents[1]
source=root/'extern/aurora/lib/gfx/texture_convert.cpp'
s=source.read_text()
def extract(name):
    start=s.index('static ',s.index(name)-30)
    brace=s.index('{',start); depth=1; end=brace+1
    while depth:
        depth+=(s[end]=='{')-(s[end]=='}');end+=1
    return s[start:end]
header='''
typedef unsigned char uint8_t; typedef unsigned int uint32_t; typedef __SIZE_TYPE__ size_t;
struct RGBA8 { uint8_t r,g,b,a; };
static uint8_t output[262144],input[262144];
struct ByteBuffer { size_t n; ByteBuffer(size_t size):n(size){for(size_t i=0;i<sizeof(output);++i)output[i]=0xA5;} uint8_t* data(){return output;} };
template<class T> struct ArrayRef {const T* p; const T* data(){return p;} };
'''
test='''
static unsigned value(unsigned mip,unsigned x,unsigned y,unsigned c){return (mip*71+x*13+y*29+c*53)&255;}
extern "C" int check(unsigned width,unsigned height,unsigned mips){
 unsigned w=width,h=height,off=0;
 for(unsigned m=0;m<mips;++m){
  for(unsigned by=0;by<h;by+=4)for(unsigned bx=0;bx<w;bx+=4)
   for(unsigned c=0;c<2;++c)for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x){
    input[off++]=value(m,bx+x,by+y,c?1:3);input[off++]=value(m,bx+x,by+y,c?2:0);
   }
  if(w>1)w/=2;if(h>1)h/=2;
 }
 auto result=BuildRGBA8FromGCN(width,height,mips,ArrayRef<uint8_t>{input});
 int failures=0;
 for(size_t i=result.n;i<result.n+4096;++i)if(output[i]!=0xA5){failures|=1;break;}
 off=0;w=width;h=height;
 for(unsigned m=0;m<mips;++m){
  for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)for(unsigned c=0;c<4;++c)
   if(output[off++]!=value(m,x,y,c))failures|=2;
  if(w>1)w/=2;if(h>1)h/=2;
 }
 return failures;
}
'''
phase=sys.argv[1] if len(sys.argv)>1 else 'green'
assert phase in ('red','green')
pathlib.Path('work').mkdir(exist_ok=True)
pathlib.Path('outputs/validation').mkdir(parents=True,exist_ok=True)
cpp=pathlib.Path(f'work/rgba8-converter-{phase}-20261023.cpp');wasm=cpp.with_suffix('.wasm')
cpp.write_text(header+extract('ComputeMippedTexelCount(')+extract('BuildRGBA8FromGCN(')+test)
clang=os.environ.get('CLANG_EXE') or shutil.which('clang')
assert clang, 'Set CLANG_EXE to a clang compiler with wasm32 support'
subprocess.run([clang,'--target=wasm32','-x','c++','-std=c++17','-O0','-nostdlib','-fno-exceptions','-fno-rtti','-Wl,--no-entry','-Wl,--export=check','-Wl,--export-memory',str(cpp),'-o',str(wasm)],check=True)
runner="""const fs=require('fs');(async()=>{const {instance}=await WebAssembly.instantiate(fs.readFileSync(process.argv[1]));const cases=[];for(const [w,h] of [[216,58],[216,36],[216,62],[1,1],[2,3],[3,2],[4,4],[5,7],[7,5],[32,32],[64,64],[217,59]])for(const mips of [1,2,3])cases.push({w,h,mips,result:instance.exports.check(w,h,mips)});console.log(JSON.stringify(cases));})();"""
node=os.environ.get('NODE_EXE') or shutil.which('node')
assert node, 'Set NODE_EXE to a Node.js executable'
cases=json.loads(subprocess.check_output([node,'-e',runner,str(wasm)],text=True))
r={'phase':phase,'sourceSha256':hashlib.sha256(source.read_bytes()).hexdigest(),'scope':'Exact extracted production converter and mip sizing; guarded output allocation replaces ByteBuffer. Independent coordinate/channel oracle; no GPU execution.','cases':cases,'failed':sum(c['result']!=0 for c in cases)}
pathlib.Path(f'outputs/validation/rgba8-converter-{phase}-20261023.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r))
if phase=='green':assert r['failed']==0
else:assert r['failed']>0
