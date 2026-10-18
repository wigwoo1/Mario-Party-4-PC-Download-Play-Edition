#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include <stdexcept>

namespace partyboard::net {
struct RecoveryPlayer { std::string peer; uint8_t slot; bool operator==(const RecoveryPlayer&) const=default; };
struct RecoverySession {
    std::string session,buildId,assetId,profile;
    uint32_t seed;
    uint64_t revision,membership;
    std::vector<RecoveryPlayer> players;
    bool operator==(const RecoverySession&) const=default;
};
struct PausedCapture {
    uint32_t missingOwner;
    uint64_t epoch,revision,inputEpoch,inputRevision,attemptFrame,inputFrame;
    uint32_t frame,checkpoint,frand,rand8,attemptCheckpoint,attemptFrand,attemptRand8;
    bool paused;
};
inline bool recoveryHex(const std::string& value,size_t length) {
    return value.size()==length&&value.find_first_not_of("0123456789abcdef")==std::string::npos;
}
// Session must be retained from accepted start; capture must come from the
// suspended game thread. Peer policy epochs remain local and are not compared
// across players, matching the browser's paused-state report contract.
inline nlohmann::ordered_json recoveryProposal(const RecoverySession& identity,
        const PausedCapture& captured,const std::string& host,const std::string& nonce) {
    if(!recoveryHex(identity.session,32)||!recoveryHex(identity.buildId,64)||
       !recoveryHex(identity.assetId,64)||identity.profile!="unlocked-no-save-v1"||
       identity.revision>9007199254740991ULL||identity.membership>identity.revision||
       identity.players.size()<2||identity.players.size()>4||!recoveryHex(nonce,32)||
       !captured.paused||!captured.missingOwner||!captured.epoch||!captured.revision||
       captured.inputEpoch!=captured.epoch||captured.inputRevision!=captured.revision||
       captured.inputFrame!=captured.attemptFrame||captured.checkpoint!=captured.attemptCheckpoint||
       captured.frand!=captured.attemptFrand||captured.rand8!=captured.attemptRand8)
        throw std::invalid_argument("Original identity and current suspended input required");
    std::set<std::string> peers;std::set<uint8_t> slots;
    auto players=nlohmann::ordered_json::array();
    for(const auto& player:identity.players) {
        // Production peer IDs are ASCII; this bound is deliberately conservative
        // for UTF-8 rather than accepting oversized unvalidated identifiers.
        if(player.peer.empty()||player.peer.size()>128||player.slot>=4||
           !peers.insert(player.peer).second||!slots.insert(player.slot).second)
            throw std::invalid_argument("Distinct assigned recovery players required");
        players.push_back({{"peer",player.peer},{"slot",player.slot}});
    }
    if(!peers.contains(host))throw std::invalid_argument("Assigned recovery host required");
    return {{"type","session-recovery-proposal"},{"version",1},{"session",identity.session},
        {"buildId",identity.buildId},{"assetId",identity.assetId},{"profile",identity.profile},
        {"seed",identity.seed},{"membership",identity.membership},{"lobbyRevision",identity.revision},
        {"players",players},{"frame",captured.frame},{"checkpoint",captured.checkpoint},
        {"frand",captured.frand},{"rand8",captured.rand8},{"inputFrame",std::to_string(captured.inputFrame)},
        {"nonce",nonce},{"host",host}};
}
// Invoke only for the original authenticated host's control channel. The
inline nlohmann::ordered_json nativePausedStateReport(const RecoverySession& identity,
        const PausedCapture& captured,const std::string& reporter,const std::string& nonce){
    bool assigned=false;for(const auto& player:identity.players)assigned|=player.peer==reporter;
    if(!assigned)throw std::invalid_argument("Assigned paused report identity required");
    auto report=recoveryProposal(identity,captured,identity.players.front().peer,nonce);
    report.erase("host");report.erase("nonce");report["type"]="paused-state-report";
    report["reporter"]=reporter;report["nonce"]=nonce;return report;
}
// Invoke only for the original authenticated host's control channel. The
// coordinator separately checks its retained attempt and monotonic deadline.
inline bool acceptRecoveryCommit(const nlohmann::ordered_json& message,
        const nlohmann::ordered_json& acknowledged,const RecoverySession& identity,
        const PausedCapture& captured,const std::string& localPeer,const std::string& boundHost) {
    bool assigned=false;
    for(const auto& player:identity.players)assigned|=player.peer==localPeer;
    if(!assigned||!acknowledged.is_object()||!acknowledged.contains("nonce")||
       !acknowledged["nonce"].is_string())return false;
    try {
        const auto actual=recoveryProposal(identity,captured,boundHost,acknowledged["nonce"].get<std::string>());
        if(acknowledged!=actual)return false;
        auto expected=actual;expected["type"]="session-recovery-commit";
        expected["acknowledged"]=nlohmann::ordered_json::array();
        for(const auto& player:identity.players)expected["acknowledged"].push_back(player.peer);
        return message==expected;
    }catch(const std::exception&){return false;}
}
}
