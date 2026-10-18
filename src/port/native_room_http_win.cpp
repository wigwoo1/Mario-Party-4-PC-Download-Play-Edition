#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <winhttp.h>
#include <port/native_room_http.hpp>
#include <array>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <memory>
#include <stdexcept>

namespace partyboard::net {
namespace {
[[noreturn]]void failed(){throw std::runtime_error("Lobby HTTP operation failed (Windows error "+std::to_string(GetLastError())+").");}
std::wstring wide(const std::string& text){
    if(text.empty())return {};
    const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),nullptr,0);if(!count)failed();
    std::wstring result(count,L'\0');if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),result.data(),count))failed();return result;
}
struct Handle {
    HINTERNET value=nullptr;
    explicit Handle(HINTERNET value):value(value){if(!value)failed();}
    ~Handle(){if(value)WinHttpCloseHandle(value);}
    Handle(const Handle&)=delete;
};
struct Completion;
struct RequestContext {std::shared_ptr<Completion> completion;};
struct Completion {
    std::mutex mutex;std::condition_variable_any cv;DWORD flags=0,error=0,bytes=0;bool closed=false;
    static void CALLBACK callback(HINTERNET,DWORD_PTR context,DWORD status,void* information,DWORD length)noexcept{
        if(!context)return;auto* binding=reinterpret_cast<RequestContext*>(context);
        const auto lifetime=binding->completion;auto& self=*lifetime;
        if(status==WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING)delete binding;
        {std::lock_guard lock(self.mutex);
         if(status==WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING)self.closed=true;
         else if(status==WINHTTP_CALLBACK_STATUS_REQUEST_ERROR)self.error=information?static_cast<WINHTTP_ASYNC_RESULT*>(information)->dwError:ERROR_WINHTTP_INTERNAL_ERROR;
         else {self.flags|=status;if(status==WINHTTP_CALLBACK_STATUS_READ_COMPLETE)self.bytes=length;}
        }self.cv.notify_all();
    }
    void wait(DWORD flag,std::stop_token stop,std::chrono::steady_clock::time_point deadline){
        std::unique_lock lock(mutex);
        const bool complete=cv.wait_until(lock,stop,deadline,[&]{return error||(flags&flag);});
        if(stop.stop_requested())throw std::runtime_error("Lobby request cancelled.");
        if(!complete)throw std::runtime_error("Lobby request timed out.");
        if(error){SetLastError(error);failed();}flags&=~flag;
    }
};
struct RequestHandle {
    HINTERNET value;std::shared_ptr<Completion> completion;std::unique_ptr<RequestContext> context;bool callbackInstalled=false;
    RequestHandle(HINTERNET value,std::shared_ptr<Completion> completion,std::unique_ptr<RequestContext> context):value(value),completion(std::move(completion)),context(std::move(context)){if(!value)failed();}
    ~RequestHandle(){
        if(!value)return;WinHttpCloseHandle(value);
        // Context/buffers must outlive the final callback, including cancellation.
        if(callbackInstalled){std::unique_lock lock(completion->mutex);completion->cv.wait(lock,[&]{return completion->closed;});}
    }
};
}
NativeRoomHttpResponse nativeRoomHttp(const NativeRoomHttpRequest& input,std::stop_token stop){
    if(stop.stop_requested())throw std::runtime_error("Lobby request cancelled.");
    if(input.url.size()>4096||input.body.size()>110000||!input.maximumBytes||input.maximumBytes>NativeRoomHttpResponseLimit||input.timeoutMs<100||input.timeoutMs>15000||
       (input.method!="GET"&&input.method!="POST"&&input.method!="DELETE"))throw std::invalid_argument("Bounded lobby HTTP request required.");
    if(!input.bearer.empty()&&(input.bearer.size()!=43||input.bearer.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos))throw std::invalid_argument("Invalid lobby access credential.");
    if(input.url.find_first_of("\r\n\t ")!=std::string::npos||input.url.find('\0')!=std::string::npos)throw std::invalid_argument("Canonical lobby URL required.");
    const auto url=wide(input.url);URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=parts.dwUserNameLength=parts.dwPasswordLength=DWORD(-1);
    if(!WinHttpCrackUrl(url.c_str(),DWORD(url.size()),0,&parts))failed();
    const std::wstring host(parts.lpszHostName,parts.dwHostNameLength),path(parts.lpszUrlPath,parts.dwUrlPathLength),extra(parts.lpszExtraInfo,parts.dwExtraInfoLength);
    if(parts.dwUserNameLength||parts.dwPasswordLength||extra.find(L'#')!=std::wstring::npos||
       (parts.nScheme!=INTERNET_SCHEME_HTTPS&&(parts.nScheme!=INTERNET_SCHEME_HTTP||(host!=L"127.0.0.1"&&host!=L"localhost"&&host!=L"::1")))||
       !path.starts_with(L"/api/lobbies"))throw std::invalid_argument("Trusted HTTPS lobby endpoint or local test service required.");
    Handle session(WinHttpOpen(L"PartyBoard-Lobby/1",WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,WINHTTP_FLAG_ASYNC));
    const int timeout=int(input.timeoutMs);if(!WinHttpSetTimeouts(session.value,timeout,timeout,timeout,timeout))failed();
    Handle connection(WinHttpConnect(session.value,host.c_str(),parts.nPort,0));auto completion=std::make_shared<Completion>();
    auto binding=std::make_unique<RequestContext>();binding->completion=completion;
    const auto method=wide(input.method),target=path+extra;
    // These buffers precede the request guard so even exception unwinding
    // cancels/drains WinHTTP before their storage can be released.
    std::array<char,8192> buffer{};
    const std::wstring headers=L"Accept: application/json\r\n"+(input.body.empty()?std::wstring{}:L"Content-Type: application/json\r\n")+(input.bearer.empty()?std::wstring{}:L"Authorization: Bearer "+wide(input.bearer)+L"\r\n");
    RequestHandle request(WinHttpOpenRequest(connection.value,method.c_str(),target.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,parts.nScheme==INTERNET_SCHEME_HTTPS?WINHTTP_FLAG_SECURE:0),completion,std::move(binding));
    DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER,disabled=WINHTTP_DISABLE_COOKIES|WINHTTP_DISABLE_AUTHENTICATION;
    DWORD_PTR context=reinterpret_cast<DWORD_PTR>(request.context.get());
    if(!WinHttpSetOption(request.value,WINHTTP_OPTION_CONTEXT_VALUE,&context,sizeof(context))||
       !WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy))||
       !WinHttpSetOption(request.value,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled)))failed();
    if(WinHttpSetStatusCallback(request.value,Completion::callback,WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS|WINHTTP_CALLBACK_FLAG_HANDLES,0)==WINHTTP_INVALID_STATUS_CALLBACK)failed();
    request.callbackInstalled=true;
    request.context.release(); // The final HANDLE_CLOSING callback owns deletion.
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(input.timeoutMs);
    if(!WinHttpSendRequest(request.value,headers.c_str(),DWORD(headers.size()),input.body.empty()?WINHTTP_NO_REQUEST_DATA:const_cast<char*>(input.body.data()),DWORD(input.body.size()),DWORD(input.body.size()),context))failed();
    completion->wait(WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE,stop,deadline);
    if(!WinHttpReceiveResponse(request.value,nullptr))failed();completion->wait(WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE,stop,deadline);
    DWORD status=0,length=sizeof(status);if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&length,WINHTTP_NO_HEADER_INDEX))failed();
    DWORD declared=0;length=sizeof(declared);
    if(WinHttpQueryHeaders(request.value,WINHTTP_QUERY_CONTENT_LENGTH|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&declared,&length,WINHTTP_NO_HEADER_INDEX)&&declared>input.maximumBytes)throw std::runtime_error("Lobby response exceeds its size limit.");
    NativeRoomHttpResponse result{status,{}};
    while(true){
        if(stop.stop_requested())throw std::runtime_error("Lobby request cancelled.");
        if(!WinHttpReadData(request.value,buffer.data(),DWORD(buffer.size()),nullptr))failed();completion->wait(WINHTTP_CALLBACK_STATUS_READ_COMPLETE,stop,deadline);
        const DWORD count=completion->bytes;if(!count)break;if(count>input.maximumBytes-result.body.size())throw std::runtime_error("Lobby response exceeds its size limit.");result.body.append(buffer.data(),count);
    }
    return result;
}
}
