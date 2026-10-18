#pragma once
#include "port/native_guest_staging_file.hpp"
#include "port/native_renderer_preparation.hpp"
#include "port/native_prepared_report.hpp"

namespace partyboard::net {
// Actual game-thread boundary. Local package measurement precedes this call.
// This opens only the verified private staged file and never resumes gameplay.
template<class OpenDisc,class PlatformSetup,class Pump>
nlohmann::ordered_json prepareNativeGuestEngine(NativeGuestJoinSession& session,
        NativeGuestStagingFile& staging,const std::string& measuredBuildId,
        OpenDisc&& openDisc,PlatformSetup&& platformSetup,Pump&& pump,std::optional<std::string> installedPipelineSeed=std::nullopt){
    const auto& configuration=session.configuration();
    (void)session.recoveryIdentity(); // Original credential retained before readiness.
    const auto& path=staging.sealVerified(session);
    // Check local package identity before any engine/platform mutation.
    const auto canonical=receiveNativeSessionConfiguration(configuration.dump(),
        configuration.at("assetId").get<std::string>(),measuredBuildId);
    if(PartyBoard_GameLifecyclePhase()!=0||PartyBoardNativeOnlineInputRequired())
        throw std::runtime_error("Fresh native Join engine required");
    if(!openDisc(path))throw std::runtime_error("Verified Join disc could not open");
    platformSetup();
    const auto prepared=initializeNativeConfiguredEngine(canonical,
        canonical.at("assetId").get<std::string>(),measuredBuildId);
    prepareNativeRenderer(prepared,[&]{
        session.pump();
        return session.error().empty()&&session.assetsComplete()&&pump();
    });
    // Engine/GPU receipt is evidence for readiness only, never a Start commit.
    return nativePreparedRuntimeReport(prepared,canonical,true,installedPipelineSeed);
}
inline void publishNativeGuestPreparedReceipt(NativeGuestJoinSession& session,
        const nlohmann::ordered_json& receipt){
    const auto& configuration=session.configuration();
    (void)session.recoveryIdentity();
    if(!session.assetsComplete())throw std::runtime_error("Verified Join assets required for readiness");
    const auto validated=receiveNativePreparedRuntimeReport(receipt.dump(),configuration);
    const auto captured=captureNativePreparedStart(configuration.at("assetId"),configuration.at("buildId"));
    if(validated.at("checkpoint")!=captured.checkpoint||validated.at("seed")!=captured.seed)
        throw std::runtime_error("Join receipt differs from actual stopped engine");
    const auto channel=session.transport().channel("partyboard-preparation-v1");
    if(!channel||!channel->isOpen())throw std::runtime_error("Original assigned Preparation channel required");
    channel->send(validated.dump());
}
// Game-thread Ready publication. A freshly captured engine must still match
// the GPU receipt, and readiness names the current host membership generation.
inline void publishNativeGuestReady(NativeGuestJoinSession& session,
        const nlohmann::ordered_json& receipt,const PreparedStartLobby& lobby,
        uint64_t& sequence,bool ready){
    const auto& configuration=session.configuration();
    (void)session.recoveryIdentity();
    if(!session.assetsComplete()||lobby.assetId!=configuration.at("assetId").get<std::string>()||
        lobby.total!=session.receivedBytes()||sequence>=9007199254740991ULL)
        throw std::runtime_error("Current verified Join lobby required");
    bool assigned=false;
    for(const auto& player:lobby.players)if(player.peer==session.transport().assignedPeer()){
        if(player.slot==0)throw std::invalid_argument("Guest cannot report host readiness");
        assigned=true;
    }
    if(!assigned)throw std::invalid_argument("Original Join membership required");
    const auto validated=receiveNativePreparedRuntimeReport(receipt.dump(),configuration);
    const auto captured=captureNativePreparedStart(configuration.at("assetId"),configuration.at("buildId"));
    if(!nativePreparedRuntimeReportsMatch(validated,nativePreparedRuntimeReport(captured,configuration,true,
        validated.at("pipelineSeed").is_null()?std::nullopt:std::optional<std::string>(validated.at("pipelineSeed").get<std::string>()))))
        throw std::runtime_error("Join readiness differs from stopped prepared engine");
    const auto channel=session.transport().channel("partyboard-lobby-v1");
    if(!channel||!channel->isOpen())throw std::runtime_error("Original Join Lobby channel required");
    const nlohmann::ordered_json report{{"assetId",lobby.assetId},{"sequence",sequence+1},
        {"bytes",session.receivedBytes()},{"verified",true},{"shaders",true},{"ready",ready},
        {"membership",lobby.membership},{"saveHash",nullptr}};
    channel->send(nlohmann::ordered_json{{"type","lobby-report"},{"version",1},{"report",report}}.dump());
    ++sequence;
}
}

