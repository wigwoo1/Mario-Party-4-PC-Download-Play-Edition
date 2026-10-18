#include "port/recovery_identity.hpp"
#include "port/recovery_credentials.hpp"
#include <iostream>
#include <memory>
// Test commands only; no production credential or signaling endpoint is exposed.
int main() {
    using Json = nlohmann::ordered_json;
    std::unique_ptr<partyboard::net::RecoveryIdentity> identity;
    std::string line;
    while (std::getline(std::cin, line)) {
        try {
            if (line.size() > 800000) throw std::invalid_argument("Oversized test command");
            const auto command = Json::parse(line);
            Json result;
            if (command.at("type") == "create-credential") {
                if (identity) throw std::invalid_argument("Credential already bound");
                result = partyboard::net::createRecoveryCredential(command.at("session"), command.at("assetId"), command.at("peer"));
                identity = std::make_unique<partyboard::net::RecoveryIdentity>(result);
            } else if (command.at("type") == "nonce") {
                result = {{"nonce", partyboard::net::createRecoveryNonce()}};
            } else if (command.at("type") == "credential") {
                if (identity) throw std::invalid_argument("Credential already bound");
                identity = std::make_unique<partyboard::net::RecoveryIdentity>(command.at("credential"));
                result = {{"bound", true}};
            } else if (!identity) throw std::invalid_argument("Original credential required");
            else if (command.at("type") == "sign") result = identity->sign(command.at("description"), command.at("nonce"));
            else if (command.at("type") == "verify") result = {{"valid", identity->verify(command.at("packet"), command.at("expectedType"), command.value("expectedNonce", std::string{}))}};
            else throw std::invalid_argument("Unknown test command");
            std::cout << result.dump() << std::endl;
        } catch (const std::exception&) { std::cout << "{\"error\":\"Rejected recovery command\"}" << std::endl; }
    }
}
