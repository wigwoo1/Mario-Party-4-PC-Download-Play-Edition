"""Compile the actual m421 direction magnitude/normalization block."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/REL/m421Dll/player.c').read_text()
a=s.index('#ifdef TARGET_PC',s.index('void fn_1_7AA0(omObjData *object)\n{'))
b=s.index('    sp20.y =',a)
block=s[a:b]
code=r'''
#include <assert.h>
#include <math.h>
#define TARGET_PC 1
typedef struct {float x,y,z;} Vec;
static int calls;
static float VECMag(const Vec*v){return sqrtf(v->x*v->x+v->y*v->y+v->z*v->z);}
static void VECNormalize(const Vec*v,Vec*out){float m=VECMag(v);assert(m>0);calls++;*out=(Vec){v->x/m,v->y/m,v->z/m};}
static float check(Vec input){Vec sp8=input;float var_f28,var_f29;
'''+block+r'''
assert(isfinite(sp8.x)&&isfinite(sp8.y)&&isfinite(sp8.z));return var_f29;}
int main(void){assert(check((Vec){0,0,0})==0&&calls==0);assert(fabsf(check((Vec){3,0,4})-.0125f)<.00001f&&calls==1);assert(check((Vec){800,0,0})==1&&calls==2);return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 c=Path(d)/'direction.c';e=Path(d)/'direction.exe';c.write_text(code)
 subprocess.run(['cl','/nologo','/TC',str(c),'/Fe:'+str(e)],cwd=d,check=True)
 subprocess.run([str(e)],check=True)
print('PASS actual m421 direction: stationary, moving, and weight clamp')
