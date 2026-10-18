const fs=require('fs');const s=fs.readFileSync('outputs/partyboard/src/REL/m417Dll/water.c','utf8').replace(/\r\n/g,'\n');function fun(name){const start=s.indexOf('void '+name+'(omObjData *object)\n{');if(start<0)throw Error(name);let p=s.indexOf('{',start),depth=1,i=p+1;for(;depth;i++){if(s[i]==='{')depth++;if(s[i]==='}')depth--;}return s.slice(start,i);}
const structs=s.slice(s.indexOf('typedef struct UnkM417Struct5'),s.indexOf('float fn_1_2F08'));
const pre=`#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef short s16;typedef int s32;typedef unsigned int u32;typedef unsigned short u16;typedef struct{float x,y,z;}Vec;typedef struct{float x,y;}Vec2f;typedef struct{unsigned char r,g,b,a;}GXColor;typedef struct{int unused;}omObjData;
#define REFRESH_FREQ 1.0f
#define HEAP_SYSTEM 0
#define MEMORY_DEFAULT_NUM 0
#define HU_FB_WIDTH 640
#define HU_FB_HEIGHT 480
#define GX_FALSE 0
#define HuMemDirectMallocNum(a,b,c) calloc(1,b)
#define HuMemDirectFree free
#define DCFlushRange(a,b) ((void)0)
#define DCStoreRangeNoSync(a,b) ((void)0)
#define PPCSync() ((void)0)
#define GXGetTexBufferSize(a,b,c,d,e) (a*b*2)
static float sind(float d){return sinf(d*0.017453292f);}static float VECMag(Vec*v){return sqrtf(v->x*v->x+v->y*v->y+v->z*v->z);}static void VECScale(Vec*a,Vec*b,float f){*b=(Vec){a->x*f,a->y*f,a->z*f};}static void VECSubtract(Vec*a,Vec*b,Vec*c){*c=(Vec){a->x-b->x,a->y-b->y,a->z-b->z};}static void VECAdd(Vec*a,Vec*b,Vec*c){*c=(Vec){a->x+b->x,a->y+b->y,a->z+b->z};}static void VECCrossProduct(Vec*a,Vec*b,Vec*c){*c=(Vec){a->y*b->z-a->z*b->y,a->z*b->x-a->x*b->z,a->x*b->y-a->y*b->x};}
${structs}
static UnkM417Struct4 lbl_1_bss_178;static int zeros,interiorZeros,faceZeros;
static void VECNormalize(Vec*a,Vec*b){float m=(a->z*a->z)+((a->x*a->x)+(a->y*a->y));assert(isfinite(m));if(m==0){zeros++;Vec*base=lbl_1_bss_178.unk_6C4[lbl_1_bss_178.unk_6B4];if(a>=base&&a<base+1080){if(a<base+lbl_1_bss_178.unk_18)interiorZeros++;}else faceZeros++;return;}m=1.0f/sqrtf(m);*b=(Vec){a->x*m,a->y*m,a->z*m};assert(isfinite(b->x)&&isfinite(b->y)&&isfinite(b->z));assert(fabsf(b->x*b->x+b->y*b->y+b->z*b->z-1.0f)<0.00001f);}
static void fn_1_4E64(omObjData*object){}static void fn_1_5E3C(omObjData*object){}void fn_1_57B0(omObjData*object);
`;
fs.writeFileSync('work/m417-water-wave-fixture.c',pre+fun('fn_1_3D58')+'\n'+fun('fn_1_57B0')+'\nint main(){omObjData o;fn_1_3D58(&o);printf("startup zero=%d interior=%d face=%d\\n",zeros,interiorZeros,faceZeros);for(int i=0;i<600;i++)fn_1_57B0(&o);printf("601 updates zero=%d interior=%d face=%d\\n",zeros,interiorZeros,faceZeros);return 0;}');


