#pragma once
#include "crypto.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <algorithm>
#include <array>
#include <vector>
#include <optional>
#include <stop_token>

namespace partyboard::net {
struct NativeRuntimeArtifact { std::string name; std::filesystem::path path; };
// Descriptor paths are local package metadata, never peer-provided paths.
// Relative entries follow the descriptor when a bundled directory is moved.
inline std::filesystem::path nativeRuntimeArtifactPath(const std::filesystem::path& descriptor,const std::string& entry){
    if(entry.empty())throw std::invalid_argument("Runtime artifact path required");
    const std::filesystem::path path(entry);
    return path.is_absolute()?path:(std::filesystem::absolute(descriptor).parent_path()/path).lexically_normal();
}
// Worker-thread measurement of the same canonical package manifest as
// runtime-build.mjs. This measures supplied bytes, not publisher authenticity
// or cross-platform simulation compatibility. Enumerate every executable.
inline nlohmann::ordered_json fingerprintNativeRuntimeFiles(std::vector<NativeRuntimeArtifact> artifacts,
        std::optional<std::string> expectedBuildId=std::nullopt,std::stop_token stop={}) {
    using Json=nlohmann::ordered_json;
    if(artifacts.empty()||artifacts.size()>256)throw std::invalid_argument("Runtime artifact list required");
    if(expectedBuildId&&!std::regex_match(*expectedBuildId,std::regex("^[a-f0-9]{64}$")))
        throw std::invalid_argument("Valid expected runtime package identity required");
    std::set<std::string> names;
    const std::regex valid("^[a-zA-Z0-9_.-]+(/[a-zA-Z0-9_.-]+)*$");
    for(const auto& artifact:artifacts){
        if(stop.stop_requested())throw std::runtime_error("Runtime package preparation cancelled");
        if(!std::regex_match(artifact.name,valid)||!names.insert(artifact.name).second)
            throw std::invalid_argument("Invalid runtime artifact name");
        size_t start=0;
        do {const auto end=artifact.name.find('/',start);const auto part=artifact.name.substr(start,end-start);
            if(part=="."||part=="..")throw std::invalid_argument("Invalid runtime artifact name");
            if(end==std::string::npos)break;start=end+1;
        }while(true);
    }
    std::sort(artifacts.begin(),artifacts.end(),[](const auto& a,const auto& b){return a.name<b.name;});
    const auto hex=[](const std::array<unsigned char,32>& bytes){std::string result;constexpr char d[]="0123456789abcdef";
        for(auto b:bytes){result+=d[b>>4];result+=d[b&15];}return result;};
    Json files=Json::array();
    for(const auto& artifact:artifacts){
        const auto size=std::filesystem::file_size(artifact.path);
        if(!size||size>1024ULL*1024*1024)throw std::invalid_argument("Bounded runtime artifact required");
        std::ifstream file(artifact.path,std::ios::binary);
        if(!file)throw std::runtime_error("Runtime artifact read failed");
        crypto::Sha256 digest;
        std::array<char,65536> buffer{};uint64_t read=0;
        while(file){if(stop.stop_requested())throw std::runtime_error("Runtime package preparation cancelled");file.read(buffer.data(),buffer.size());const auto count=file.gcount();read+=uint64_t(count);
            if(count)digest.update(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(buffer.data()),size_t(count)));}
        if(!file.eof()||read!=size)throw std::runtime_error("Runtime artifact changed or read failed");
        const auto hash=digest.finish();
        files.push_back({{"name",artifact.name},{"size",size},{"sha256",hex(hash)}});
    }
    const auto canonical=Json{{"version",1},{"profile","unlocked-no-save-v1"},{"files",files}}.dump();
    const auto hash=crypto::sha256(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(canonical.data()),canonical.size()));
    const auto buildId=hex(hash);
    if(expectedBuildId&&buildId!=*expectedBuildId)throw std::runtime_error("Runtime package differs from agreed identity");
    return {{"buildId",buildId},{"files",files}};
}
}
