#include <aurora/dvd.h>
#include <dolphin/dvd.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <emscripten/heap.h>
#include <port/port_version.h>
#include <port/dolassets.h>

int main(int argc, char** argv) {
    assert(GetDolIncludeData(DOL_INCLUDE_ANK8X8_4B) == nullptr);
    const char* path = argc > 1 ? argv[1] : "/verified-disc.ciso";
    // Exercise libc buffered seeks and EOF over the mounted adapter before DVD access.
    FILE* input = std::fopen(path, "rb");
    assert(input);
    assert(std::fseek(input, 0, SEEK_END) == 0);
    const long size = std::ftell(input);
    assert(size >= 8);
    unsigned char tail[8] = {};
    assert(std::fread(tail, 1, 8, input) == 0 && std::feof(input));
    assert(std::fseek(input, -4, SEEK_END) == 0 && !std::feof(input));
    assert(std::fread(tail, 1, 8, input) == 4 && std::feof(input));
    assert(!std::ferror(input));
    assert(std::fseek(input, 0, SEEK_SET) == 0);
    assert(std::fread(tail, 1, 4, input) == 4 && std::memcmp(tail, "CISO", 4) == 0);
    assert(std::fclose(input) == 0);
    puts("Browser disc I/O passed: end seek, short read, EOF and rewind.");
    assert(aurora_dvd_open(path));
    DVDInit();
    if (argc > 2 && std::strcmp(argv[2], "--inspect-game") == 0) {
        const auto* id = DVDGetCurrentDiskID();
        assert(id && std::memcmp(id->gameName, "GMPE", 4) == 0);
        assert(std::memcmp(id->company, "01", 2) == 0 && id->gameVersion == 1);
        partyboard::version::init();
        assert(partyboard::version::getGameVersion() == partyboard::version::GameVersion::UsaRev1);
        assert(partyboard::version::isRegionUsa() && !partyboard::version::isRegionPal());
        s32 dolSize = 0;
        const auto* dol = DVDGetDOLLocation(&dolSize);
        // USA revision 1 embedded font locations, compared against the mounted DOL.
        constexpr size_t fontOffset = 0x8011FE00u - 0x80003000u;
        constexpr size_t asciiOffset = 0x8012DCD7u - 0x80003000u;
        assert(dol && dolSize > 0 && static_cast<size_t>(dolSize) >= asciiOffset + 0x800);
        InitializeDol();
        assert(GetDolIncludeData(-1) == nullptr);
        assert(GetDolIncludeData(DOL_INCLUDE_COUNT) == nullptr);
        const auto* font = GetDolIncludeData(DOL_INCLUDE_ANK8X8_4B);
        const auto* ascii = GetDolIncludeData(DOL_INCLUDE_ASCII8X8_1BPP);
        assert(font && ascii);
        assert(std::memcmp(font, dol + fontOffset, 0x2000) == 0);
        assert(std::memcmp(ascii, dol + asciiOffset, 0x800) == 0);
        InitializeDol();
        assert(std::memcmp(GetDolIncludeData(DOL_INCLUDE_ANK8X8_4B), dol + fontOffset, 0x2000) == 0);
        puts("Browser bootstrap assets passed: actual version initialization and both embedded fonts match mounted DOL.");
        const s32 entries = aurora_dvd_base_entry_count();
        assert(entries > 100);
        bool readFile = false;
        for (s32 entry = 1; entry < entries; ++entry) {
            DVDFileInfo candidate{};
            if (!DVDFastOpen(entry, &candidate)) continue;
            if (candidate.length >= 32) {
                alignas(32) unsigned char sample[32];
                assert(DVDReadPrio(&candidate, sample, 32, 0, 2) == 32);
                readFile = true;
            }
            assert(DVDClose(&candidate));
            if (readFile) break;
        }
        assert(readFile);
        std::printf("Browser local game disc passed: GMPE01 revision 1, %d FST entries, bounded file read; heap %zu bytes.\n", entries, emscripten_get_heap_size());
        aurora_dvd_close();
        InitializeDol();
        assert(GetDolIncludeData(DOL_INCLUDE_ANK8X8_4B) == nullptr);
        puts("Browser DOL lifecycle passed: reinitialization preserves bytes; closed disc clears assets.");
        return 0;
    }
    DVDFileInfo file{};
    partyboard::version::init();
    InitializeDol();
    // Generated DOL is too short to contain either game's embedded font range.
    assert(GetDolIncludeData(DOL_INCLUDE_ANK8X8_4B) == nullptr);
    assert(GetDolIncludeData(DOL_INCLUDE_ASCII8X8_1BPP) == nullptr);
    assert(DVDOpen("/payload.bin", &file));
    assert(file.length == 9);
    alignas(32) unsigned char bytes[32] = {};
    assert(DVDReadPrio(&file, bytes, 32, 0, 2) == 9);
    assert(std::memcmp(bytes, "test-data", 9) == 0);
    assert(DVDClose(&file));
    aurora_dvd_close();
    InitializeDol();
    assert(GetDolIncludeData(DOL_INCLUDE_ANK8X8_4B) == nullptr);
    puts("Browser DOL bounds passed: truncated embedded font ranges rejected.");
    std::printf("Browser heap bytes: %zu\n", emscripten_get_heap_size());
    if (argc == 1) std::remove(path);
    puts("Browser asset mount passed: verified IndexedDB CISO mounted and read through Aurora DVD.");
    return 0;
}
