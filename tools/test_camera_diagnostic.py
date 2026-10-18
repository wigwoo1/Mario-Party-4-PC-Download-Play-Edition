"""Compile the actual browser camera diagnostic with a deterministic logging fixture.
Run from a Visual Studio developer shell: python tools/test_camera_diagnostic.py
This checks reporting behavior, not whole-game camera fidelity.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/game/hsfman.c').read_text()
start = source.index('static void ReportCameraPositionWrite(')
end = source.index('\n}\n', start) + 3
prefix = '#include <stdio.h>\n#include <stdarg.h>\n#include <string.h>\n#include <assert.h>\ntypedef int s32;\ntypedef struct {float x,y,z;} Vec;\n#define HU3D_CAM_MAX 16\n#define EM_LOG_C_STACK 1\n#define EM_LOG_JS_STACK 2\nstruct {float fov;} Hu3DCamera[16];\nstatic int logs,stacks; static char last[9000];\nstatic int emscripten_get_callstack(int flags,char *buf,int len){++stacks; strcpy(buf,"stack"); return 5;}\nstatic int capture(FILE *stream,const char *fmt,...){va_list args;va_start(args,fmt);vsnprintf(last,sizeof(last),fmt,args);va_end(args);++logs;return 0;}\n#define fprintf capture\n'
suffix = '\nint main(void){\nVec p={0,0,0},t={0,0,0},valid={1,2,3};\nHu3DCamera[3].fov=-1; Hu3DCamera[4].fov=45;\nReportCameraPositionWrite(8,&p,&t); assert(logs==1 && strstr(last,"activeMask=0 reportMask=8"));\nfor(int i=1;i<599;++i) ReportCameraPositionWrite(8,&p,&t);\nassert(logs==1 && stacks==1);\nReportCameraPositionWrite(8,&p,&t); assert(logs==2 && stacks==2);\nReportCameraPositionWrite(16,&p,&t); assert(logs==3 && strstr(last,"activeMask=16 reportMask=16"));\nReportCameraPositionWrite(8,&valid,&t); assert(logs==3);\nReportCameraPositionWrite(8,&p,&t); assert(logs==4);\nReportCameraPositionWrite(16,&p,&t); assert(logs==4);\nassert(p.x==0 && p.y==0 && p.z==0 && t.x==0 && valid.x==1 && valid.y==2 && valid.z==3);\nassert(Hu3DCamera[3].fov==-1 && Hu3DCamera[4].fov==45);\nputs("PASS: first/600th reports, independent slots, valid reset, active context and unchanged camera/vector data");return 0;}\n'
with tempfile.TemporaryDirectory(prefix='partyboard-camera-') as directory:
    base = Path(directory)
    fixture = base / 'camera.c'
    fixture.write_text(prefix + source[start:end] + suffix)
    executable = base / 'camera.exe'
    subprocess.run(['cl', '/nologo', str(fixture), '/Fe:' + str(executable),
                    '/Fo:' + str(base / 'camera.obj')], check=True)
    subprocess.run([str(executable)], check=True)
