from pathlib import Path
import subprocess
import sys

root=Path(__file__).resolve().parents[1]
source=(root/'src/REL/m406Dll/player.c').read_text()
start=source.index('                    sp20.x = sp2C[var_r27 - 1]->trans.x - lbl_1_bss_E0[1].x;')
end=source.index('                    fn_1_12B38(&lbl_1_bss_E0[1]);',start)
block=source[start:end]
workspace=root.parents[1]/'work'
fixture=workspace/'m406-tracking-regression.cpp'
fixture.write_text('''#include <cmath>
#include <stdexcept>
#include <iostream>
struct Vec {float x,y,z;}; struct Object {Vec trans;};
static unsigned zeros=0;
float VECMag(Vec* p){return std::sqrt(p->x*p->x+p->y*p->y+p->z*p->z);}
void VECNormalize(Vec* p,Vec* q){float m=VECMag(p);if(m==0){++zeros;return;}*q={p->x/m,p->y/m,p->z/m};}
Vec track(Vec target,Vec current){Object object{target};Object* sp2C[]={&object};int var_r27=1;Vec lbl_1_bss_E0[2]={{},current};Vec sp20;float var_f29;
'''+block+'''return lbl_1_bss_E0[1];}
void require(bool ok){if(!ok)throw std::runtime_error("Tracking regression failed");}
int main(){
for(float distance:{0.f,10.f,99.f,100.f,200.f}){
auto value=track({distance,0,0},{0,0,0});require(value.x==std::fmin(distance,100.f)&&value.y==0&&value.z==0);
}
auto diagonal=track({300,400,0},{0,0,0});require(std::fabs(diagonal.x-60)<0.0001f&&std::fabs(diagonal.y-80)<0.0001f);
#ifdef TARGET_PC
require(zeros==0);
#else
require(zeros==1);
#endif
std::cout<<"PASS actual m406 tracking block; zero calls="<<zeros<<"\\n";
}
''')
for fixed in (False,True):
    wasm='--wasm' in sys.argv
    name='m406-tracking-fixed' if fixed else 'm406-tracking-original'
    exe=workspace/(name+('.js' if wasm else '.exe'))
    if wasm:
        command=[sys.executable,str(workspace/'emsdk/upstream/emscripten/em++.py'),str(fixture),'-std=c++20','-fexceptions','-sENVIRONMENT=node','-o',str(exe)]
        if fixed:command.append('-DTARGET_PC')
    else:
        command=['cl.exe','/nologo','/std:c++20','/EHsc',str(fixture),'/Fe'+str(exe),'/Fo'+str(workspace/'m406-tracking.obj')]
        if fixed:command.append('/DTARGET_PC')
    subprocess.run(command,check=True)
    subprocess.run(['node',str(exe)] if wasm else [str(exe)],check=True)
