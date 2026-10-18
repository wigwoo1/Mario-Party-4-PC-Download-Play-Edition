#pragma once
#include "port/native_pipeline_seed.hpp"
#include <nlohmann/json.hpp>

namespace partyboard::net {
struct NativeBundledPipelineSeed {
    std::shared_ptr<const NativePipelineSeed> snapshot;
    uint32_t definitions;
};
// Distribution metadata is local, bounded and never supplied by a peer. The
// measured snapshot is shared; Aurora merges the bundled rows into its cache.
inline NativeBundledPipelineSeed readNativeBundledPipelineSeed(const std::filesystem::path& directory){
    const auto metadata=directory/"initial_pipeline_cache.json";
    const auto database=directory/"initial_pipeline_cache.db";
    if(!std::filesystem::is_regular_file(metadata)||!std::filesystem::is_regular_file(database))
        throw std::runtime_error("Bundled shader definitions are missing; install the complete native package");
    if(std::filesystem::file_size(metadata)>4096)throw std::invalid_argument("Bounded bundled shader metadata required");
    std::ifstream input(metadata,std::ios::binary);
    std::string text(4097,'\0');input.read(text.data(),text.size());
    const auto count=input.gcount();
    if(count>4096)throw std::invalid_argument("Bounded bundled shader metadata required");
    text.resize(static_cast<size_t>(count));const auto manifest=nlohmann::json::parse(text);
    if(manifest.at("version")!=1||manifest.at("scope")!="discovered-pipelines-only"||
        !manifest.at("pipelineCount").is_number_unsigned()||manifest.at("pipelineCount").get<uint64_t>()<1||
        manifest.at("pipelineCount").get<uint64_t>()>1000000||!manifest.at("bytes").is_number_unsigned()||
        manifest.at("bytes").get<uint64_t>()<100||manifest.at("bytes").get<uint64_t>()>32*1024*1024)
        throw std::invalid_argument("Invalid bundled shader metadata");
    const auto snapshot=readNativePipelineSeed(database,manifest.at("sha256").get<std::string>());
    if(snapshot->bytes.size()!=manifest.at("bytes").get<uint64_t>())
        throw std::invalid_argument("Bundled shader size differs from metadata");
    return {snapshot,manifest.at("pipelineCount").get<uint32_t>()};
}
}
