#pragma once
#include <filesystem>
#include <cmath>
#include "main.h"
#include "input_trace.h"
#include "session_start_barrier.hpp"
#include "native_input_poll.hpp"
#include "native_session_configuration.hpp"
#include "native_start_coordinator.hpp"
namespace partyboard::net {
// Game-thread capture after initialization and start-clock alignment. Asset/build
// identities come from the verified runtime owner, never from a remote message.
// Does not initialize, install input, send commit or authorize game execution.
inline PreparedStartRuntime captureNativePreparedStart(const std::string& assetId,const std::string& buildId){
    if(PartyBoard_IsRunning||PartyBoard_GameLifecyclePhase()!=2||PartyBoard_GameFrameCount()!=0)
        throw std::runtime_error("Native prepared engine must be idle at frame zero");
    const auto prepared=PartyBoard_GamePreparedCheckpoint();
    if(!std::isfinite(prepared)||prepared<0||prepared>UINT32_MAX||prepared!=double(uint32_t(prepared)))
        throw std::runtime_error("Native start clock/state not prepared");
    uint32_t seed=0;PartyBoardInputState state{};
    if(!PartyBoardFixedSeed(&seed)||!PartyBoardCaptureInputState(&state)||state.frame!=0||state.checkpoint!=uint32_t(prepared))
        throw std::runtime_error("Native prepared capture differs");
    return {assetId,buildId,seed,state.checkpoint,uint32_t(prepared),true};
}
// Runtime owner calls after real disc/assets/platform setup in a fresh process.
// Initialization creates the ordinary boot overlay but executes no game frames.
// Renderer pipeline preparation must still finish before advertising readiness.
inline PreparedStartRuntime initializeNativePreparedEngine(uint32_t agreedSeed,
        const std::string& verifiedAssetId,const std::string& verifiedBuildId){
    if(PartyBoard_GameLifecyclePhase()!=0||PartyBoardNativeOnlineInputRequired())
        throw std::runtime_error("Fresh native engine required for online session");
    if(PartyBoard_GameConfigureSessionSeed(agreedSeed)!=0)
        throw std::runtime_error("Native agreed seed configuration rejected");
    PartyBoard_GameRequestStop();
    if(PartyBoard_GameInitialize()!=0||PartyBoard_GameAlignStartClock()!=0)
        throw std::runtime_error("Native online engine preparation failed");
    auto runtime=captureNativePreparedStart(verifiedAssetId,verifiedBuildId);
    if(runtime.seed!=agreedSeed)throw std::runtime_error("Native engine did not retain agreed seed");
    return runtime;
}
// Coordinator boundary: compare retained advertised identity against local
// measurements before mutating the fresh engine. Never use remote identity as
// the evidence that the local assets or executable package were measured.
inline PreparedStartRuntime initializeNativeConfiguredEngine(
        const nlohmann::ordered_json& configuration,
        const std::string& measuredAssetId,const std::string& measuredBuildId){
    const auto& seed=configuration.at("seed");
    if(!seed.is_number_integer()||seed.get<int64_t>()<0||seed.get<uint64_t>()>UINT32_MAX)
        throw std::invalid_argument("Invalid retained native session seed");
    std::vector<std::string> extras;
    if(configuration.contains("runtimeArtifacts"))
        extras=configuration.at("runtimeArtifacts").get<std::vector<std::string>>();
    const auto expected=nativeSessionConfiguration(configuration.at("session").get<std::string>(),
        seed.get<uint32_t>(),measuredAssetId,measuredBuildId,extras);
    // JSON objects compare without relying on serialization key ordering.
    if(nlohmann::json(configuration)!=nlohmann::json(expected))
        throw std::invalid_argument("Retained native configuration differs from local measurements");
    return initializeNativePreparedEngine(seed.get<uint32_t>(),measuredAssetId,measuredBuildId);
}
// Host coordinator prepares its commit and attaches input while the actual
// engine is still stopped. The binding must outlive a successful stopped clear.
// Caller delivers commit to every assigned transport before authorizing resume.
template<class IO>
void installNativeCommitInput(const nlohmann::ordered_json& commit,NativeInputPollBinding<IO>& binding){
    RecoverySession identity{commit.at("session").template get<std::string>(),commit.at("buildId").template get<std::string>(),
        commit.at("assetId").template get<std::string>(),commit.at("profile").template get<std::string>(),
        commit.at("seed").template get<uint32_t>(),commit.at("revision").template get<uint64_t>(),commit.at("membership").template get<uint64_t>(),{}};
    for(const auto& player:commit.at("players"))identity.players.push_back({player.at("peer").template get<std::string>(),player.at("slot").template get<uint8_t>()});
    if(!binding.installOriginalSession(identity))throw std::runtime_error("Prepared input installation rejected");
}
template<class IO>
nlohmann::ordered_json prepareNativeInputCommit(SessionStartBarrier& barrier,const PreparedStartLobby& lobby,
        const std::string& verifiedAssetId,const std::string& verifiedBuildId,NativeInputPollBinding<IO>& binding){
    auto commit=barrier.prepareCommit(lobby,captureNativePreparedStart(verifiedAssetId,verifiedBuildId));
    installNativeCommitInput(commit,binding);
    return commit;
}
// Sender returns true only after every assigned transport accepted the commit.
// Failure leaves the engine stopped, cancels this agreement and detaches only
// its binding. Successful delivery still requires explicit caller resume.
template<class IO,class Sender>
bool deliverNativeInputCommit(SessionStartBarrier& barrier,const PreparedStartLobby& lobby,
        const std::string& verifiedAssetId,const std::string& verifiedBuildId,NativeInputPollBinding<IO>& binding,Sender&& send){
    bool installed=false;
    try{
        auto commit=prepareNativeInputCommit(barrier,lobby,verifiedAssetId,verifiedBuildId,binding);
        installed=true;
        if(send(commit))return true;
    }catch(...){ }
    barrier.cancel();
    if(installed&&!binding.detachStopped())throw std::runtime_error("Stopped input rollback rejected; binding must remain alive");
    return false;
}
template<class IO,class Sender>
bool deliverNativeCoordinatedInputCommit(NativeStartCoordinator& coordinator,
        const std::string& measuredAssetId,const std::string& measuredBuildId,
        const std::string& localHost,NativeInputPollBinding<IO>& binding,Sender&& send){
    bool installed=false;
    try{
        const auto captured=captureNativePreparedStart(measuredAssetId,measuredBuildId);
        const auto commit=coordinator.prepareCommit(captured);
        installNativeCommitInput(commit,binding);installed=true;
        if(coordinator.deliverCommit(captured,localHost,std::forward<Sender>(send)))return true;
    }catch(...){ }
    coordinator.cancel();
    if(installed&&!binding.detachStopped())
        throw std::runtime_error("Native coordinator rollback rejected; retain input owner");
    return false;
}
}
