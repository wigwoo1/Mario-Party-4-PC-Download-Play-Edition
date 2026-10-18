const fs=require('fs');const source=fs.readFileSync('outputs/partyboard/src/REL/m430Dll/player.c','utf8');const start=source.indexOf('static s32 portListenerActive;'),end=source.indexOf('\n#endif',start);if(start<0||end<0)throw Error('Actual production listener helper required');const fixture=`#include <assert.h>
#include <math.h>
typedef int s32; typedef struct {float x,y,z;} Vec;
#define REFRESH_RATE_F 60.0f
static int sets,updates,normalizations;static Vec seenPos,seenHeading;
static void VECNormalize(Vec *a,Vec *b){float m=sqrtf(a->x*a->x+a->y*a->y+a->z*a->z);assert(m>0);++normalizations;b->x=a->x/m;b->y=a->y/m;b->z=a->z/m;}
static void HuAudFXListnerSetEX(Vec *p,Vec *h,float d,float speed,float start,float front,float back){assert(d==100000 && speed==34000.0f/60 && start==0 && front==300 && back==300);++sets;seenPos=*p;seenHeading=*h;}
static void HuAudFXListnerUpdate(Vec *p,Vec *h){++updates;seenPos=*p;seenHeading=*h;}
${source.slice(start,end)}
int main(void){Vec p={12,34,56},h={0,0,0};PortListenerApply(&p,&h);assert(!sets&&!updates&&!normalizations&&!portListenerActive);h=(Vec){0,0,-700};PortListenerApply(&p,&h);assert(sets==1&&!updates&&portListenerActive&&seenHeading.z==-1&&seenPos.x==12);h=(Vec){3,4,0};PortListenerApply(&p,&h);assert(sets==1&&updates==1&&fabsf(seenHeading.x-.6f)<.00001f&&fabsf(seenHeading.y-.8f)<.00001f);h=(Vec){0,0,0};PortListenerApply(&p,&h);assert(sets==1&&updates==1&&normalizations==2);h=(Vec){0,700,0};PortListenerApply(&p,&h);assert(sets==1&&updates==2&&seenHeading.y==1);return 0;}
`;fs.writeFileSync('work/m430-listener-fixture.c',fixture);
