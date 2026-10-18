#pragma once
// Windows diagnostic TCP channel; the game/browser transport will use WebRTC.
#include <winsock2.h>
#include "lockstep.hpp"
#include <algorithm>
#include <cstring>

namespace partyboard::net {
enum class ChannelState { Connected, Disconnected, Error };
class SocketChannel {
public:
    static constexpr size_t Capacity = 48 * 16;
    // Socket lifetime belongs to the caller. Construction enables nonblocking I/O.
    explicit SocketChannel(SOCKET socket, size_t ioBudget = Capacity) : socket_(socket), budget_(ioBudget) {
        if (!budget_ || budget_ > Capacity) { state_ = ChannelState::Error; return; }
        u_long nonblocking = 1;
        if (ioctlsocket(socket_, FIONBIO, &nonblocking)) state_ = ChannelState::Error;
    }
    bool enqueue(std::span<const Packet> packets) {
        if (state_ != ChannelState::Connected || packets.size() > (Capacity - sendSize_) / 48)
            return false;
        // Copy bytes now: PadExchange releases its outgoing span after commit.
        for (const auto& packet : packets) {
            std::memcpy(send_.data() + sendSize_, packet.data(), packet.size());
            sendSize_ += packet.size();
        }
        return true;
    }
    // At most one send, one receive and 16 packet deliveries per call. Never wait.
    template<class Deliver> ChannelState pump(Deliver deliver) {
        if (state_ != ChannelState::Connected) return state_;
        if (sendSize_) {
            const auto sent = ::send(socket_, reinterpret_cast<const char*>(send_.data()), int((std::min)(sendSize_, budget_)), 0);
            if (sent > 0) {
                sendSize_ -= size_t(sent);
                std::memmove(send_.data(), send_.data() + sent, sendSize_);
            } else if (sent == 0 || WSAGetLastError() != WSAEWOULDBLOCK) {
                return state_ = ChannelState::Error;
            }
        }
        const auto received = ::recv(socket_, reinterpret_cast<char*>(receive_.data() + receiveSize_),
                                     int((std::min)(Capacity - receiveSize_, budget_)), 0);
        if (received == 0)
            return state_ = receiveSize_ ? ChannelState::Error : ChannelState::Disconnected;
        if (received < 0) {
            if (WSAGetLastError() != WSAEWOULDBLOCK) state_ = ChannelState::Error;
            return state_;
        }
        receiveSize_ += size_t(received);
        size_t consumed = 0;
        while (receiveSize_ - consumed >= 48) {
            if (!deliver(std::span<const uint8_t>(receive_.data() + consumed, 48)))
                return state_ = ChannelState::Error;
            consumed += 48;
        }
        receiveSize_ -= consumed;
        std::memmove(receive_.data(), receive_.data() + consumed, receiveSize_);
        return state_;
    }
    size_t pendingBytes() const { return sendSize_; }
    ChannelState state() const { return state_; }
private:
    SOCKET socket_;
    ChannelState state_{ChannelState::Connected};
    std::array<uint8_t, Capacity> send_{}, receive_{};
    size_t sendSize_{}, receiveSize_{};
    size_t budget_;
};
}
