#pragma once
#include "recovery_identity.hpp"

namespace partyboard::net {
// For the original lobby connection only. Never regenerate a credential when
// accepting a replacement connection; retain the original RecoveryIdentity.
inline std::string recoveryRandomHex(size_t count) {
    if (count != 16 && count != 32) throw std::invalid_argument("Recovery random size");
    std::array<unsigned char, 32> bytes{};
    crypto::secureRandom(std::span<uint8_t>(bytes.data(), count));
    static constexpr char digits[] = "0123456789abcdef";
    std::string text;
    for (size_t i = 0; i < count; ++i) {
        text += digits[bytes[i] >> 4];
        text += digits[bytes[i] & 15];
    }
    return text;
}
inline nlohmann::ordered_json createRecoveryCredential(const std::string& session,
        const std::string& assetId, const std::string& peer) {
    nlohmann::ordered_json credential{{"type", "recovery-credential"}, {"version", 1},
        {"session", session}, {"assetId", assetId}, {"peer", peer},
        {"secret", recoveryRandomHex(32)}};
    RecoveryIdentity validate(credential);
    return credential;
}
inline std::string createRecoveryNonce() { return recoveryRandomHex(16); }
}
