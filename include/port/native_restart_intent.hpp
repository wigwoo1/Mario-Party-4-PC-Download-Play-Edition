#pragma once
#include <optional>
#include <stdexcept>
#include <string_view>
namespace partyboard::net {
inline constexpr char NativeSessionEndedNotice[] =
    "The online game ended because players could not reconnect or synchronize. You can host or join a new game.";
}
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
namespace partyboard::net {
// Fixed, single-use presentation state only. No remote text, credentials,
// session data or private paths are carried into the fresh menu process.
inline void publishNativeSessionEndedNotice(bool ended){
    if(!SetEnvironmentVariableW(L"PARTYBOARD_NATIVE_SESSION_ENDED",ended?L"1":nullptr))
        throw std::runtime_error("Native session-ended notice publication failed");
}
inline bool consumeNativeSessionEndedNotice(){
    wchar_t value[4]{};SetLastError(ERROR_SUCCESS);
    const DWORD length=GetEnvironmentVariableW(L"PARTYBOARD_NATIVE_SESSION_ENDED",value,4);
    if(!length&&GetLastError()==ERROR_ENVVAR_NOT_FOUND)return false;
    const bool valid=length==1&&value[0]==L'1';
    if(!SetEnvironmentVariableW(L"PARTYBOARD_NATIVE_SESSION_ENDED",nullptr))
        throw std::runtime_error("Native session-ended notice consumption failed");
    if(!valid)throw std::invalid_argument("Malformed native session-ended notice");
    return true;
}
// Single-use action only. No credentials, private asset path or network handles
// enter the command line. The fresh process consumes this before game setup.
inline void publishNativeRestartIntent(int action){
    if(action!=-1&&action!=0&&action!=1)throw std::invalid_argument("Invalid native restart action");
    const wchar_t* value=action==0?L"host":action==1?L"join":nullptr;
    if(!SetEnvironmentVariableW(L"PARTYBOARD_NATIVE_ONLINE_INTENT",value))
        throw std::runtime_error("Native restart intent publication failed");
}
inline std::optional<int> consumeNativeRestartIntent(){
    wchar_t value[8]{};
    SetLastError(ERROR_SUCCESS);
    const DWORD length=GetEnvironmentVariableW(L"PARTYBOARD_NATIVE_ONLINE_INTENT",value,8);
    if(!length&&GetLastError()==ERROR_ENVVAR_NOT_FOUND)return std::nullopt;
    const bool host=length==4&&std::wstring_view(value,length)==L"host";
    const bool join=length==4&&std::wstring_view(value,length)==L"join";
    if(!SetEnvironmentVariableW(L"PARTYBOARD_NATIVE_ONLINE_INTENT",nullptr))
        throw std::runtime_error("Native restart intent consumption failed");
    if(!host&&!join)throw std::invalid_argument("Malformed native restart intent");
    return host?0:1;
}
}
#endif
