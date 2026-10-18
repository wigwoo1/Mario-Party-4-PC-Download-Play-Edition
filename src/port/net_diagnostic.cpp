#include "port/net_diagnostic.h"
#include <filesystem>
#include "port/main.h"
#include <cstring>
static PartyBoardNativeInputExchange nativeInputExchange;
static void* nativeInputContext;
static s8 nativeGlyphLanes[4]={-1,-1,-1,-1};
static bool nativeGlyphLanesInstalled;
extern "C" int PartyBoardNativeInstallInputExchange(PartyBoardNativeInputExchange exchange,void* context){
    if(!exchange||nativeInputExchange||PartyBoard_IsRunning||PartyBoard_GamePreparedCheckpoint()<0)return 0;
    nativeInputExchange=exchange;nativeInputContext=context;nativeGlyphLanesInstalled=false;return 1;
}
extern "C" int PartyBoardNativeClearInputExchange(void){
    if(PartyBoard_IsRunning)return 0;
    nativeInputExchange=nullptr;nativeInputContext=nullptr;nativeGlyphLanesInstalled=false;return 1;
}
extern "C" int PartyBoardNativeClearOwnedInputExchange(PartyBoardNativeInputExchange exchange,void* context){
    if(PartyBoard_IsRunning||!exchange||nativeInputExchange!=exchange||nativeInputContext!=context)return 0;
    return PartyBoardNativeClearInputExchange();
}
extern "C" int PartyBoardNativeOnlineInputRequired(void){return nativeInputExchange!=nullptr;}
extern "C" int PartyBoardNativeSetOwnedGlyphLanes(PartyBoardNativeInputExchange exchange,void* context,const s8 lanes[4]){
    if(PartyBoard_IsRunning||!exchange||nativeInputExchange!=exchange||nativeInputContext!=context||!lanes)return 0;
    unsigned used=0;
    for(int slot=0;slot<4;++slot){
        const int lane=lanes[slot];
        if(lane < -1 || lane>=4 || (lane>=0&&(used&(1u<<lane))))return 0;
        if(lane>=0)used|=1u<<lane;
    }
    if(!used)return 0;
    std::memcpy(nativeGlyphLanes,lanes,sizeof(nativeGlyphLanes));nativeGlyphLanesInstalled=true;return 1;
}
extern "C" int PartyBoardNativeGlyphLane(int logicalPort){
    if(logicalPort<0||logicalPort>=4)return -1;
    return nativeInputExchange?(nativeGlyphLanesInstalled?nativeGlyphLanes[logicalPort]:-1):logicalPort;
}
static int sampleNativeInput(PADStatus pads[4]){
    if(!pads||!nativeInputExchange)return 0;
    PartyBoardInputState state{};if(!PartyBoardCaptureInputState(&state))return 0;
    PADStatus staged[4];std::memcpy(staged,pads,sizeof(staged));
    if(nativeInputExchange(nativeInputContext,staged,&state)!=1)return 0;
    PartyBoardInputState current{};
    if(!PartyBoardCaptureInputState(&current)||current.frame!=state.frame||current.checkpoint!=state.checkpoint||current.frand!=state.frand||current.rand8!=state.rand8)return 0;
    std::memcpy(pads,staged,sizeof(staged));return 1;
}
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include "port/net_diagnostic.h"
#include "port/netplay_handshake.hpp"
#include "port/input_wait.hpp"
#include "port/socket_channel.hpp"
#include "port/input_trace.h"
#include "port/controller_routes.hpp"
#include <SDL3/SDL_events.h>
#include <cstdio>
#include <fstream>
#include <memory>
#pragma comment(lib, "Ws2_32.lib")
extern "C" s32 rnd_seed;
namespace {
using namespace partyboard::net;
SOCKET connection = INVALID_SOCKET;
bool wsa = false, failed = false;
std::unique_ptr<PadExchange> exchange;
std::unique_ptr<SocketChannel> channel;
uint32_t peer;
uint64_t ticks() {
    return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
bool cancelled() {
    SDL_PumpEvents();
    SDL_Event event{};
    // Drain only close requests. Rendering and gameplay events remain in the normal loop.
    return SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_QUIT, SDL_EVENT_QUIT) > 0 ||
           SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_WINDOW_CLOSE_REQUESTED,
                          SDL_EVENT_WINDOW_CLOSE_REQUESTED) > 0;
}
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void waitSocket(SOCKET socket, bool write, uint64_t deadline) {
    for (;;) {
        require(!cancelled(), "cancelled");
        require(ticks() < deadline, "connection deadline exceeded");
        fd_set set; FD_ZERO(&set); FD_SET(socket, &set);
        timeval timeout{};
        const int result = select(0, write ? nullptr : &set, write ? &set : nullptr, nullptr, &timeout);
        require(result != SOCKET_ERROR, "socket selection failed");
        if (result > 0) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
void transfer(uint8_t* bytes, size_t size, bool write, uint64_t deadline) {
    while (size) {
        waitSocket(connection, write, deadline);
        const auto count = write ? send(connection, reinterpret_cast<char*>(bytes), int(size), 0)
                                 : recv(connection, reinterpret_cast<char*>(bytes), int(size), 0);
        if (count < 0 && WSAGetLastError() == WSAEWOULDBLOCK) continue;
        require(count > 0, "connection closed during compatibility exchange");
        bytes += count; size -= size_t(count);
    }
}
struct IO : WaitIO {
    bool send(std::span<const Packet> packets) override { return channel->enqueue(packets); }
    bool sendPending() const override { return channel->pendingBytes() != 0; }
    Pump pump(PadExchange& target) override {
        if (cancelled()) return Pump::Cancel;
        const auto status = channel->pump([&](std::span<const uint8_t> packet) {
            const auto accepted = target.receive(peer, packet);
            return accepted == Receive::Accepted || accepted == Receive::Duplicate;
        });
        return status == ChannelState::Connected ? Pump::Continue : Pump::Disconnected;
    }
    uint32_t checkpoint() override {
        auto hash = PartyBoardSessionCheckpoint();
        hash = (hash ^ PartyBoardRandomState()) * 16777619u;
        return (hash ^ uint32_t(rnd_seed)) * 16777619u;
    }
};
}
extern "C" int PartyBoardNetDiagnosticOpen(int host, unsigned short port, const char* path, u32 seed) {
    SOCKET listener = INVALID_SOCKET;
    try {
        require(!exchange && !wsa, "diagnostic already open");
        failed = false;
        std::array<uint8_t, 160> config{};
        std::ifstream file(path, std::ios::binary);
        require(bool(file.read(reinterpret_cast<char*>(config.data()), config.size())) && file.peek() == EOF,
                "diagnostic config must contain exactly 160 bytes");
        SessionId session{};
        std::copy_n(config.begin(), 16, session.begin());
        const auto expected = decodeHello(std::span(config).subspan(16));
        require(expected && expected->seed == seed, "invalid diagnostic compatibility/seed");
        exchange = std::make_unique<PadExchange>(session, std::array<uint32_t, 4>{11, 22, 11, 22}, host ? 11 : 22);
        peer = host ? 22 : 11;
        WSADATA data{};
        require(WSAStartup(MAKEWORD(2, 2), &data) == 0, "Winsock startup failed"); wsa = true;
        sockaddr_in address{}; address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); address.sin_port = htons(port);
        const auto deadline = ticks() + 30000;
        if (host) {
            listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            require(listener != INVALID_SOCKET, "listener creation failed");
            u_long mode = 1; require(ioctlsocket(listener, FIONBIO, &mode) == 0, "listener nonblocking setup failed");
            require(bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0 && listen(listener, 1) == 0,
                    "loopback listen failed");
            int length = sizeof(address);
            require(getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length) == 0, "port lookup failed");
            fprintf(stderr, "Net diagnostic: PORT %u\n", unsigned(ntohs(address.sin_port))); fflush(stderr);
            waitSocket(listener, false, deadline);
            connection = accept(listener, nullptr, nullptr);
            closesocket(listener); listener = INVALID_SOCKET;
            require(connection != INVALID_SOCKET, "accept failed");
        } else {
            require(port != 0, "join requires a port");
            connection = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            require(connection != INVALID_SOCKET, "socket creation failed");
            u_long mode = 1; require(ioctlsocket(connection, FIONBIO, &mode) == 0, "nonblocking setup failed");
            const auto result = connect(connection, reinterpret_cast<sockaddr*>(&address), sizeof(address));
            require(result == 0 || WSAGetLastError() == WSAEWOULDBLOCK, "loopback connection failed");
            waitSocket(connection, true, deadline);
            int error = 0, length = sizeof(error);
            require(getsockopt(connection, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &length) == 0 && !error,
                    "loopback connection failed");
        }
        channel = std::make_unique<SocketChannel>(connection);
        require(channel->state() == ChannelState::Connected, "nonblocking setup failed");
        const BOOL noDelay = TRUE;
        require(setsockopt(connection, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay)) == 0,
                "TCP setup failed");
        auto offered = config;
        transfer(offered.data(), offered.size(), true, deadline);
        transfer(offered.data(), offered.size(), false, deadline);
        require(offered == config, "session or compatibility mismatch");
        // Scripted input has no physical rumble; still exercise real negotiation.
        ControllerCapabilities capabilities({11, 22, 11, 22}, 1);
        const uint32_t local = host ? 11 : 22;
        auto capability = encodeCapabilities({session, 1, 0});
        require(capabilities.receive(local, session, capability), "local capability rejected");
        transfer(capability.data(), capability.size(), true, deadline);
        transfer(capability.data(), capability.size(), false, deadline);
        require(capabilities.receive(peer, session, capability), "remote capability rejected");
        const auto combined = capabilities.agreedMask();
        require(combined && *combined == 0, "scripted capability mismatch");
        auto acknowledgement = encodeCapabilityAck({session, 1, *combined});
        require(capabilities.receiveAck(local, session, acknowledgement), "local capability acknowledgement rejected");
        transfer(acknowledgement.data(), acknowledgement.size(), true, deadline);
        transfer(acknowledgement.data(), acknowledgement.size(), false, deadline);
        require(capabilities.receiveAck(peer, session, acknowledgement) && capabilities.ready(),
                "remote capability acknowledgement rejected");
        fprintf(stderr, "Net diagnostic: capabilities acknowledged mask=0\n");
        fprintf(stderr, "Net diagnostic: connected; paired input-script check only\n"); fflush(stderr);
        return 1;
    } catch (const std::exception& error) {
        if (listener != INVALID_SOCKET) closesocket(listener);
        fprintf(stderr, "Net diagnostic: %s\n", error.what()); failed = true;
        PartyBoardNetDiagnosticClose(); return 0;
    }
}
extern "C" int PartyBoardNetDiagnosticSample(PADStatus pads[4]) {
    if(nativeInputExchange)return sampleNativeInput(pads);
    if (!exchange) return !failed;
    std::array<Pad, 4> original{};
    for (unsigned i = 0; i < 4; ++i) original[i] = packPad(pads[i]);
    IO io;
    const auto result = waitForInput(*exchange, pads, io, 15000);
    if (result != WaitResult::Ready) {
        fprintf(stderr, "Net diagnostic: sample %llu stopped (%u)\n",
                (unsigned long long)exchange->nextFrame(), unsigned(result)); failed = true; return 0;
    }
    // Keep the paired-script assertion until live ownership is implemented.
    // Trace recording now happens after this gate accepts the merged input.
    for (unsigned i = 0; i < 4; ++i) if (packPad(pads[i]) != original[i]) {
        fprintf(stderr, "Net diagnostic: input script mismatch at slot %u\n", i); failed = true; return 0;
    }
    return 1;
}
extern "C" u32 PartyBoardNetDiagnosticRumble(u32 localCapability) {
    // This scripted diagnostic deliberately has no hardware rumble capability.
    // Live netplay will need slot-owner capability negotiation, not local PADRead bits.
    return (exchange || nativeInputExchange) ? 0 : localCapability;
}
extern "C" int PartyBoardNetDiagnosticClose(void) {
    if (exchange) fprintf(stderr, "Net diagnostic: closed after %llu paired samples\n",
                          (unsigned long long)exchange->nextFrame());
    exchange.reset(); channel.reset();
    if (connection != INVALID_SOCKET) { closesocket(connection); connection = INVALID_SOCKET; }
    if (wsa) { WSACleanup(); wsa = false; }
    return failed ? 2 : 0;
}
#else
#include "port/net_diagnostic.h"
int PartyBoardNetDiagnosticOpen(int, unsigned short, const char*, u32) { return 0; }
int PartyBoardNetDiagnosticSample(PADStatus pads[4]) { return nativeInputExchange?sampleNativeInput(pads):1; }
u32 PartyBoardNetDiagnosticRumble(u32 localCapability) { return nativeInputExchange?0:localCapability; }
int PartyBoardNetDiagnosticClose(void) { return 0; }
#endif


