#pragma once
#include "port/session_start_barrier.hpp"
#include "port/native_session_configuration.hpp"
#include <optional>
#include <map>

namespace partyboard::net {
// Caller captures the actual stopped engine and completed GPU preparation.
// Discovered pipelines remain a limited readiness scope, not full coverage.
inline nlohmann::ordered_json nativePreparedRuntimeReport(
        const PreparedStartRuntime& runtime,const nlohmann::ordered_json& configuration,
        bool rendererPrepared,std::optional<std::string> pipelineSeed=std::nullopt){
    const auto& seed=configuration.at("seed");
    if(!seed.is_number_integer()||seed.get<int64_t>()<0||seed.get<uint64_t>()>UINT32_MAX)
        throw std::invalid_argument("Invalid prepared native seed");
    std::vector<std::string> extras;
    if(configuration.contains("runtimeArtifacts"))extras=configuration.at("runtimeArtifacts").get<std::vector<std::string>>();
    const auto expected=nativeSessionConfiguration(configuration.at("session").get<std::string>(),
        seed.get<uint32_t>(),runtime.assetId,runtime.buildId,extras);
    if(nlohmann::json(expected)!=nlohmann::json(configuration)||!runtime.idle||
       runtime.seed!=seed.get<uint32_t>()||runtime.checkpoint!=runtime.preparedCheckpoint||!rendererPrepared)
        throw std::invalid_argument("Native prepared receipt requires matching idle engine and GPU preparation");
    if(pipelineSeed&&(pipelineSeed->size()!=64||!std::all_of(pipelineSeed->begin(),pipelineSeed->end(),
        [](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');})))
        throw std::invalid_argument("Invalid native pipeline identity");
    return {{"type","runtime-prepared"},{"version",1},{"session",configuration.at("session")},
        {"assetId",runtime.assetId},{"seed",runtime.seed},{"buildId",runtime.buildId},
        {"checkpoint",runtime.checkpoint},{"frame",0},
        {"pipelineSeed",pipelineSeed?nlohmann::ordered_json(*pipelineSeed):nlohmann::ordered_json(nullptr)},
        {"shaderScope","discovered-pipelines-only"}};
}
// Transport authentication supplies the bound peer separately. This parser
// only validates receipt identity/state; it cannot authenticate a sender.
inline nlohmann::ordered_json receiveNativePreparedRuntimeReport(
        const std::string& packet,const nlohmann::ordered_json& configuration){
    if(packet.size()>2048)throw std::invalid_argument("Native prepared packet exceeds bound");
    const auto message=nlohmann::ordered_json::parse(packet);
    const auto uint=[](const nlohmann::ordered_json& value){return value.is_number_integer()&&
        value.get<int64_t>()>=0&&value.get<uint64_t>()<=UINT32_MAX;};
    if(message.at("type")!="runtime-prepared"||message.at("version")!=1||
       message.at("session")!=configuration.at("session")||message.at("frame")!=0||
       message.at("shaderScope")!="discovered-pipelines-only"||
       !uint(message.at("seed"))||!uint(message.at("checkpoint")))
        throw std::invalid_argument("Invalid native prepared receipt");
    std::optional<std::string> pipeline;
    if(message.contains("pipelineSeed")&&!message.at("pipelineSeed").is_null())
        pipeline=message.at("pipelineSeed").get<std::string>();
    PreparedStartRuntime runtime{message.at("assetId").get<std::string>(),message.at("buildId").get<std::string>(),
        message.at("seed").get<uint32_t>(),message.at("checkpoint").get<uint32_t>(),message.at("checkpoint").get<uint32_t>(),true};
    return nativePreparedRuntimeReport(runtime,configuration,true,pipeline);
}
inline bool nativePreparedRuntimeReportsMatch(const nlohmann::ordered_json& host,
        const nlohmann::ordered_json& guest){
    for(const auto* key:{"session","assetId","seed","buildId","checkpoint","pipelineSeed","frame","shaderScope"})
        if(host.at(key)!=guest.at(key))return false;
    return true;
}
// Coordinator provides receipts keyed by authenticated transport peer identity,
// never a peer name claimed inside a packet. Membership/ready state is still
// checked by the barrier and must come from the current owned lobby snapshot.
inline SessionStartBarrier nativeStartBarrierFromPreparedReceipts(
        const PreparedStartLobby& lobby,const nlohmann::ordered_json& configuration,
        const PreparedStartRuntime& runtime,const nlohmann::ordered_json& hostReceipt,
        const std::map<std::string,nlohmann::ordered_json>& authenticatedReceipts){
    const auto expectedHost=receiveNativePreparedRuntimeReport(hostReceipt.dump(),configuration);
    if(!nativePreparedRuntimeReportsMatch(expectedHost,
        nativePreparedRuntimeReport(runtime,configuration,true,
            expectedHost.at("pipelineSeed").is_null()?std::nullopt:
                std::optional<std::string>(expectedHost.at("pipelineSeed").get<std::string>()))))
        throw std::invalid_argument("Local prepared receipt differs from captured engine");
    if(authenticatedReceipts.size()!=lobby.players.size())
        throw std::invalid_argument("Every assigned player must supply a prepared receipt");
    for(const auto& player:lobby.players){
        const auto found=authenticatedReceipts.find(player.peer);
        if(found==authenticatedReceipts.end()||!nativePreparedRuntimeReportsMatch(expectedHost,
            receiveNativePreparedRuntimeReport(found->second.dump(),configuration)))
            throw std::invalid_argument("Assigned prepared receipt missing or mismatched");
    }
    return SessionStartBarrier::fromConfiguration(lobby,configuration,runtime);
}
}
