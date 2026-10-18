"""Test actual allocator diagnostics using MSVC from a developer shell.
The browser console call is captured; this does not prove game heap fidelity.
"""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = (root / 'src/game/memory.c').read_text()
start = source.index('#if INTPTR_MAX')
end = source.index('\nvoid HuMemMemoryFreeNum',start)
part = source[start:end]
start = part.index('EM_ASM(')
end = part.index('free_bytes, largest_free, blocks);',start)+len('free_bytes, largest_free, blocks);')
part = part[:start]+'capture(heap_ptr, size, alloc_size, num, free_bytes, largest_free, blocks);'+part[end:]
prefix = '#include <stdint.h>\n#include <stddef.h>\n#include <string.h>\n#include <assert.h>\n#include <stdio.h>\ntypedef int32_t s32;typedef uint8_t u8;\n#define TARGET_PC 1\n#define __EMSCRIPTEN__ 1\n#define EMSCRIPTEN_KEEPALIVE\nstatic size_t calls,requested,aligned,freebytes,largest,count;\nstatic void capture(void* heap,size_t size,int alloc,uintptr_t tag,size_t free,size_t big,size_t blocks){++calls;requested=size;aligned=alloc;freebytes=free;largest=big;count=blocks;}\nstatic void OSReport(const char* format,...){ }\nstatic void HuMemHeapDump(void* heap,int status){ }\n'
suffix = '\nint main(void){\n union {long double alignment; unsigned char bytes[4096];} storage;\n struct memory_block *a=(struct memory_block*)storage.bytes,*b=(struct memory_block*)(storage.bytes+512);\n memset(storage.bytes,0,sizeof(storage.bytes));HuMemHeapInit(a,1024);\n a->size=512;a->next=b;a->prev=b;b->size=512;b->flag=0;b->next=a;b->prev=a;\n unsigned char before[4096];memcpy(before,storage.bytes,4096);\n assert(!HuMemMemoryAlloc(a,700,0));assert(calls==1 && requested==700 && freebytes==1024 && largest==512 && count==2);\n assert(memcmp(before,storage.bytes,4096)==0);\n assert(PartyBoardBrowserHeapFailureWord(0)==1 && PartyBoardBrowserHeapFailureWord(2)==700 && PartyBoardBrowserHeapFailureWord(5)==1024 && PartyBoardBrowserHeapFailureWord(6)==512 && PartyBoardBrowserHeapFailureWord(7)==2 && PartyBoardBrowserHeapFailureWord(9)==0);\n a->flag=b->flag=1;memcpy(before,storage.bytes,4096);\n assert(!HuMemMemoryAlloc(a,64,0));assert(calls==2 && freebytes==0 && largest==0 && count==2);\n assert(PartyBoardBrowserHeapFailureWord(2)==700);\n assert(memcmp(before,storage.bytes,4096)==0);\n HuMemHeapInit(a,4096);assert(HuMemMemoryAlloc(a,128,0));assert(calls==2);\n puts("PASS production allocator: fragmented/exhausted capacity, failure leaves heap unchanged, success emits no failure report");return 0;}\n\n'
with tempfile.TemporaryDirectory(prefix='partyboard-heap-') as directory:
    base = Path(directory)
    fixture = base / 'heap.c'
    fixture.write_text(prefix+part+suffix)
    executable = base / 'heap.exe'
    subprocess.run(['cl','/nologo',str(fixture),'/Fe:'+str(executable),'/Fo:'+str(base/'heap.obj')],check=True)
    subprocess.run([str(executable)],check=True)
