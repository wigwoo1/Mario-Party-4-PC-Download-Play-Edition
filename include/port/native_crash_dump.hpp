#pragma once
#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>

namespace partyboard::diagnostics {
using DumpWriter = BOOL (WINAPI *)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
    PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION,
    PMINIDUMP_CALLBACK_INFORMATION);
inline wchar_t crashDumpPath[32768]{};
inline DumpWriter dumpWriter;
inline LPTOP_LEVEL_EXCEPTION_FILTER previousCrashFilter;
inline PVOID firstChanceHandler;
inline LONG WINAPI captureFirstChance(EXCEPTION_POINTERS* exception);
inline bool fullMemoryDump{};
inline void writeDumpSnapshot(EXCEPTION_POINTERS* exception) {
    // A unique requested file preserves evidence from earlier test runs.
    const HANDLE file = CreateFileW(crashDumpPath, GENERIC_WRITE, 0, nullptr,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION information{GetCurrentThreadId(), exception, FALSE};
        dumpWriter(GetCurrentProcess(), GetCurrentProcessId(), file,
            static_cast<MINIDUMP_TYPE>(MiniDumpWithIndirectlyReferencedMemory |
                (fullMemoryDump ? MiniDumpWithFullMemory : 0)), &information, nullptr, nullptr);
        CloseHandle(file);
    }
}
inline LONG WINAPI writeCrashDump(EXCEPTION_POINTERS* exception) {
    writeDumpSnapshot(exception);
    return previousCrashFilter ? previousCrashFilter(exception) : EXCEPTION_EXECUTE_HANDLER;
}
inline LONG WINAPI captureFirstChance(EXCEPTION_POINTERS* exception) {
    if (exception && exception->ExceptionRecord &&
        exception->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        // Diagnostic snapshots may include handled faults. Never consume the
        // exception or treat this file alone as proof of a terminal crash.
        writeDumpSnapshot(exception);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
inline bool installNativeCrashDump() {
    if (dumpWriter) return true;
    const DWORD length = GetEnvironmentVariableW(L"PARTYBOARD_NATIVE_CRASH_DUMP",
        crashDumpPath, DWORD(sizeof(crashDumpPath) / sizeof(*crashDumpPath)));
    if (!length || length >= sizeof(crashDumpPath) / sizeof(*crashDumpPath)) return false;
    // Resolve the system library before an exception, without allocations in
    // the handler or a search through the game's runtime directory.
    const HMODULE library = LoadLibraryExW(L"dbghelp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!library) return false;
    dumpWriter = reinterpret_cast<DumpWriter>(GetProcAddress(library, "MiniDumpWriteDump"));
    if (!dumpWriter) { FreeLibrary(library); return false; }
    wchar_t fullMemory[2]{};
    fullMemoryDump = GetEnvironmentVariableW(L"PARTYBOARD_NATIVE_FULL_MEMORY_DUMP", fullMemory, 2) == 1 && fullMemory[0] == L'1';
    previousCrashFilter = SetUnhandledExceptionFilter(writeCrashDump);
    wchar_t firstChance[2]{};
    if (GetEnvironmentVariableW(L"PARTYBOARD_NATIVE_FIRST_CHANCE_DUMP", firstChance, 2) == 1 && firstChance[0] == L'1') {
        firstChanceHandler = AddVectoredExceptionHandler(1, captureFirstChance);
        if (!firstChanceHandler) return false;
    }
    return true;
}
}
#endif

namespace partyboard {
inline bool installNativeCrashCapture() {
#ifdef _WIN32
    // Missing configuration is the ordinary production path, not an error.
    if (GetEnvironmentVariableW(L"PARTYBOARD_NATIVE_CRASH_DUMP", nullptr, 0) == 0) return true;
    return diagnostics::installNativeCrashDump();
#else
    return true;
#endif
}
}
