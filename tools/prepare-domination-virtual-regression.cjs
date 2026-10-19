// Generate standalone C fixtures from the current production callbacks.
// Compile each with assertions enabled. The baseline must fail with both the
// idle and opening arguments; the fixed fixture must pass all nine cases.
// Native C11 and Emscripten/Node are supported; this never runs a game DLL.
const fs=require('node:fs'),assert=require('node:assert/strict');
const path=require('node:path'),crypto=require('node:crypto');
const project=path.resolve(__dirname,'..'),output=process.argv[2];
if(!output)throw Error('Usage: node tools/prepare-domination-virtual-regression.cjs OUTPUT_DIRECTORY');
fs.mkdirSync(output,{recursive:true});
const source=fs.readFileSync(path.join(project,'src/REL/m407Dll/whomp.c'));
const fixed=source.toString('utf8').replaceAll('\r\n','\n');
function extract(s,name){const start=s.indexOf('void '+name+'(omObjData *arg0)\n{');assert.ok(start>=0,name);let i=s.indexOf('{',start),level=0;for(;i<s.length;i++){if(s[i]==='{')level++;if(s[i]==='}'&&!--level)return s.slice(start,i+1)+'\n';}throw Error(name);}
const state=fixed.slice(fixed.indexOf('typedef struct unkDominationData7'),fixed.indexOf('// function signatures'));
const prefix=`#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define TARGET_PC 1
typedef int16_t s16;typedef int32_t s32;typedef uint32_t u32;typedef float f32;
typedef struct {float x,y,z;} Vec;
typedef struct {s16 *model,*motion;void *data;Vec trans,rot;} omObjData;
typedef void (*ObjFuncs)(omObjData *);
${state}
#define HU3D_ATTR_DISPOFF 1
#define HU3D_MOTATTR_LOOP 0x40000001u
#define HU3D_MOTATTR_PAUSE 0x40000002u
#define HU3D_MOTATTR_REV 0x40000004u
static ObjFuncs lbl_1_data_258[5];static omObjData *lbl_1_bss_2DB0[644];static s16 lbl_1_bss_1E98[644][3],lbl_1_bss_68[644][6];static Vec Center;
static unsigned attrs,motions,checks,shadows,effects;static int ended,effectType;
static void valid(s16 model){assert(model>=0&&model<512);}
void Hu3DModelAttrSet(s16 m,u32 a){(void)a;valid(m);attrs++;}
void Hu3DModelAttrReset(s16 m,u32 a){(void)a;valid(m);attrs++;}
void Hu3DMotionSet(s16 m,s16 motion){valid(m);assert(motion>=0);motions++;}
s32 Hu3DMotionEndCheck(s16 m){valid(m);checks++;return ended;}
void Hu3DModelShadowDispOn(s16 m){valid(m);shadows++;}
void Hu3DModelShadowDispOff(s16 m){valid(m);shadows++;}
void Hu3DModelPosSet(s16 m,float x,float y,float z){(void)x;(void)y;(void)z;valid(m);}
void omSetTra(omObjData *o,float x,float y,float z){o->trans=(Vec){x,y,z};}
void omSetRot(omObjData *o,float x,float y,float z){o->rot=(Vec){x,y,z};}
void fn_1_4544(s32 type,s32 delay,f32 x,f32 y,f32 z){(void)delay;(void)x;(void)y;(void)z;effects++;effectType=type;}
void fn_1_290C(omObjData *o,s32 state){unkDominationData7 *s=o->data;s->unk_24=state;s->unk_2C=0;}
static void idle(omObjData *o){(void)o;}
`;
const tests=`
static omObjData object;static unkDominationData7 data;static s16 models[3],motionIds[6];
static omObjData *reset(void){memset(&object,0,sizeof(object));memset(&data,0,sizeof(data));models[0]=models[1]=models[2]=-1;for(int i=0;i<6;i++){motionIds[i]=-1;lbl_1_bss_68[0][i]=8;}object.model=models;object.motion=motionIds;object.data=&data;data.unk_08=(Vec){12,0,5000};data.unk_14=(Vec){1,2,3};data.unk_00=80;attrs=motions=checks=shadows=effects=0;ended=0;effectType=-1;lbl_1_data_258[0]=idle;lbl_1_data_258[1]=fn_1_3364;for(int i=0;i<644;i++)lbl_1_bss_2DB0[i]=&object;return &object;}
int main(int argc,char **argv){omObjData *o;
 o=reset();if(argc>1&&strcmp(argv[1],"opening")==0)data.unk_24=1;fn_1_2930(o);assert(attrs==0&&motions==0&&checks==0);assert(o->trans.x==12&&o->trans.z==5000&&o->rot.y==2);
 o=reset();data.unk_24=1;fn_1_2930(o);assert(data.unk_24==0&&data.unk_08.y==-300&&effects==1&&effectType==0&&motions==0&&checks==0&&attrs==0);
 o=reset();data.unk_24=1;data.unk_28=1;fn_1_3364(o);assert(data.unk_24==0&&effects==1&&effectType==1&&motions==0&&checks==0);
 o=reset();models[0]=7;data.unk_04=1;fn_1_2930(o);assert(attrs==1&&motions==0&&o->trans.z==5000);
 o=reset();models[0]=7;data.unk_04=1;data.unk_08.z=0;fn_1_2930(o);assert(attrs==1&&o->trans.z==0);
 o=reset();models[0]=7;motionIds[4]=8;data.unk_24=1;fn_1_3364(o);assert(motions==1&&checks==1&&effects==1&&data.unk_24==1&&data.unk_2C==1&&data.unk_08.y==0);
 ended=1;fn_1_3364(o);assert(data.unk_24==0&&data.unk_08.y==-300&&checks==2&&effects==1);
 o=reset();data.unk_08.z=0;lbl_1_bss_1E98[0][0]=9;fn_1_2930(o);assert(models[0]==9&&data.unk_04==1&&shadows==1&&attrs==3&&motions==1&&motionIds[5]==8&&o->trans.x==12);
 o=reset();data.unk_24=1;data.unk_2C=5;fn_1_3364(o);assert(data.unk_24==0&&data.unk_08.y==-300&&effects==0&&motions==0&&checks==0);
 puts("PASS 9 exact Domination callback scenarios: virtual rows, opening effects, resident motion wait/end, and model handoff");return 0;}
`;
for(const name of ['baseline','fixed']){
 // The baseline removes the port guards from these exact callbacks. It must
 // fail for both idle visibility and the opening transition's missing model.
 const setup=name==='baseline'?prefix.replace('#define TARGET_PC 1',''):prefix;
 fs.writeFileSync(path.join(output,'domination-virtual-'+name+'.c'),setup+extract(fixed,'fn_1_3364')+extract(fixed,'fn_1_2930')+tests,{flag:'wx'});
}
fs.writeFileSync(path.join(output,'source.json'),JSON.stringify({source:'src/REL/m407Dll/whomp.c',sha256:crypto.createHash('sha256').update(source).digest('hex'),fixedCases:9,baselineCases:['idle','opening'],scope:'Exact production callbacks with isolated model/API fixtures; not a whole-game or console executable test.'},null,2)+'\n',{flag:'wx'});
console.log('Generated exact production callback regressions and two baseline negative controls');
