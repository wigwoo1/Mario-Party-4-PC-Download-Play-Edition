from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];work=root.parents[1]/'work'
source=(root/'src/REL/m444dll/pinball.c').read_text()
start=source.index('        arg1->y += 0.3;\n#if VERSION_REV2')
end=source.index('\n    }\n    if (arg0->x',start)
fixture=work/'pinball-velocity-test.cpp'
fixture.write_text('''#include <cassert>
#include <cmath>
#include <iostream>
#define TARGET_PC 1
#define VERSION_REV2 1
struct Vec{float x,y,z;};
static float VECMag(const Vec* v){return std::sqrt(v->x*v->x+v->y*v->y+v->z*v->z);}
static void step(Vec* arg1){
'''+source[start:end]+'''
}
int main(){Vec stalled{0,-0.3f,0};step(&stalled);assert(std::abs(stalled.y-0.3f)<0.000001f);
Vec moving{3,-0.3f,4};step(&moving);assert(moving.x==3&&moving.z==4&&std::abs(moving.y)<0.000001f);
Vec falling{0,2,0};step(&falling);assert(std::abs(falling.y-2.3f)<0.000001f);
std::cout<<"PASS actual pinball velocity fallback uses pointed vector; stalled/moving/falling cases\\n";}
''')
if '--wasm' in sys.argv:
    output=work/'pinball-velocity-test.js'
    subprocess.run([sys.executable,str(work/'emsdk/upstream/emscripten/em++.py'),str(fixture),'-std=c++20','-sENVIRONMENT=node','-o',str(output)],check=True)
    subprocess.run(['node',str(output)],check=True)
else:
    subprocess.run(['cl','/nologo','/std:c++20','/EHsc',str(fixture),'/Fe'+str(work/'pinball-velocity-test.exe'),'/Fo'+str(work/'pinball-velocity-test.obj')],check=True)
    subprocess.run([str(work/'pinball-velocity-test.exe')],check=True)
