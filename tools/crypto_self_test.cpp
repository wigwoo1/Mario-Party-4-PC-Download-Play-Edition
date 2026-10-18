#include "port/crypto.hpp"
#include <algorithm>
#include <iostream>
#include <string>

static std::string hex(std::span<const uint8_t> bytes) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (auto byte : bytes) {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 15]);
    }
    return result;
}

int main() {
    const std::string empty;
    const auto emptyDigest = partyboard::crypto::sha256(std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(empty.data()), empty.size()));
    if (hex(emptyDigest) != "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") return 1;

    const std::string message = "abc";
    partyboard::crypto::Sha256 streaming;
    streaming.update(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(message.data()), 1));
    streaming.update(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(message.data() + 1), 2));
    if (hex(streaming.finish()) != "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") return 2;

    const std::string repeated(1000, 'a');
    partyboard::crypto::Sha256 longStreaming;
    for (size_t offset = 0; offset < repeated.size(); offset += 37) {
        const auto count = std::min<size_t>(37, repeated.size() - offset);
        longStreaming.update(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(repeated.data() + offset), count));
    }
    if (hex(longStreaming.finish()) != "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3") return 3;

    std::array<uint8_t, 20> key{};
    key.fill(0x0b);
    const std::string hmacMessage = "Hi There";
    const auto hmac = partyboard::crypto::hmacSha256(key, std::span<const uint8_t>(
        reinterpret_cast<const uint8_t*>(hmacMessage.data()), hmacMessage.size()));
    if (hex(hmac) != "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7") return 4;

    std::array<uint8_t, 32> random{};
    partyboard::crypto::secureRandom(random);
    bool nonzero = false;
    for (auto byte : random) nonzero |= byte != 0;
    if (!nonzero) return 5;
    std::cout << "crypto SHA-256, streaming, HMAC-SHA256, and secure-random checks passed\n";
}
