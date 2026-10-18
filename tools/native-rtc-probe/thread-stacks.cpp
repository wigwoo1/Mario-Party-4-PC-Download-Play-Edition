// Read-only startup diagnosis for one explicitly identified owned game process.
// No dump/memory contents are saved. Each thread is resumed before continuing.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <dbghelp.h>
#include <cstdio>
#include <cstdlib>
#include <cwchar>

int wmain(int argc, wchar_t** argv) {
    if (argc != 4) return 2; // PID, exact executable path, local symbol directory
    const DWORD pid = wcstoul(argv[1], nullptr, 10);
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!process) return 3;
    wchar_t image[32768]; DWORD length = 32768;
    if (!QueryFullProcessImageNameW(process, 0, image, &length) || _wcsicmp(image, argv[2]) != 0) {
        CloseHandle(process); return 4;
    }
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_NO_PROMPTS);
    if (!SymInitializeW(process, argv[3], TRUE)) { CloseHandle(process); return 5; }
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 entry{}; entry.dwSize = sizeof(entry);
    if (snapshot == INVALID_HANDLE_VALUE) { SymCleanup(process); CloseHandle(process); return 6; }
    unsigned inspected = 0;
    if (Thread32First(snapshot, &entry)) do {
        if (entry.th32OwnerProcessID != pid) continue;
        HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
        if (!thread) continue;
        if (SuspendThread(thread) == DWORD(-1)) { CloseHandle(thread); continue; }
        CONTEXT context{}; context.ContextFlags = CONTEXT_FULL;
        if (GetThreadContext(thread, &context)) {
            ++inspected; printf("THREAD %lu\n", entry.th32ThreadID);
            STACKFRAME64 frame{};
            frame.AddrPC = {context.Rip, 0, AddrModeFlat};
            frame.AddrFrame = {context.Rbp, 0, AddrModeFlat};
            frame.AddrStack = {context.Rsp, 0, AddrModeFlat};
            for (unsigned depth = 0; depth < 40; ++depth) {
                if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context, nullptr,
                                 SymFunctionTableAccess64, SymGetModuleBase64, nullptr) || !frame.AddrPC.Offset) break;
                alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME]{};
                auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
                symbol->SizeOfStruct = sizeof(SYMBOL_INFO); symbol->MaxNameLen = MAX_SYM_NAME;
                DWORD64 displacement = 0;
                if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol))
                    printf("  %02u %s + 0x%llx\n", depth, symbol->Name, displacement);
                else printf("  %02u 0x%llx\n", depth, frame.AddrPC.Offset);
            }
        }
        ResumeThread(thread);
        CloseHandle(thread);
    } while (Thread32Next(snapshot, &entry));
    CloseHandle(snapshot); SymCleanup(process); CloseHandle(process);
    printf("INSPECTED %u OWNED THREADS\n", inspected);
    return inspected ? 0 : 7;
}
