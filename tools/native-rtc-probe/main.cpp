#include <rtc/rtc.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include "port/lockstep.hpp"
#include "port/recovery_identity.hpp"
#include "port/recovery_credentials.hpp"
#include "port/input_mailbox.hpp"

using Json = nlohmann::json;
static std::mutex outputMutex;
static void report(const Json& value) {
    std::lock_guard lock(outputMutex);
    std::cout << value.dump() << std::endl;
}

// Transport acceptance probe only. It does not authenticate a lobby, prepare
// a game, authorize simulation, or claim that native multiplayer is playable.
int main(int argc, char** argv) {
    try {
        const bool authenticatedMode = argc == 2 && std::string(argv[1]) == "--authenticated-lockstep";
        const bool lockstepMode = authenticatedMode || (argc == 2 && std::string(argv[1]) == "--lockstep");
        std::shared_ptr<partyboard::net::RecoveryIdentity> recovery;
        std::string recoveryNonce;
        if (authenticatedMode) {
            // Test bootstrap stands in for an original authenticated connection;
            // it is never accepted from the replacement offer/answer itself.
            std::string bootstrap;
            if (!std::getline(std::cin, bootstrap) || bootstrap.size() > 65536)
                throw std::runtime_error("Original recovery credential required");
            const auto command = Json::parse(bootstrap);
            if (command.at("type") != "original-credential") throw std::runtime_error("Original credential required");
            recovery = std::make_shared<partyboard::net::RecoveryIdentity>(command.at("credential"));
            recoveryNonce = partyboard::net::createRecoveryNonce();
        }
        partyboard::net::SessionId session{};
        for (unsigned i = 0; i < session.size(); ++i) session[i] = uint8_t(i + 1);
        constexpr uint32_t checkpoint = 0xdeadbeef;
        auto queue = std::make_shared<partyboard::net::Lockstep>(session,
            std::array<uint32_t, 4>{11, 22, 22, 22});
        auto queueMutex = std::make_shared<std::mutex>();
        auto mailbox = std::make_shared<partyboard::net::InputMailbox>();
        const auto inputRoute = mailbox->bind(22);
        partyboard::net::Pad localPad{};
        for (unsigned i = 0; i < localPad.size(); ++i) localPad[i] = uint8_t(i * 23);
        const auto localPacket = partyboard::net::encode({session, 0, checkpoint, 0, localPad});
        if (lockstepMode && queue->receive(11, localPacket) != partyboard::net::Receive::Accepted)
            throw std::runtime_error("Local lockstep packet rejected");
        auto connection = std::make_shared<rtc::PeerConnection>(rtc::Configuration{});
        connection->onGatheringStateChange([connection, recovery, recoveryNonce](rtc::PeerConnection::GatheringState state) {
            if (state != rtc::PeerConnection::GatheringState::Complete) return;
            const auto description = connection->localDescription();
            if (description) {
                if (recovery) report({{"type", "offer"}, {"packet", recovery->sign(
                    nlohmann::ordered_json{{"type", "offer"}, {"sdp", std::string(*description)}}, recoveryNonce)}});
                else report({{"type", "offer"}, {"sdp", std::string(*description)}});
            }
        });
        auto channel = connection->createDataChannel("partyboard-input-v1");
        channel->onOpen([channel, lockstepMode, localPacket] {
            if (lockstepMode) channel->send(reinterpret_cast<const rtc::byte*>(localPacket.data()), localPacket.size());
            report({{"type", "open"}});
        });
        channel->onError([](std::string error) { report({{"type", "error"}, {"message", error}}); });
        channel->onMessage([channel, lockstepMode, queue, queueMutex, mailbox, inputRoute](rtc::message_variant message) {
            auto* bytes = std::get_if<rtc::binary>(&message);
            if (!bytes || bytes->empty() || bytes->size() > 16384) {
                report({{"type", "error"}, {"message", "Bounded binary probe packet required"}});
                return;
            }
            if (bytes->size() == partyboard::net::Packet{}.size()) {
                const auto decoded = partyboard::net::decode(std::span<const uint8_t>(
                    reinterpret_cast<const uint8_t*>(bytes->data()), bytes->size()));
                if (!decoded) {
                    report({{"type", "error"}, {"message", "Malformed PBIP input packet"}});
                    return;
                }
                report({{"type", "decoded"}, {"slot", decoded->slot},
                    {"frame", std::to_string(decoded->frame)},
                    {"checkpoint", decoded->checkpoint}, {"session", decoded->session},
                    {"pad", decoded->pad}});
                if (lockstepMode) {
                    // Fixed test route identity, not packet-supplied identity or
                    // a substitute for production signed-lobby authentication.
                    std::lock_guard lock(*queueMutex);
                    const auto result = mailbox->push(inputRoute, std::span<const uint8_t>(
                        reinterpret_cast<const uint8_t*>(bytes->data()), bytes->size()));
                    if (result != partyboard::net::InputMailbox::Push::Queued || !mailbox->drain(*queue)) {
                        report({{"type", "error"}, {"message", "Lockstep packet rejected"}});
                        return;
                    }
                    const auto missing = queue->missingSlots();
                    const auto completed = queue->advance(0xdeadbeef);
                    if (missing && completed) throw std::runtime_error("Incomplete frame advanced");
                    if (completed) report({{"type", "committed"},
                        {"frame", std::to_string(completed->index)}, {"checkpoint", completed->checkpoint},
                        {"pads", completed->pads}, {"nextFrame", std::to_string(queue->nextFrame())}});
                    else report({{"type", "waiting"}, {"missing", missing},
                        {"nextFrame", std::to_string(queue->nextFrame())}});
                }
            } else if (lockstepMode) {
                report({{"type", "error"}, {"message", "PBIP packet required for lockstep"}});
                return;
            }
            channel->send(*bytes);
            report({{"type", "echo"}, {"bytes", bytes->size()}});
        });
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.size() > 65536) throw std::runtime_error("Oversized signaling command");
            const auto command = Json::parse(line);
            if (command.at("type") == "stop") break;
            if (command.at("type") != "answer") throw std::runtime_error("Answer required");
            if (recovery) {
                bool valid = false;
                try { valid = recovery->verify(command.at("packet"), "answer", recoveryNonce); }
                catch (const std::exception&) { }
                if (!valid) { report({{"type", "rejected"}}); continue; }
                connection->setRemoteDescription(rtc::Description(command.at("packet").at("description").at("sdp").get<std::string>(), "answer"));
            } else connection->setRemoteDescription(rtc::Description(command.at("sdp").get<std::string>(), "answer"));
        }
        channel->resetCallbacks();
        connection->resetCallbacks();
        channel->close();
        connection->close();
        channel.reset();
        connection.reset();
        rtc::Cleanup().wait();
        return 0;
    } catch (const std::exception& error) {
        report({{"type", "error"}, {"message", error.what()}});
        return 1;
    }
}
