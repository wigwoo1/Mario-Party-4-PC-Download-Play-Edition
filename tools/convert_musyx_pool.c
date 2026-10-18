/* Private stdin/stdout only; no extracted asset files. */
#include <stdio.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
#include "../src/port/musyx_pool.h"
#include "../src/port/musyx_project.h"
int main(int argc, char **argv) {
    uint8_t header[4], *input, *output;
    size_t bytes;
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    if (fread(header, 1, 4, stdin) != 4) return 2;
    bytes = PartyboardMsmU32(header);
    if (!bytes || bytes > 64 * 1024 * 1024) return 2;
    input = (uint8_t *)malloc(bytes);
    if (!input) return 3;
    if (fread(input, 1, bytes, stdin) != bytes) { free(input); return 4; }
    output = argc == 2 && !strcmp(argv[1], "--project")
        ? PartyboardMusyxProjectConvert(input, bytes) : PartyboardMusyxPoolConvert(input, bytes);
    free(input);
    if (!output) return 5;
    if (fwrite(output, 1, bytes, stdout) != bytes) { free(output); return 6; }
    free(output);
    return fflush(stdout) ? 7 : 0;
}
