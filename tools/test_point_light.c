#include "game/hu3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void) {
 Vec p={0,0,0},d={0,0,0}; GXColor c={0,0,0,255};
 Hu3DLightAllKill();
 s16 id=Hu3DGLightCreateV(&p,&d,&c);assert(id==0);
 assert(Hu3DGlobalLight[id].dir.x==0 && Hu3DGlobalLight[id].dir.y==0 && Hu3DGlobalLight[id].dir.z==0);
 Hu3DGLightPointSet(id,0,0,3);assert((Hu3DGlobalLight[id].type&255)==2);
 d=(Vec){0,-0.5f,-1};id=Hu3DGLightCreateV(&p,&d,&c);assert(id==1);
 Vec n=Hu3DGlobalLight[id].dir;assert(isfinite(n.x)&&isfinite(n.y)&&isfinite(n.z));assert(fabsf(n.x*n.x+n.y*n.y+n.z*n.z-1)<0.00001f);
 assert(d.y==-0.5f&&d.z==-1);
 puts("Actual shared light constructors: zero point-light initialization and nonzero unit direction PASS");return 0;
}
