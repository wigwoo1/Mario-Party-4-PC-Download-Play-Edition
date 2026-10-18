import json, os, subprocess, hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[3]
source=root/'outputs/partyboard/src/game/board/main.c'
text=source.read_text().replace('\r\n','\n')
header=(root/'outputs/partyboard/include/game/board/main.h').read_text()
def function(signature):
    start=text.index(signature+'\n{'); opening=text.index('{',start); depth=1; end=opening+1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}'); end+=1
    return text[start:end]
structs=header[header.index('typedef struct board_focus_data'):header.index('extern BoardCameraData')]
views=text[text.index('typedef struct camera_view'):text.index('omObjData *boardMainObj;')]
table=text[text.index('static CameraView camViewTbl[]'):text.index('extern void BoardMGSetupPlayClear')]
macros=text[text.index('#define CAM_LERP('):text.index('static void CalcCameraPos(BoardCameraData *camera)\n{')]
preamble=r"""#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
typedef uint8_t u8;typedef uint16_t u16;typedef int16_t s16;typedef int32_t s32;typedef uint32_t u32;
typedef struct {float x,y,z;} Vec;typedef void omObjData;
#define FALSE 0
#define HU_DISP_ASPECT 1.2f
#define HU_FB_WIDTH 640
#define HU_FB_HEIGHT 480
#define FLAG_ID_MAKE(a,b) 0
static void _ClearFlag(int flag){}
static void OSs16tof32(const s16 *input,float *output){*output=*input;}
"""
stubs=r"""
static int cameraUseBackup;static BoardCameraData cameraBackup,boardCamera;static void *cameraObj,*boardObjMan;
static void BoardSpaceFlagPosGet(int layer,u32 flag,Vec *target){*target=(Vec){10,20,30};}
static void Hu3DCameraCreate(int mask){}
static void Hu3DCameraScissorSet(int a,int b,int c,int d,int e){}
static void Hu3DCameraPerspectiveSet(int a,int b,int c,int d,float e){}
static void UpdateCamera(omObjData *object){}
static void *omAddObjEx(void *a,int b,int c,int d,int e,void(*f)(omObjData*)){return 0;}
"""
checks=r"""
int main(){
 BoardCameraInit();assert(boardCamera.zoom==3200 && boardCamera.fov==25);
 CalcCameraPos(&boardCamera);
#ifdef TARGET_PC
 assert(boardCamera.zoom==3200 && boardCamera.fov==25 && boardCamera.target.x==10);
#else
 assert(boardCamera.zoom==0 && boardCamera.fov==0 && boardCamera.target.x==0);
#endif
 boardCamera.focus=(BoardFocusData){.view_type=1,.time=0,.max_time=2,.fov_start=25,.fov_end=25,.zoom_start=3200,.zoom_end=2100,.target_start={10,20,30},.target_end={40,50,60}};
 CalcCameraPos(&boardCamera);assert(boardCamera.zoom==3200);
 CalcCameraPos(&boardCamera);assert(boardCamera.zoom==2650 && boardCamera.target.x==25);
 CalcCameraPos(&boardCamera);assert(boardCamera.zoom==2100 && boardCamera.target.x==40);
 for(int i=0;i<120;i++)CalcCameraPos(&boardCamera);
 assert(boardCamera.zoom==2100 && boardCamera.target.x==40);
 puts("PASS board camera initialization and configured interpolation");return 0;
}
"""
folder=root/'work/board-camera-init-fixture';folder.mkdir(exist_ok=True)
fixture=folder/'fixture.c';fixture.write_text(preamble+structs+views+table+stubs+function('void BoardCameraInit(void)')+'\n'+macros+function('static void CalcCameraPos(BoardCameraData *camera)')+'\n'+checks)
env=os.environ.copy();env['EM_CONFIG']=str(root/'work/emsdk/.emscripten')
python=root/'work/emsdk/python/3.13.3_64bit/python.exe';env['EMSDK_PYTHON']=str(python)
node=Path('C:/Users/wigwo/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node.exe')
for mode in ['original','fixed']:
 output=folder/(mode+'.js');command=[str(python),str(root/'work/emsdk/upstream/emscripten/emcc.py'),str(fixture),'-O0','-sENVIRONMENT=node','-o',str(output)]
 if mode=='fixed':command+=['-DTARGET_PC']
 subprocess.run(command,env=env,check=True);subprocess.run([str(node),str(output)],check=True)
report={'scope':'Compiled actual BoardCameraInit/CalcCameraPos; graphics/object/space services stubbed','originalZeroEndpointOverwriteReproduced':True,'fixedDefaultsRetained':True,'configuredMotionStillInterpolates':True,'sourceSha256':hashlib.sha256(source.read_bytes()).hexdigest(),'actualFullMatchVerified':False}
(root/'outputs/validation/board-camera-init-regression-20261005.json').write_text(json.dumps(report,indent=2)+'\n')
