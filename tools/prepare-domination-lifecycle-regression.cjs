// Compile these extracted production callbacks on Windows and in Wasm. No game
// DLL or disc is used. API traps detect invalid models; baseline controls remove
// TARGET_PC guards to reproduce growth, falling and pooled-motion failures.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const output=process.argv[2];
if(!output)throw Error('Usage: node tools/prepare-domination-lifecycle-regression.cjs OUTPUT_DIRECTORY');
fs.mkdirSync(output,{recursive:true});
const bytes=fs.readFileSync(path.resolve(__dirname,'../src/REL/m407Dll/whomp.c'));
const source=bytes.toString('utf8').replaceAll('\r\n','\n');
function extract(name){const match=new RegExp('(?:void|s32|s16) '+name+'\\([^;]*?\\)\\n\\{').exec(source);assert.ok(match,name);let i=source.indexOf('{',match.index),depth=0;for(;i<source.length;i++){if(source[i]==='{')depth++;if(source[i]==='}'&&!--depth)return source.slice(match.index,i+1)+'\n';}throw Error(name);}
const data=source.slice(source.indexOf('typedef struct unkDominationData7'),source.indexOf('// function signatures'));
const callbacks=['fn_1_290C','fn_1_3360','fn_1_3364','fn_1_34A4','fn_1_36D0','fn_1_37B0','fn_1_2930','fn_1_26CC','fn_1_25C0','fn_1_2880','fn_1_31F8'];
const prefix=`#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define TARGET_PC 1
#define ARRAY_COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define MAX_WHOMPS_PLAYER 160
typedef int16_t s16;typedef int32_t s32;typedef uint32_t u32;typedef uint8_t u8;typedef float f32;
typedef struct {float x,y,z;} Vec;
typedef struct {s16 *model,*motion;void *data;Vec trans,rot;} omObjData;
typedef void (*ObjFuncs)(omObjData *);
${data}
#define HU3D_ATTR_DISPOFF 1u
#define HU3D_MOTATTR_LOOP 2u
#define HU3D_MOTATTR_PAUSE 4u
#define HU3D_MOTATTR_REV 8u
static unsigned GWPlayer[4];
static ObjFuncs lbl_1_data_258[5];static omObjData *lbl_1_bss_2DB0[644];
static s16 lbl_1_bss_1E98[644][3],lbl_1_bss_68[644][6];
static s16 lbl_1_bss_66,lbl_1_bss_64,lbl_1_bss_3A[21],lbl_1_bss_38;static Vec Center;
static omObjData objects[644];static unkDominationData7 states[644];
static s16 models[644][3],motions[644][6];
static unsigned attrs[512],modelKilled[512],motionKilled[256],freed[644];
static s16 currentMotion[512];
static unsigned effects[5],sounds[3],vibrations,bursts,shifts,apiCalls;static int ended;
static void valid(s16 m){assert(m>=0&&m<512);assert(!modelKilled[m]);apiCalls++;}
void Hu3DModelAttrSet(s16 m,u32 a){valid(m);attrs[m]|=a;}
void Hu3DModelAttrReset(s16 m,u32 a){valid(m);attrs[m]&=~a;}
void Hu3DMotionSet(s16 m,s16 motion){valid(m);assert(motion>=-1&&motion<256);currentMotion[m]=motion;}
void Hu3DMotionShiftSet(s16 m,s16 motion,float t,float duration,u32 a){(void)a;valid(m);assert(motion>=0&&motion<256);assert(t==0&&duration==10);currentMotion[m]=motion;shifts++;}
s32 Hu3DMotionEndCheck(s16 m){valid(m);return ended;}
void Hu3DModelShadowDispOn(s16 m){valid(m);}
void Hu3DModelShadowDispOff(s16 m){valid(m);}
void Hu3DModelPosSet(s16 m,float x,float y,float z){(void)x;(void)y;(void)z;valid(m);}
void omSetTra(omObjData *o,float x,float y,float z){o->trans=(Vec){x,y,z};}
void omSetRot(omObjData *o,float x,float y,float z){o->rot=(Vec){x,y,z};}
void fn_1_4544(s32 type,s32 delay,f32 x,f32 y,f32 z){(void)x;(void)y;(void)z;assert(type>=0&&type<5&&delay==0);effects[type]++;}
void HuAudFXPlay(s32 sound){assert(sound>=0x542&&sound<=0x544);sounds[sound-0x542]++;}
void fn_1_568(s16 player,s16 a,s16 b){assert(player>=0&&player<4&&a==2&&b==4);vibrations++;}
void fn_1_3BB8(s16 count,f32 x,f32 z){(void)x;(void)z;assert(count>0);bursts++;}
void Hu3DModelKill(s16 m){valid(m);assert(!modelKilled[m]);modelKilled[m]++;}
void Hu3DMotionKill(s16 m){assert(m>=0&&m<256);assert(!motionKilled[m]);motionKilled[m]++;}
void HuMemDirectFree(void *p){unkDominationData7 *s=p;assert(s>=states&&s<states+644);freed[s-states]++;assert(freed[s-states]==1);}
`;
const tests=`
static void reset(void){
 memset(objects,0,sizeof(objects));memset(states,0,sizeof(states));memset(attrs,0,sizeof(attrs));
 memset(modelKilled,0,sizeof(modelKilled));memset(motionKilled,0,sizeof(motionKilled));memset(freed,0,sizeof(freed));
 memset(effects,0,sizeof(effects));memset(sounds,0,sizeof(sounds));memset(lbl_1_bss_3A,0,sizeof(lbl_1_bss_3A));
 memset(lbl_1_bss_1E98,0xff,sizeof(lbl_1_bss_1E98));memset(lbl_1_bss_68,0xff,sizeof(lbl_1_bss_68));
 memset(currentMotion,0xff,sizeof(currentMotion));Center=(Vec){0,0,0};
 lbl_1_bss_66=lbl_1_bss_64=lbl_1_bss_38=0;vibrations=bursts=shifts=apiCalls=0;ended=0;
 lbl_1_data_258[0]=fn_1_3360;lbl_1_data_258[1]=fn_1_3364;lbl_1_data_258[2]=fn_1_34A4;lbl_1_data_258[3]=fn_1_37B0;lbl_1_data_258[4]=fn_1_36D0;
 for(int n=0;n<644;n++){
  omObjData *o=&objects[n];unkDominationData7 *s=&states[n];lbl_1_bss_2DB0[n]=o;o->model=models[n];o->motion=motions[n];o->data=s;
  for(int k=0;k<3;k++)models[n][k]=-1;for(int k=0;k<6;k++)motions[n][k]=-1;
  s->unk_00=(u32)n;s->unk_08=(Vec){600-400*(float)(n&3),0,200*(float)(n>>2)};s->unk_14.y=180;
  if(n<80){s->unk_04=1;models[n][0]=(s16)n;motions[n][3]=13;motions[n][4]=14;motions[n][5]=15;
   for(int k=0;k<3;k++)lbl_1_bss_1E98[n][k]=models[n][k];for(int k=0;k<6;k++)lbl_1_bss_68[n][k]=motions[n][k];currentMotion[n]=15;}
  if(n>=640){s->unk_28=1;s->unk_04=1;for(int k=0;k<3;k++)models[n][k]=(s16)(100+(n-640)*3+k);
   for(int k=0;k<6;k++)if(k!=4)motions[n][k]=(s16)(40+(n-640)*6+k);}
 }
}
static void ownership(void){unsigned seen[80]={0};int count=0;
 for(int n=0;n<640;n++){assert((models[n][0]!=-1)==(states[n].unk_04!=0));if(models[n][0]!=-1){int slot=n%80;assert(models[n][0]==slot);assert(++seen[slot]==1);count++;}}
 assert(count==80);
}
int main(int argc,char **argv){
 reset();
 if(argc>1&&strcmp(argv[1],"grow")==0){fn_1_290C(&objects[80],2);fn_1_34A4(&objects[80]);return 0;}
 if(argc>1&&strcmp(argv[1],"fall")==0){lbl_1_bss_3A[0]=21;fn_1_290C(&objects[80],3);fn_1_37B0(&objects[80]);return 0;}
 if(argc>1&&strcmp(argv[1],"handoff")==0){Center.z=4000;fn_1_2930(&objects[80]);assert(motions[80][3]==13&&motions[80][5]==15);return 0;}
 /* Every unresident idle/opening row, including effects and object transforms. */
 for(int n=80;n<640;n++){fn_1_2930(&objects[n]);fn_1_290C(&objects[n],1);fn_1_2930(&objects[n]);assert(states[n].unk_24==0&&states[n].unk_08.y==-300);assert(objects[n].trans.z==states[n].unk_08.z);}
 assert(effects[0]==560&&apiCalls==0);
 /* Resident opening waits; resident growth retains its ten-frame shadow event
    and transitions through the idle blend; special carriers retain state 4. */
 reset();fn_1_290C(&objects[0],1);fn_1_3364(&objects[0]);assert(states[0].unk_24==1&&states[0].unk_2C==1&&currentMotion[0]==14);ended=1;fn_1_3364(&objects[0]);assert(states[0].unk_24==0&&states[0].unk_08.y==-300);
 ended=0;fn_1_290C(&objects[0],2);for(int n=0;n<11;n++)fn_1_34A4(&objects[0]);assert(states[0].unk_24==2&&states[0].unk_2C==11&&currentMotion[0]==13);ended=1;fn_1_34A4(&objects[0]);assert(states[0].unk_24==0&&shifts==1&&currentMotion[0]==15);
 for(int n=640;n<644;n++){fn_1_290C(&objects[n],2);fn_1_34A4(&objects[n]);assert(states[n].unk_24==4);fn_1_36D0(&objects[n]);assert(states[n].unk_24==0);assert(currentMotion[models[n][0]]==motions[n][2]&&currentMotion[models[n][1]]==motions[n][0]);}
 /* Score all four columns to 160, then try another hit. No camera-dependent
    scoring change, no skipped off-screen effects, no model-less animation API. */
 reset();ended=1;for(int row=0;row<161;row++)for(int p=0;p<4;p++){fn_1_26CC((u8)p);if(row<160)fn_1_34A4(&objects[p+row*4]);}
 for(int p=0;p<4;p++)assert(lbl_1_bss_3A[p]==160);assert(effects[0]==640&&sounds[1]==640);assert(shifts==80);
 /* Reuse every pool slot in both camera directions; a previously paused,
    reversed animation must not leak into its next logical owner. */
 for(int direction=0;direction<2;direction++)for(int step=0;step<8;step++){
  int row=direction?7-step:step;Center.z=(float)(row*4000+1900);
  for(int slot=0;slot<80;slot++){int index=row*80+slot,missing=models[index][0]==-1;if(missing)attrs[slot]=HU3D_MOTATTR_PAUSE|HU3D_MOTATTR_REV;fn_1_2930(&objects[index]);assert(motions[index][3]==13&&motions[index][4]==14&&motions[index][5]==15);if(missing){assert(!(attrs[slot]&(HU3D_MOTATTR_PAUSE|HU3D_MOTATTR_REV)));}assert(currentMotion[slot]==15);}
  ownership();
 }
 /* A virtual falling row can acquire a model partway through its chain. */
 reset();states[80].unk_24=3;states[80].unk_2C=1;states[80].unk_14.x=-20;lbl_1_bss_3A[0]=21;Center.z=4000;fn_1_2930(&objects[80]);assert(models[80][0]==0&&motions[80][5]==15);assert(attrs[0]&HU3D_MOTATTR_PAUSE);ownership();
 /* Complete all 640 logical dominoes at maximum score. Rotation, propagation,
    effects, vibration and the four completion counters must all finish. */
 reset();for(int p=0;p<4;p++)lbl_1_bss_3A[p]=160;fn_1_25C0();assert(lbl_1_bss_66==4);
 int ticks;for(ticks=0;ticks<2000&&!fn_1_2880();ticks++)for(int n=0;n<640;n++)if(states[n].unk_24==3)fn_1_37B0(&objects[n]);
 assert(ticks<2000&&fn_1_2880()&&lbl_1_bss_66==0&&lbl_1_bss_64==160);assert(effects[2]==636&&effects[3]==4&&vibrations==640&&sounds[1]==636&&sounds[2]==4);
 for(int n=0;n<640;n++){assert(states[n].unk_24==0);assert(states[n].unk_14.x==(n>=636?-90.0f:-68.0f));}
 /* Unequal scores and zero-score columns preserve the completion counts. */
 reset();lbl_1_bss_3A[0]=0;lbl_1_bss_3A[1]=1;lbl_1_bss_3A[2]=80;lbl_1_bss_3A[3]=160;fn_1_25C0();assert(lbl_1_bss_66==3);
 for(ticks=0;ticks<2000&&!fn_1_2880();ticks++)for(int n=0;n<640;n++)if(states[n].unk_24==3)fn_1_37B0(&objects[n]);assert(ticks<2000&&lbl_1_bss_66==0&&vibrations==241&&bursts==2);
 /* Release one owner of each model and shared animation even after remapping. */
 reset();Center.z=29800;for(int n=0;n<644;n++)fn_1_2930(&objects[n]);ownership();for(int n=0;n<644;n++)fn_1_31F8(&objects[n]);
 unsigned modelCount=0,motionCount=0;for(int n=0;n<512;n++)modelCount+=modelKilled[n];for(int n=0;n<256;n++)motionCount+=motionKilled[n];assert(modelCount==92&&motionCount==23);for(int n=0;n<644;n++)assert(freed[n]==1);
 puts("PASS Domination lifecycle: all 644 objects, growth, camera pool ownership, special carriers, 640-row domino completion, score limits and exact cleanup");return 0;
}
`;
for(const variant of ['baseline','fixed'])fs.writeFileSync(path.join(output,variant+'.c'),(variant==='baseline'?prefix.replace('#define TARGET_PC 1',''):prefix)+callbacks.map(extract).join('\n')+tests,{flag:'wx'});
fs.writeFileSync(path.join(output,'source.json'),JSON.stringify({source:'src/REL/m407Dll/whomp.c',sha256:crypto.createHash('sha256').update(bytes).digest('hex'),callbacks,negativeControls:['grow','fall','handoff'],scope:'Extracted production callbacks with deterministic API traps and logical objects; not actual game or multiplayer execution.'},null,2)+'\n',{flag:'wx'});
console.log('Generated full Domination lifecycle fixture and three baseline controls');

