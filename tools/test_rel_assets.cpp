// Fault injection only: exercises the real asset loader without disc/game resources.
#include <port/port_version.h>
#include <port/dolassets.h>
extern "C" {
#include <game/memory.h>
}
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>

static bool openOk, allocateOk;
static u32 fileLength;
static s32 readResult;
static int opens, closes, allocations, frees, reads;
static constexpr u32 offset = 0x2B80 + 0xA0, length = 0x307D;
namespace partyboard::version {
GameVersion getGameVersion() { return GameVersion::UsaRev1; }
}
extern "C" {
void OSReport(const char*, ...) {}
const u8* DVDGetDOLLocation(s32* size) { *size=0; return nullptr; }
BOOL DVDOpen(const char* path, DVDFileInfo* file) {
    assert(std::strcmp(path, "dll/bootDll.rel") == 0);
    ++opens; file->length=fileLength; return openOk;
}
BOOL DVDClose(DVDFileInfo*) { ++closes; return TRUE; }
s32 DVDReadPrio(DVDFileInfo*, void* buffer, s32 size, s32 start, s32) {
    ++reads; assert(size==length && start==offset && buffer);
    if(readResult>0) std::memset(buffer, 0x5a, static_cast<size_t>(readResult));
    return readResult;
}
void* HuMemDirectMalloc(HeapID heap, size_t size) {
    assert(heap==HEAP_DVD && size==length); ++allocations;
    return allocateOk ? std::malloc(size) : nullptr;
}
void HuMemDirectFree(void* buffer) { assert(buffer); ++frees; std::free(buffer); }
}
static void reset() {
    openOk=allocateOk=true; fileLength=offset+length; readResult=length;
    opens=closes=allocations=frees=reads=0;
}
int main() {
    reset(); assert(!GetRelIncludeData(-1)); assert(!GetRelIncludeData(REL_INCLUDE_COUNT)); assert(opens==0);
    reset(); openOk=false; assert(!GetRelIncludeData(0)); assert(opens==1 && closes==0 && allocations==0);
    for(u32 size : {offset-1, offset, offset+length-1}) {
        reset(); fileLength=size; assert(!GetRelIncludeData(0));
        assert(closes==1 && allocations==0 && reads==0);
    }
    reset(); allocateOk=false; assert(!GetRelIncludeData(0)); assert(closes==1 && reads==0 && frees==0);
    for(s32 result : {-1, 0, static_cast<s32>(length-1)}) {
        reset(); readResult=result; assert(!GetRelIncludeData(0));
        assert(reads==1 && closes==1 && allocations==1 && frees==1);
    }
    reset(); auto* data=static_cast<unsigned char*>(GetRelIncludeData(0));
    assert(data && closes==1 && reads==1 && frees==0);
    for(u32 i=0;i<length;++i) assert(data[i]==0x5a);
    HuMemDirectFree(data);
    puts("REL assets passed: missing/truncated files, allocation failure, short/error reads and exact success.");
}
