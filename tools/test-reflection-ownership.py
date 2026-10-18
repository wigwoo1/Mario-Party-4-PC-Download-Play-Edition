from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[1]
s=(root/'src/game/hsfman.c').read_text()
start=s.index('    // Decode the requested map')
end=s.index('\n#endif',start)
setter=s[start:end]
start=s.index('    if (reflectAnim[0] != defaultReflectAnim)',s.index('void Hu3DAllKill'))
end=s.index('\n#endif',start)
reset=s[start:end]
work=root.parents[1]/'work'
f=work/'reflection-ownership-test.cpp'
f.write_text('''#include <cassert>
#include <iostream>
struct ANIMDATA {int useNum; bool decoded; bool freed;};
ANIMDATA *reflectAnim[5], *defaultReflectAnim; int reflectMapNo;
ANIMDATA* HuSprAnimRead(ANIMDATA* a){assert(!a->freed);if(a->decoded)++a->useNum;else{a->decoded=true;a->useNum=0;}return a;}
void HuSprAnimLock(ANIMDATA* a){assert(!a->freed);++a->useNum;}
void HuSprAnimKill(ANIMDATA* a){assert(!a->freed);if(--a->useNum<=0)a->freed=true;}
void set(ANIMDATA* arg0){
'''+setter+'''
reflectMapNo=0;}
void reset(){
'''+reset+'''
}
int main(){ANIMDATA base{1,true,false},a{0,false,false},b{0,false,false};defaultReflectAnim=&base;reflectAnim[0]=&base;
set(&a);assert(a.useNum==1&&base.useNum==1);set(&a);assert(a.useNum==1&&!a.freed);
set(&b);assert(a.freed&&b.useNum==1);set(&base);assert(b.freed&&base.useNum==1);set(&base);assert(base.useNum==1);
ANIMDATA shared{1,true,false};set(&shared);assert(shared.useNum==2);reset();assert(shared.useNum==1&&!shared.freed&&reflectAnim[0]==&base);reset();assert(base.useNum==1&&!base.freed);
ANIMDATA c{0,false,false};set(&c);reset();assert(c.freed&&base.useNum==1&&reflectMapNo==0);
std::cout<<"PASS extracted production reflection replacement/reset ownership; decoder refcount contract fixture only\\n";
}
''')
if '--wasm' in sys.argv:
    output=work/'reflection-ownership-test.js'
    subprocess.run([sys.executable,str(work/'emsdk/upstream/emscripten/em++.py'),str(f),'-std=c++20','-sENVIRONMENT=node','-o',str(output)],check=True)
    subprocess.run(['node',str(output)],check=True)
else:
    subprocess.run(['cl.exe','/nologo','/std:c++20','/EHsc',str(f),'/Fe'+str(work/'reflection-ownership-test.exe'),'/Fo'+str(work/'reflection-ownership-test.obj')],check=True)
    subprocess.run([str(work/'reflection-ownership-test.exe')],check=True)
