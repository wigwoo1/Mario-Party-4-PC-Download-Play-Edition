#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <bcrypt.h>
#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

namespace partyboard::crypto {

class Sha256 {
public:
    void update(std::span<const uint8_t> input) {
        if (finished_) throw std::logic_error("SHA256 already finished");
        byteCount_ += input.size();
        while (!input.empty()) {
            const auto count = (input.size() < block_.size() - used_)
                ? input.size() : block_.size() - used_;
            for (size_t i = 0; i < count; ++i) block_[used_ + i] = input[i];
            used_ += count;
            input = input.subspan(count);
            if (used_ == block_.size()) {
                transform(block_.data());
                used_ = 0;
            }
        }
    }

    std::array<uint8_t, 32> finish() {
        if (finished_) throw std::logic_error("SHA256 already finished");
        const uint64_t bitCount = byteCount_ * 8;
        block_[used_++] = 0x80;
        if (used_ > 56) {
            while (used_ < block_.size()) block_[used_++] = 0;
            transform(block_.data());
            used_ = 0;
        }
        while (used_ < 56) block_[used_++] = 0;
        for (unsigned i = 0; i < 8; ++i)
            block_[63 - i] = static_cast<uint8_t>(bitCount >> (i * 8));
        transform(block_.data());

        std::array<uint8_t, 32> result{};
        for (size_t i = 0; i < state_.size(); ++i) {
            result[i * 4] = static_cast<uint8_t>(state_[i] >> 24);
            result[i * 4 + 1] = static_cast<uint8_t>(state_[i] >> 16);
            result[i * 4 + 2] = static_cast<uint8_t>(state_[i] >> 8);
            result[i * 4 + 3] = static_cast<uint8_t>(state_[i]);
        }
        finished_ = true;
        return result;
    }

private:
    static constexpr std::array<uint32_t, 64> constants_ = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    static constexpr uint32_t rotate(uint32_t value, unsigned count) {
        return (value >> count) | (value << (32 - count));
    }
    void transform(const uint8_t* block) {
        uint32_t words[64]{};
        for (unsigned i = 0; i < 16; ++i)
            words[i] = (uint32_t(block[i*4]) << 24) | (uint32_t(block[i*4+1]) << 16) |
                (uint32_t(block[i*4+2]) << 8) | uint32_t(block[i*4+3]);
        for (unsigned i = 16; i < 64; ++i) {
            const auto s0 = rotate(words[i-15],7) ^ rotate(words[i-15],18) ^ (words[i-15] >> 3);
            const auto s1 = rotate(words[i-2],17) ^ rotate(words[i-2],19) ^ (words[i-2] >> 10);
            words[i] = words[i-16] + s0 + words[i-7] + s1;
        }
        auto a=state_[0], b=state_[1], c=state_[2], d=state_[3];
        auto e=state_[4], f=state_[5], g=state_[6], h=state_[7];
        for (unsigned i = 0; i < 64; ++i) {
            const auto sum1 = rotate(e,6) ^ rotate(e,11) ^ rotate(e,25);
            const auto choice = (e & f) ^ (~e & g);
            const auto temp1 = h + sum1 + choice + constants_[i] + words[i];
            const auto sum0 = rotate(a,2) ^ rotate(a,13) ^ rotate(a,22);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto temp2 = sum0 + majority;
            h=g; g=f; f=e; e=d+temp1; d=c; c=b; b=a; a=temp1+temp2;
        }
        state_[0]+=a; state_[1]+=b; state_[2]+=c; state_[3]+=d;
        state_[4]+=e; state_[5]+=f; state_[6]+=g; state_[7]+=h;
    }

    std::array<uint32_t, 8> state_ = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    std::array<uint8_t, 64> block_{};
    uint64_t byteCount_{};
    size_t used_{};
    bool finished_{};
};

inline std::array<uint8_t, 32> sha256(std::span<const uint8_t> input) {
    Sha256 digest;
    digest.update(input);
    return digest.finish();
}

inline std::array<uint8_t, 32> hmacSha256(std::span<const uint8_t> key,
        std::span<const uint8_t> input) {
    std::array<uint8_t, 64> normalized{};
    if (key.size() > normalized.size()) {
        const auto digest = sha256(key);
        for (size_t i = 0; i < digest.size(); ++i) normalized[i] = digest[i];
    } else {
        for (size_t i = 0; i < key.size(); ++i) normalized[i] = key[i];
    }
    std::array<uint8_t, 64> innerPad{}, outerPad{};
    for (size_t i = 0; i < normalized.size(); ++i) {
        innerPad[i] = normalized[i] ^ 0x36;
        outerPad[i] = normalized[i] ^ 0x5c;
    }
    Sha256 inner;
    inner.update(innerPad);
    inner.update(input);
    const auto innerDigest = inner.finish();
    Sha256 outer;
    outer.update(outerPad);
    outer.update(innerDigest);
    return outer.finish();
}

inline void secureRandom(std::span<uint8_t> output) {
    if (output.empty()) return;
#if defined(_WIN32)
    while (!output.empty()) {
        const auto count = static_cast<ULONG>(output.size() > std::numeric_limits<ULONG>::max()
            ? std::numeric_limits<ULONG>::max() : output.size());
        if (BCryptGenRandom(nullptr, output.data(), count, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
            throw std::runtime_error("Windows CNG random generation failed");
        output = output.subspan(count);
    }
#elif defined(__EMSCRIPTEN__)
    if (output.size() > 65536) throw std::invalid_argument("Web Crypto random request too large");
    const auto ok = EM_ASM_INT({
        try {
            if (!globalThis.crypto || !globalThis.crypto.getRandomValues) return 0;
            globalThis.crypto.getRandomValues(HEAPU8.subarray($0, $0 + $1));
            return 1;
        } catch (_) { return 0; }
    }, output.data(), static_cast<int>(output.size()));
    if (!ok) throw std::runtime_error("Web Crypto random generation failed");
#else
    std::ifstream random("/dev/urandom", std::ios::binary);
    if (!random || !random.read(reinterpret_cast<char*>(output.data()),
            static_cast<std::streamsize>(output.size())) ||
        random.gcount() != static_cast<std::streamsize>(output.size()))
        throw std::runtime_error("Operating-system random generation failed");
#endif
}

} // namespace partyboard::crypto
