// Test-only Windows loopback transport. Not a game transport or public server.
#include <winsock2.h>
#include <ws2tcpip.h>
#include "../include/port/netplay_handshake.hpp"
#include "../include/port/socket_channel.hpp"
#include "../include/port/input_wait.hpp"
#include "../include/port/controller_routes.hpp"
#include <cstdio>
#include <string>
#pragma comment(lib, "Ws2_32.lib")
using namespace partyboard::net;
struct Socket {
    SOCKET value{INVALID_SOCKET};
    ~Socket() { if (value != INVALID_SOCKET) closesocket(value); }
};
static void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static void transfer(SOCKET socket, uint8_t* data, size_t length, bool write) {
    while (length) {
        // Deliberately fragment writes; TCP readers must assemble exact packets.
        const int count = write ? send(socket, reinterpret_cast<const char*>(data), int((length > 7) ? 7 : length), 0)
                                : recv(socket, reinterpret_cast<char*>(data), int(length), 0);
        require(count > 0, "connection closed or socket timeout");
        data += count; length -= count;
    }
}
struct ProbeIO : WaitIO {
    SocketChannel channel;
    uint32_t peer, hash{42};
    ProbeIO(SOCKET socket, uint32_t remote) : channel(socket, 31), peer(remote) {}
    bool send(std::span<const Packet> packets) override { return channel.enqueue(packets); }
    bool sendPending() const override { return channel.pendingBytes() != 0; }
    Pump pump(PadExchange& exchange) override {
        const auto result = channel.pump([&](std::span<const uint8_t> packet) {
            const auto accepted = exchange.receive(peer, packet);
            return accepted == Receive::Accepted || accepted == Receive::Duplicate;
        });
        return result == ChannelState::Connected ? Pump::Continue : Pump::Disconnected;
    }
    uint32_t checkpoint() override { return hash; }
};
int main(int argc, char** argv) {
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data)) return 2;
    int status = 0;
    try {
        require(argc == 4, "Usage: probe listen|connect port normal|mismatch|disconnect");
        const bool host = std::string(argv[1]) == "listen";
        require(host || std::string(argv[1]) == "connect", "Invalid role");
        const auto port = std::stoul(argv[2]);
        require(port <= 65535 && (host || port), "Invalid port");
        const std::string scenario = argv[3];
        require(scenario == "normal" || scenario == "mismatch" || scenario == "disconnect" ||
                scenario == "bad-capability" || scenario == "bad-ack", "Invalid scenario");
        Socket listener, connection;
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(static_cast<u_short>(port));
        if (host) {
            listener.value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            require(listener.value != INVALID_SOCKET, "socket failed");
            require(bind(listener.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "bind failed");
            require(listen(listener.value, 1) == 0, "listen failed");
            int size = sizeof(address);
            require(getsockname(listener.value, reinterpret_cast<sockaddr*>(&address), &size) == 0, "port lookup failed");
            printf("PORT %u\n", unsigned(ntohs(address.sin_port))); fflush(stdout);
            fd_set sockets; FD_ZERO(&sockets); FD_SET(listener.value, &sockets);
            timeval timeout{5, 0};
            require(select(0, &sockets, nullptr, nullptr, &timeout) > 0, "accept timeout");
            connection.value = accept(listener.value, nullptr, nullptr);
            require(connection.value != INVALID_SOCKET, "accept failed");
        } else {
            connection.value = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            require(connection.value != INVALID_SOCKET, "socket failed");
            require(connect(connection.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "connect failed");
        }
        const DWORD timeout = 5000;
        require(setsockopt(connection.value, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0, "timeout setup failed");
        require(setsockopt(connection.value, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0, "timeout setup failed");
        const BOOL noDelay = TRUE;
        require(setsockopt(connection.value, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay)) == 0, "TCP setup failed");
        Compatibility config;
        config.build[0] = 1; config.assets[0] = 2; config.save[0] = 3; config.settings[0] = 4;
        config.seed = (!host && scenario == "mismatch") ? 43 : 42;
        auto hello = encodeHello(config);
        transfer(connection.value, hello.data(), hello.size(), true);
        transfer(connection.value, hello.data(), hello.size(), false);
        const auto remote = decodeHello(hello);
        require(remote && compare(config, *remote) == Mismatch::None, "compatibility mismatch");
        SessionId session{1, 2, 3, 4}; // Fixed test fixture only, not an invitation token.
        const uint32_t local = host ? 11 : 22, peer = host ? 22 : 11;
        ControllerCapabilities capabilities({11, 22, 11, 22}, 1);
        const uint32_t localMask = host ? 0xa0000000u : 0x40000000u;
        auto capability = encodeCapabilities({session, 1, localMask});
        require(capabilities.receive(local, session, capability), "local capability rejected");
        if (!host && scenario == "bad-capability")
            capability = encodeCapabilities({session, 1, 0x80000000u}); // Claims host slot.
        transfer(connection.value, capability.data(), capability.size(), true);
        transfer(connection.value, capability.data(), capability.size(), false);
        require(capabilities.receive(peer, session, capability), "remote capability rejected");
        const auto combined = capabilities.agreedMask();
        require(combined && *combined == 0xe0000000u, "combined capability mismatch");
        auto acknowledgement = encodeCapabilityAck({session, 1, *combined});
        require(capabilities.receiveAck(local, session, acknowledgement), "local acknowledgement rejected");
        if (!host && scenario == "bad-ack")
            acknowledgement = encodeCapabilities({session, 1, *combined}); // Offer reflected as ack.
        transfer(connection.value, acknowledgement.data(), acknowledgement.size(), true);
        transfer(connection.value, acknowledgement.data(), acknowledgement.size(), false);
        require(capabilities.receiveAck(peer, session, acknowledgement) && capabilities.ready(),
                "remote acknowledgement rejected");
        printf("CAPABILITIES agreed=%u\n", *combined); fflush(stdout);
        PadExchange exchange(session, {11, 22, 11, 22}, local);
        ProbeIO io(connection.value, peer);
        for (uint64_t frame = 0; frame < 360; ++frame) {
            if (!host && scenario == "disconnect" && frame == 10)
                throw std::runtime_error("injected disconnect");
            PADStatus pads[4]{};
            for (uint8_t slot = host ? 0 : 1; slot < 4; slot += 2) {
                Pad raw{};
                for (unsigned i = 0; i < 11; ++i) raw[i] = uint8_t(frame + slot * 13 + i);
                pads[slot] = unpackPad(raw);
            }
            require(waitForInput(exchange, pads, io, 5000) == WaitResult::Ready,
                    "connection closed or socket timeout");
            for (const auto& pad : pads) for (auto byte : packPad(pad)) io.hash = (io.hash ^ byte) * 16777619u;
        }
        // Compare the final post-frame state too, not just the preceding checkpoint.
        PADStatus final[4]{};
        require(waitForInput(exchange, final, io, 5000) == WaitResult::Ready, "final state mismatch");
        printf("PASS frames=360 hash=%u\n", io.hash);
    } catch (const std::exception& error) {
        fprintf(stderr, "FAIL %s\n", error.what()); status = 2;
    }
    WSACleanup();
    return status;
}
