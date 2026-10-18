#include <nod.h>
#include <aurora/dvd.h>
#include <dolphin/dvd.h>
#include <cassert>
#include <cstdio>
#include <cstring>

static void be32(unsigned char* out, unsigned value) {
    out[0] = value >> 24; out[1] = value >> 16;
    out[2] = value >> 8; out[3] = value;
}
static unsigned callbackCount;
static DVDFileInfo* expectedFile;
static void read_callback(s32 result, DVDFileInfo* file) {
    assert(result == 9 && file == expectedFile);
    ++callbackCount;
}
static void expect_bad_dol(const unsigned char* bytes, size_t size) {
    FILE* file = std::fopen("/bad-dol.iso", "wb");
    assert(file && std::fwrite(bytes, 1, size, file) == size);
    assert(std::fclose(file) == 0);
    NodHandle *disc = nullptr, *partition = nullptr, *payload = nullptr;
    auto result = nod_disc_open("/bad-dol.iso", nullptr, &disc);
    if (result == NOD_RESULT_OK)
        result = nod_disc_open_partition_kind(disc, NOD_PARTITION_KIND_DATA, nullptr, &partition);
    if (result == NOD_RESULT_OK)
        result = nod_partition_open_file(partition, 1, &payload);
    assert(result != NOD_RESULT_OK && payload == nullptr);
    nod_free(payload); nod_free(partition); nod_free(disc);
    std::remove("/bad-dol.iso");
}
static void test_dvd_adapter(const char* path) {
    callbackCount = 0;
    assert(aurora_dvd_open(path));
    DVDInit();
    assert(aurora_dvd_base_entry_count() == 2);
    assert(DVDConvertPathToEntrynum("/payload.bin") == 1);
    DVDFileInfo dvdFile{};
    assert(DVDOpen("/payload.bin", &dvdFile));
    assert(dvdFile.length == 9);
    alignas(32) unsigned char dvdBytes[32] = {};
    assert(DVDReadPrio(&dvdFile, dvdBytes, 32, 0, 2) == 9);
    assert(std::memcmp(dvdBytes, "test-data", 9) == 0);
    expectedFile = &dvdFile;
    std::memset(dvdBytes, 0, sizeof(dvdBytes));
    assert(DVDReadAsyncPrio(&dvdFile, dvdBytes, 32, 0, read_callback, 2));
    assert(callbackCount == 1 && std::memcmp(dvdBytes, "test-data", 9) == 0);
    assert(DVDClose(&dvdFile));
    aurora_dvd_close();
    puts("Browser DVD passed: Aurora mounts FST, resolves file paths, reads exact bytes and delivers the read callback.");
}

int main() {
    NodHandle* disc = nullptr;
    assert(nod_disc_open("/missing.iso", nullptr, &disc) != NOD_RESULT_OK);
    assert(disc == nullptr && nod_error_message() != nullptr);
    unsigned char bytes[32768] = {};
    FILE* file = std::fopen("/invalid.iso", "wb");
    assert(file);
    assert(std::fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
    assert(std::fclose(file) == 0);
    assert(nod_disc_open("/invalid.iso", nullptr, &disc) != NOD_RESULT_OK);
    assert(disc == nullptr && nod_error_message() != nullptr);
    std::remove("/invalid.iso");
    // Minimal generated GameCube image: one DOL section and one FST file.
    std::memcpy(bytes, "TEST01", 6);
    be32(bytes + 0x1c, 0xc2339f3d);
    std::memcpy(bytes + 0x20, "Synthetic browser disc", 22);
    be32(bytes + 0x420, 0x3000);
    be32(bytes + 0x424, 0x4000);
    be32(bytes + 0x428, 40);
    be32(bytes + 0x42c, 40);
    be32(bytes + 0x3000, 0x100); // text section file offset
    be32(bytes + 0x3048, 0x80003100); // text section address
    be32(bytes + 0x3090, 4); // text section length
    be32(bytes + 0x30e0, 0x80003100); // entry point (never executed)
    be32(bytes + 0x3100, 0x4e800020); // blr
    be32(bytes + 0x4000, 0x01000000);
    be32(bytes + 0x4008, 2);
    be32(bytes + 0x400c, 1);
    be32(bytes + 0x4010, 0x5000);
    be32(bytes + 0x4014, 9);
    std::memcpy(bytes + 0x4019, "payload.bin", 12);
    std::memcpy(bytes + 0x5000, "test-data", 9);
    be32(bytes + 0x3000, 0);
    be32(bytes + 0x3090, 0);
    expect_bad_dol(bytes, sizeof(bytes));
    be32(bytes + 0x3000, 0xfffffff0u);
    be32(bytes + 0x3090, 0x100);
    expect_bad_dol(bytes, sizeof(bytes));
    be32(bytes + 0x3000, 0x100);
    be32(bytes + 0x3090, 4);
    puts("Browser malformed DOL passed: undersized and overflowing extents return errors without aborting.");
    file = std::fopen("/synthetic.iso", "wb");
    assert(file && std::fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
    assert(std::fclose(file) == 0);
    assert(nod_disc_open("/synthetic.iso", nullptr, &disc) == NOD_RESULT_OK);
    NodHandle* partition = nullptr;
    assert(nod_disc_open_partition_kind(disc, NOD_PARTITION_KIND_DATA, nullptr, &partition) == NOD_RESULT_OK);
    NodHandle* payload = nullptr;
    assert(nod_partition_open_file(partition, 1, &payload) == NOD_RESULT_OK);
    unsigned char readback[16] = {};
    assert(nod_read(payload, readback, sizeof(readback)) == 9);
    assert(std::memcmp(readback, "test-data", 9) == 0);
    assert(nod_read(payload, readback, 1) == 0);
    assert(nod_seek(payload, 5, SEEK_SET) == 5);
    assert(nod_read(payload, readback, 4) == 4);
    assert(std::memcmp(readback, "data", 4) == 0);
    nod_free(payload); nod_free(partition); nod_free(disc);
    test_dvd_adapter("/synthetic.iso");
    static unsigned char cisoHeader[32768] = {};
    std::memcpy(cisoHeader, "CISO", 4);
    cisoHeader[5] = 0x80; // little-endian 32768-byte block size
    cisoHeader[8] = 1; // first logical block is stored, others are sparse
    file = std::fopen("/synthetic.ciso", "wb");
    assert(file && std::fwrite(cisoHeader, 1, sizeof(cisoHeader), file) == sizeof(cisoHeader));
    assert(std::fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
    assert(std::fclose(file) == 0);
    for (unsigned cycle = 0; cycle < 3; ++cycle) {
        assert(!aurora_dvd_open("/missing.ciso"));
        test_dvd_adapter("/synthetic.ciso");
    }
    std::remove("/synthetic.ciso");
    puts("Browser CISO passed: generated sparse image reads correctly across three failed-open/reopen cycles.");
    std::remove("/synthetic.iso");
    puts("Browser valid disc passed: synthetic GameCube partition/file opens, reads exact bytes, seeks and reaches EOF.");
    puts("Browser disc passed: actual Rust C API rejects missing and invalid images without creating handles.");
}
