#pragma once
#include "port/native_runtime_build.hpp"
#include <optional>
#include "port/native_session_configuration.hpp"
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#endif

namespace partyboard::net {
#ifdef _WIN32
// New host sessions use OS entropy; recovery must retain the resulting
// configuration instead of invoking this factory again.
inline nlohmann::ordered_json createNativeHostSessionPackage(
        std::vector<NativeRuntimeArtifact> artifacts,
        const std::string& expectedBuildId,const std::string& measuredAssetId,std::stop_token stop={},std::optional<uint32_t> diagnosticSeed=std::nullopt);
#endif
// Worker boundary for the native lobby: derive advertised artifacts from the
// measured package, never from a second independently supplied list.
// The caller still owns asset measurement and loaded-engine attestation.
inline nlohmann::ordered_json measureNativeSessionPackage(
        std::vector<NativeRuntimeArtifact> artifacts,
        const std::string& expectedBuildId,const std::string& session,
        uint32_t seed,const std::string& measuredAssetId,std::stop_token stop={}) {
    auto measured=fingerprintNativeRuntimeFiles(std::move(artifacts),expectedBuildId,stop);
    bool wrapper=false,wasm=false;
    std::vector<std::string> extras;
    for(const auto& file:measured.at("files")) {
        const auto name=file.at("name").get<std::string>();
        if(name=="contract.mjs")wrapper=true;
        else if(name=="contract.wasm")wasm=true;
        else extras.push_back(name);
    }
    if(!wrapper||!wasm)throw std::invalid_argument("Shared package requires browser wrapper and Wasm");
    measured["configuration"]=nativeSessionConfiguration(session,seed,measuredAssetId,
        measured.at("buildId").get<std::string>(),extras);
    return measured;
}
#ifdef _WIN32
inline nlohmann::ordered_json createNativeHostSessionPackage(
        std::vector<NativeRuntimeArtifact> artifacts,
        const std::string& expectedBuildId,const std::string& measuredAssetId,std::stop_token stop,std::optional<uint32_t> diagnosticSeed) {
    std::array<unsigned char,20> entropy{};
    if(BCryptGenRandom(nullptr,entropy.data(),static_cast<ULONG>(entropy.size()),
            BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)
        throw std::runtime_error("Native host session entropy unavailable");
    std::string session;
    constexpr char digits[]="0123456789abcdef";
    for(size_t i=0;i<16;++i){session+=digits[entropy[i]>>4];session+=digits[entropy[i]&15];}
    if(session==std::string(32,'0'))throw std::runtime_error("Invalid native host session entropy");
    const uint32_t seed=(uint32_t(entropy[16])<<24)|(uint32_t(entropy[17])<<16)|
        (uint32_t(entropy[18])<<8)|uint32_t(entropy[19]);
    return measureNativeSessionPackage(std::move(artifacts),expectedBuildId,session,diagnosticSeed.value_or(seed),measuredAssetId,stop);
}
#endif
}
