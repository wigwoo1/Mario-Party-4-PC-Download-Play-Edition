"""Compile the actual light setter; run in a Visual Studio developer shell."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/game/hsfman.c').read_text()
start = source.index('static inline void Hu3DLightPosSetV(')
end = source.index('\n}\n', start) + 3
scalar_start = source.index('static inline void Hu3DLightPosSet(')
scalar_end = source.index('\n}\n', scalar_start) + 3
aim_start = source.index('static inline void Hu3DLightPosAimSetV(')
aim_end = source.index('\n}\n', aim_start) + 3
prefix = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#define TARGET_PC 1
typedef unsigned char u8;
typedef float f32;
typedef struct {float x,y,z;} Vec;
typedef struct {int type; Vec pos,dir;} HU3DLIGHT;
static int normalized;
static float VECSquareMag(const Vec *v){return v->x*v->x+v->y*v->y+v->z*v->z;}
static void VECSubtract(const Vec *a,const Vec *b,Vec *out){out->x=a->x-b->x;out->y=a->y-b->y;out->z=a->z-b->z;}
static void VECNormalize(const Vec *v,Vec *out){
 ++normalized; float mag=sqrtf(VECSquareMag(v));
 if(mag){out->x=v->x/mag;out->y=v->y/mag;out->z=v->z/mag;}
}
'''
suffix = r'''
int main(void){
 Vec pos={930,400,-1000},zero={0,0,0},direction={0,3,4};
 HU3DLIGHT light={2,{0,0,0},{1,2,3}};
 Hu3DLightPosSetV(&light,&pos,&zero);
 assert(normalized==0 && light.dir.x==0 && light.dir.y==0 && light.dir.z==0);
 assert(light.pos.x==930 && light.pos.y==400 && light.pos.z==-1000);
 light.type=0x8002;Hu3DLightPosSetV(&light,&pos,&zero);assert(normalized==0);
 Hu3DLightPosSetV(&light,&pos,&direction);
 assert(normalized==1 && fabsf(light.dir.y-.6f)<1e-6f && fabsf(light.dir.z-.8f)<1e-6f);
 for(int type=0;type<2;type++){
  light.type=type;Hu3DLightPosSetV(&light,&pos,&direction);
  Hu3DLightPosSetV(&light,&pos,&zero);
 }
 assert(normalized==5); /* Other light types retain the original normalization path. */
 for(int type=0;type<3;type++){
  light.type=type;
  Hu3DLightPosSet(&light,930,400,-1000,0,0,0);
  assert(normalized==5+type && light.dir.x==0 && light.dir.y==0 && light.dir.z==0);
  assert(light.pos.x==930 && light.pos.y==400 && light.pos.z==-1000);
  Hu3DLightPosSet(&light,930,400,-1000,0,3,4);
  assert(fabsf(light.dir.y-.6f)<1e-6f && fabsf(light.dir.z-.8f)<1e-6f);
 }
 assert(normalized==8);
 /* m427 first sets position with no direction, then immediately assigns aim. */
 Vec aim={930,400,-1050};
 Hu3DLightPosSet(&light,pos.x,pos.y,pos.z,0,0,0);
 assert(normalized==8);
 Hu3DLightPosAimSetV(&light,&pos,&aim);
 assert(normalized==9 && light.dir.x==0 && light.dir.y==0 && light.dir.z==-1);
 assert(light.pos.x==pos.x && light.pos.y==pos.y && light.pos.z==pos.z);
 assert(direction.y==3 && direction.z==4 && zero.x==0 && pos.x==930);
 puts("PASS: zero point directions, scalar position then aim sequence, nonzero normalization and unchanged inputs");
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='partyboard-point-light-') as directory:
    base = Path(directory)
    fixture = base / 'point_light.c'
    fixture.write_text(prefix + source[start:end] + source[scalar_start:scalar_end] + source[aim_start:aim_end] + suffix)
    executable = base / 'point_light.exe'
    subprocess.run(['cl', '/nologo', str(fixture), '/Fe:' + str(executable),
                    '/Fo:' + str(base / 'point_light.obj')], check=True)
    subprocess.run([str(executable)], check=True)
