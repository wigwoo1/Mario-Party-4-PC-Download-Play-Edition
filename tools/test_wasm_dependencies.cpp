#include <cstdlib>
#include <fmt/format.h>
#include <absl/container/flat_hash_map.h>
#include <sqlite3.h>
#include <xxhash.h>
#include <cassert>
#include <cstdio>
#include <png.h>
#include <vector>
#include <cstring>
int main() {
    absl::flat_hash_map<int,int> values;
    for(int i=0;i<1000;++i) values[i]=i*3;
    for(int i=0;i<1000;++i) assert(values.at(i)==i*3);
    assert(fmt::format("slot {}",3)=="slot 3");
    assert(XXH64("abc",3,0)==0x44bc2cf5ad770999ULL);
    sqlite3 *db=nullptr;
    assert(sqlite3_open(":memory:",&db)==SQLITE_OK);
    assert(sqlite3_exec(db,"CREATE TABLE cache(k INTEGER PRIMARY KEY,v INTEGER); INSERT INTO cache VALUES(7,42)",nullptr,nullptr,nullptr)==SQLITE_OK);
    sqlite3_stmt *stmt=nullptr;
    assert(sqlite3_prepare_v2(db,"SELECT v FROM cache WHERE k=7",-1,&stmt,nullptr)==SQLITE_OK);
    assert(sqlite3_step(stmt)==SQLITE_ROW && sqlite3_column_int(stmt,0)==42);
    assert(sqlite3_finalize(stmt)==SQLITE_OK && sqlite3_close(db)==SQLITE_OK);
    png_image image{}; image.version=PNG_IMAGE_VERSION;
    image.width=2; image.height=1; image.format=PNG_FORMAT_RGBA;
    const unsigned char pixels[]={255,0,0,255,0,255,0,128};
    png_alloc_size_t size=0;
    assert(png_image_write_to_memory(&image,nullptr,&size,0,pixels,0,nullptr));
    std::vector<unsigned char> encoded(size);
    assert(png_image_write_to_memory(&image,encoded.data(),&size,0,pixels,0,nullptr));
    png_image decoded{}; decoded.version=PNG_IMAGE_VERSION;
    assert(png_image_begin_read_from_memory(&decoded,encoded.data(),size));
    assert(decoded.width==2 && decoded.height==1);
    decoded.format=PNG_FORMAT_RGBA;
    unsigned char output[8];
    assert(png_image_finish_read(&decoded,nullptr,output,0,nullptr));
    assert(std::memcmp(output,pixels,8)==0);
    png_image_free(&decoded); png_image_free(&image);
    puts("Browser PNG passed: exact RGBA memory encode/decode round trip.");
    puts("Browser dependencies passed: fmt, Abseil map, xxHash golden value and SQLite query.");
}
