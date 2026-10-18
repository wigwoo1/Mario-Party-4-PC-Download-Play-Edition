#include <aurora/main.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>
#include <cwchar>
#include <cstdio>
#include <port/native_restart_intent.hpp>
#endif

extern "C" int port_main(int argc, char* argv[]);
extern "C" int PartyBoardNativeRestartPending();
extern "C" int PartyBoardNativeRestartOnlineAction();

int main(int argc, char *argv[])
{
    const int result=port_main(argc, argv);
#ifdef _WIN32
    // port_main has shut down OS workers, audio and renderer before returning.
    // A new process supplies fresh game globals; rerunning port_main would
    // retain initialized engine/overlay state and cannot reset a session.
    if(result==0&&PartyBoardNativeRestartPending()){
        try{partyboard::net::publishNativeRestartIntent(PartyBoardNativeRestartOnlineAction());}
        catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 2;}
        std::vector<wchar_t> executable(32768);
        const auto length=GetModuleFileNameW(nullptr,executable.data(),DWORD(executable.size()));
        if(!length||length>=executable.size()){std::fprintf(stderr,"Native restart executable lookup failed\n");return 2;}
        const auto original=GetCommandLineW();
        std::vector<wchar_t> command(original,original+std::wcslen(original)+1);
        STARTUPINFOW startup{};startup.cb=sizeof(startup);
        PROCESS_INFORMATION process{};
        if(!CreateProcessW(executable.data(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&process)){
            std::fprintf(stderr,"Native restart failed: %lu\n",GetLastError());return 2;
        }
        CloseHandle(process.hThread);CloseHandle(process.hProcess);
    }
#endif
    return result;
}

