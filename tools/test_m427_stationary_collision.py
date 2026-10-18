"""Exercise the production collision function in a Visual Studio developer shell."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/REL/m427Dll/player.c').read_text()
start = source.index('s32 fn_1_15AF0(s32 arg0, Vec *arg1, Vec *arg2, Vec *arg3, Vec *arg4)\n{')
end = source.index('\n}\n', start) + 3
prefix = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#define TARGET_PC 1
typedef int s32;
typedef struct {float x,y,z;} Vec;
typedef struct {Vec unk_44;float unk_50,unk_54,unk_58,unk_5C,unk_64,unk_60,unk_68;void *unk_14,*unk_2C;} M427DllBss590Struct;
static M427DllBss590Struct lbl_1_bss_590[2];
static int lbl_1_bss_580=2;
static s32 fn_1_155E0(float x,float y,float z,float *t){*t=0;return 0;}
static void fn_1_17048(void *wall,void *unused,Vec *pos,Vec *out){*out=*pos;out->x=wall==lbl_1_bss_590[0].unk_14?-10:10;}
static void VECNormalize(Vec *v,Vec *out){assert(0 && "unused normalization must not run");}
'''
suffix = r'''
int main(void){
 lbl_1_bss_590[0].unk_5C=lbl_1_bss_590[0].unk_60=10;
 lbl_1_bss_590[0].unk_14=(void *)1;lbl_1_bss_590[0].unk_2C=(void *)2;
 for(int moving=0;moving<2;moving++)for(int x=-20;x<=20;x+=20){
  Vec pos={(float)x,2,3},old=pos,out,correction;
  if(moving)old.z-=5;
  int hit=fn_1_15AF0(-1,&old,&pos,&out,&correction);
  assert(hit==(x!=0));assert(out.x==(x<0?-10:x>0?10:0));
  assert(out.y==2 && out.z==3);assert(correction.x==out.x-pos.x);
  assert(correction.y==0 && correction.z==0);
 }
 puts("PASS stationary and moving players: inside bounds, left/right collision correction");
}
'''
with tempfile.TemporaryDirectory(prefix='partyboard-m427-collision-') as directory:
    base = Path(directory)
    fixture = base / 'collision.c'
    fixture.write_text(prefix + source[start:end] + suffix)
    executable = base / 'collision.exe'
    subprocess.run(['cl', '/nologo', str(fixture), '/Fe:' + str(executable), '/Fo:' + str(base / 'collision.obj')], check=True)
    subprocess.run([str(executable)], check=True)
