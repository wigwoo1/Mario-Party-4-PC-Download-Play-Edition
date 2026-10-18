from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];work=root.parents[1]/'work';d=work/'reflection-decoder-test';d.mkdir(exist_ok=True)
s=(root/'src/game/sprman.c').read_text();b=(root/'src/port/byteswap.cpp').read_text();h=(root/'include/port/byteswap.h').read_text()
types=(root/'include/game/animdata.h').read_text().replace('#include "dolphin.h"','')
wire=h[h.index('typedef struct AnimData32b'):h.index('typedef struct HsfCluster32b')]
common='#include <stdint.h>\n#include <stdlib.h>\n#include <string.h>\n#include <assert.h>\ntypedef int8_t s8;typedef int16_t s16;typedef int32_t s32;typedef uint8_t u8;typedef uint16_t u16;typedef uint32_t u32;\n#define BYTESWAPPING\n#define HEAP_DATA 0\n'+types+wire
prototypes='''void byteswap_animdata(void*,ANIMDATA*);
void byteswap_animbankdata(AnimBankData32b*,ANIMBANK*);
void byteswap_animpatdata(AnimPatData32b*,ANIMPAT*);
void byteswap_animbmpdata(AnimBmpData32b*,ANIMBMP*);
void byteswap_animframedata(ANIMFRAME*);
void byteswap_animlayerdata(ANIMLAYER*);
'''
(d/'types.h').write_text(common)
# Keep exactly the reader and lock function, stopping before next sprite API.
start=s.index('ANIMDATA *HuSprAnimRead');end=s.index('\n}',s.index('void HuSprAnimLock',start))+2
reader=s[start:end]
start=s.index('void HuSprAnimKill');end=s.index('\n}',start)+2
kill=s[start:end]
manager=(root/'src/game/hsfman.c').read_text()
start=manager.index('    // Decode the requested map');end=manager.index('\n#endif',start)
setter=manager[start:end]
start=manager.index('    if (reflectAnim[0] != defaultReflectAnim)',manager.index('void Hu3DAllKill'));end=manager.index('\n#endif',start)
reset=manager[start:end]
reflection='ANIMDATA *reflectAnim[5], *defaultReflectAnim;int reflectMapNo;\nvoid set(ANIMDATA*arg0){\n'+setter+'\nreflectMapNo=0;}\nvoid reset(){\n'+reset+'\n}\n'
(d/'reader.c').write_text('#include "types.h"\n'+prototypes+'static int live;void* HuMemDirectMalloc(int heap,size_t size){++live;return malloc(size);}void HuMemDirectFree(void*p){assert(p);--live;free(p);}\n'+reader+'\n'+kill+'''
#include <stdio.h>
void be16(u8*p,unsigned v){p[0]=v>>8;p[1]=v;}
void be32(u8*p,unsigned v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
int main(){u8 raw[128]={0};be16(raw,1);be16(raw+2,1);be16(raw+4,1);be32(raw+8,20);be32(raw+12,28);be32(raw+16,44);
be16(raw+20,1);be32(raw+24,64);be16(raw+28,1);be16(raw+34,2);be16(raw+36,2);be32(raw+40,76);
raw[44]=8;raw[45]=7;be16(raw+48,2);be16(raw+50,2);be32(raw+52,4);be32(raw+60,108);be16(raw+66,5);raw[76]=255;be16(raw+84,2);be16(raw+86,2);raw[108]=17;raw[109]=34;
ANIMDATA*a=HuSprAnimRead(raw);assert(live==4&&a->bankNum==1&&a->useNum==0&&a->valid==ANIM_DATA_ALLOCATION_VALID);assert(a->bank->frame==(ANIMFRAME*)(raw+64)&&a->bank->frame->time==5);assert(a->pat->layer==(ANIMLAYER*)(raw+76)&&a->pat->layer->sizeX==2);assert(a->bmp->sizeX==2&&a->bmp->data==raw+108&&((u8*)a->bmp->data)[1]==34);
HuSprAnimLock(a);assert(HuSprAnimRead(a)==a&&a->useNum==2);HuSprAnimKill(a);assert(live==4&&a->useNum==1);HuSprAnimKill(a);assert(live==0&&raw[108]==17);puts("PASS actual animation reader/converters/free; generated big-endian map, not rendered game asset");}
''')
fixture=d/'reader.c'
content=fixture.read_text().replace('#include <stdio.h>',reflection+'\n#include <stdio.h>')
content=content.replace('assert(live==0&&raw[108]==17);puts(','''assert(live==0&&raw[108]==17);
// A fresh raw buffer is required: actual conversion swaps source records in place.
u8 fresh[128];memcpy(fresh,raw,sizeof(raw));
// Restore big-endian records mutated by the prior decoder before reusing fixture bytes.
be16(fresh,1);be16(fresh+2,1);be16(fresh+4,1);be32(fresh+8,20);be32(fresh+12,28);be32(fresh+16,44);
be16(fresh+20,1);be32(fresh+24,64);be16(fresh+28,1);be16(fresh+34,2);be16(fresh+36,2);be32(fresh+40,76);
be16(fresh+48,2);be16(fresh+50,2);be32(fresh+52,4);be32(fresh+60,108);be16(fresh+66,5);be16(fresh+84,2);be16(fresh+86,2);
ANIMDATA base={0};base.valid=ANIM_DATA_ALLOCATION_VALID;base.useNum=1;defaultReflectAnim=&base;reflectAnim[0]=&base;
set((ANIMDATA*)fresh);assert(live==4&&reflectAnim[0]!=&base&&reflectAnim[0]->useNum==1);ANIMDATA*custom=reflectAnim[0];
set(custom);assert(live==4&&custom->useNum==1);set(&base);assert(live==0&&base.useNum==1&&reflectAnim[0]==&base);
set(&base);reset();reset();assert(live==0&&base.useNum==1&&reflectMapNo==0);
puts(''')
fixture.write_text(content)
helpers=b[b.index('template <typename T> [[nodiscard]] constexpr T bswap16'):b.index('template <typename T> [[nodiscard]] constexpr T bswap64')]
scalars=b[b.index('template <typename B> void bswap(B &base, u32'):b.index('template <typename B> void bswap(B &base, Vec')]
flat=b[b.index('template <typename B, typename T> static void bswap_flat'):b.index('template <typename B> void bswap(B &base, f32')]
converters=b[b.index('template <typename B> void bswap(B &base, AnimData32b'):b.index('template <typename B> void bswap(B &base, HSFSECTION')]
wrappers=b[b.index('void byteswap_animdata'):b.index('void byteswap_hsfheader')]
(d/'convert.cpp').write_text('#include "types.h"\n'+helpers+scalars+flat+converters+'extern "C" {\n'+wrappers+'}\n')
em=work/'emsdk/upstream/emscripten'
subprocess.run([sys.executable,str(em/'emcc.py'),str(d/'reader.c'),'-c','-o',str(d/'reader.o')],check=True)
subprocess.run([sys.executable,str(em/'em++.py'),str(d/'convert.cpp'),str(d/'reader.o'),'-std=c++20','-sENVIRONMENT=node','-o',str(d/'test.js')],check=True)
subprocess.run(['node',str(d/'test.js')],check=True)

