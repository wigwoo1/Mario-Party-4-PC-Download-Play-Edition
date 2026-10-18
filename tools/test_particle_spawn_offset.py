"""Compile the actual particle spawn offset block in a VS developer shell."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/game/hsfanim.c').read_text()
a=s.index('#ifdef TARGET_PC', s.index('vel.z = (s32)frandmod', s.index('static void ParManFunc(void)\n{')))
b=s.index('                        VECNormalize(&parManP->vec', a)
block=s[a:b].replace('param->scaleRange','range').replace('&particleDataP->pos','&pos')
code=r"""
#include <assert.h>
#include <math.h>
#define TARGET_PC 1
typedef struct {float x,y,z;} Vec;
static int calls;
static float VECSquareMag(const Vec*v){return v->x*v->x+v->y*v->y+v->z*v->z;}
static void VECNormalize(const Vec*v,Vec*out){float m=sqrtf(VECSquareMag(v));assert(m>0);calls++;*out=(Vec){v->x/m,v->y/m,v->z/m};}
static void VECScale(const Vec*v,Vec*out,float s){*out=(Vec){v->x*s,v->y*s,v->z*s};}
static void VECAdd(const Vec*a,const Vec*b,Vec*out){*out=(Vec){a->x+b->x,a->y+b->y,a->z+b->z};}
static Vec spawn(Vec vel,float range){Vec pos={10,20,30};
"""+block+r"""
return pos;}
int main(void){
Vec p=spawn((Vec){0,0,0},0);assert(calls==0&&p.x==10&&p.y==20&&p.z==30);
p=spawn((Vec){0,0,0},5);assert(calls==0&&p.x==10&&p.y==20&&p.z==30);
p=spawn((Vec){3,4,0},10);assert(calls==1&&p.x==16&&p.y==28&&p.z==30);
p=spawn((Vec){-3,0,4},5);assert(calls==2&&p.x==7&&p.y==20&&p.z==34);
return 0;}
"""
with tempfile.TemporaryDirectory() as d:
 c=Path(d)/'spawn.c';e=Path(d)/'spawn.exe';c.write_text(code)
 subprocess.run(['cl','/nologo','/TC',str(c),'/Fe:'+str(e)],cwd=d,check=True)
 subprocess.run([str(e)],check=True)
print('PASS actual particle offset: zero range, zero random offset, and nonzero directions')
