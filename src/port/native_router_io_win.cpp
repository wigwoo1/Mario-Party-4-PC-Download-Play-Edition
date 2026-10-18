#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
#include "port/native_router_io.hpp"
extern "C" {
#include "connecthostport.h"
#include "receivedata.h"
}

namespace {
int budgetMs(){const auto budget=partyboard::net::NativeRouterIoBudget::current;return budget?budget->remainingMs():0;}
int ready(SOCKET socket,bool write,int maximumWait){
    const auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(maximumWait);
    while(true){
        const int remaining=std::min<int>(budgetMs(),static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(end-std::chrono::steady_clock::now()).count()));
        if(remaining<=0)return 0;
        fd_set sockets,errors;FD_ZERO(&sockets);FD_SET(socket,&sockets);FD_ZERO(&errors);FD_SET(socket,&errors);
        const int slice=std::min(remaining,50);timeval wait{0,slice*1000};
        const int result=select(0,write?nullptr:&sockets,write?&sockets:nullptr,&errors,&wait);
        if(result==SOCKET_ERROR||FD_ISSET(socket,&errors))return -1;
        if(result>0&&FD_ISSET(socket,&sockets))return 1;
    }
}
}
// These two hooks replace only MiniUPnPc's Windows socket wait functions. The
// pinned upstream parser/discovery/SOAP source remains unmodified. No hostname
// DNS lookup is allowed on this path, because it has no cancellable deadline.
extern "C" SOCKET connecthostport(const char* host,unsigned short port,unsigned int scope){
    if(!host||!port||!budgetMs())return INVALID_SOCKET;
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(port);
    if(scope||InetPtonA(AF_INET,host,&address.sin_addr)!=1)return INVALID_SOCKET;
    SOCKET socket=::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(socket==INVALID_SOCKET)return INVALID_SOCKET;
    const auto fail=[&]{closesocket(socket);return INVALID_SOCKET;};
    u_long mode=1;if(ioctlsocket(socket,FIONBIO,&mode)==SOCKET_ERROR)return fail();
    const int connected=connect(socket,reinterpret_cast<const sockaddr*>(&address),sizeof(address));
    if(connected==SOCKET_ERROR){
        const int error=WSAGetLastError();
        if(error!=WSAEWOULDBLOCK&&error!=WSAEINPROGRESS)return fail();
        if(ready(socket,true,budgetMs())!=1)return fail();
        int result=0,length=sizeof(result);
        if(getsockopt(socket,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&result),&length)==SOCKET_ERROR||result)return fail();
    }
    if(!budgetMs())return fail();
    mode=0;if(ioctlsocket(socket,FIONBIO,&mode)==SOCKET_ERROR)return fail();
    const DWORD timeout=static_cast<DWORD>(std::min(budgetMs(),200));
    if(!timeout||setsockopt(socket,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout))==SOCKET_ERROR||
       setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout))==SOCKET_ERROR)return fail();
    return socket;
}
extern "C" int receivedata(SOCKET socket,char* data,int length,int timeout,unsigned int* scope){
    if(scope)*scope=0;
    auto* budget=partyboard::net::NativeRouterIoBudget::current;
    if(!budget||length<=0||timeout<=0)return -1;
    const size_t allowed=budget->receiveLimit(static_cast<size_t>(length));
    if(!allowed)return -1;
    const int readable=ready(socket,false,std::min(timeout,budgetMs()));
    if(readable<=0)return readable;
    const int count=recv(socket,data,static_cast<int>(allowed),0);
    if(count>0)budget->consumed(static_cast<size_t>(count));
    return count;
}
