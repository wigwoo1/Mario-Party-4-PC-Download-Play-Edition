#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <string>
#include <stdexcept>
#include <vector>
#include <set>
#include <regex>
namespace partyboard::net {
// The native coordinator supplies locally measured asset/package identities.
// This canonical wire configuration does not itself authorize engine execution.
inline nlohmann::ordered_json nativeSessionConfiguration(const std::string& session,
        uint32_t seed,const std::string& assetId,const std::string& measuredBuildId,
        const std::vector<std::string>& runtimeArtifacts={}) {
    const auto hex=[](const std::string& value,size_t length){return value.size()==length&&
        std::all_of(value.begin(),value.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});};
    if(!hex(session,32)||session==std::string(32,'0')||!hex(assetId,64)||!hex(measuredBuildId,64))
        throw std::invalid_argument("Invalid measured native session configuration");
    if(runtimeArtifacts.size()>254)throw std::invalid_argument("Bounded shared artifact list required");
    std::set<std::string> names{"contract.mjs","contract.wasm"};
    const std::regex valid("^[a-zA-Z0-9_.-]+(/[a-zA-Z0-9_.-]+)*$");
    for(const auto& name:runtimeArtifacts){
        if(name.size()>512||!std::regex_match(name,valid)||!names.insert(name).second)
            throw std::invalid_argument("Invalid shared runtime artifact");
        size_t start=0;
        do{const auto end=name.find('/',start);const auto part=name.substr(start,end-start);
            if(part=="."||part=="..")throw std::invalid_argument("Invalid shared runtime artifact");
            if(end==std::string::npos)break;start=end+1;
        }while(true);
    }
    nlohmann::ordered_json result={{"type","session-configuration"},{"version",1},{"profile","unlocked-no-save-v1"},
        {"assetId",assetId},{"session",session},{"seed",seed},{"buildId",measuredBuildId}};
    if(!runtimeArtifacts.empty())result["runtimeArtifacts"]=runtimeArtifacts;
    if(result.dump().size()>65536)throw std::invalid_argument("Session configuration packet exceeds bound");
    return result;
}
inline nlohmann::ordered_json receiveNativeSessionConfiguration(const std::string& text,
        const std::string& verifiedAssetId,const std::string& measuredBuildId){
    if(text.empty()||text.size()>65536)throw std::invalid_argument("Bounded session configuration required");
    const auto packet=nlohmann::ordered_json::parse(text);
    const auto& seed=packet.at("seed");
    if(!seed.is_number_integer()||seed.get<int64_t>()<0||seed.get<uint64_t>()>UINT32_MAX)
        throw std::invalid_argument("Invalid retained session seed");
    std::vector<std::string> artifacts;
    if(packet.contains("runtimeArtifacts"))artifacts=packet.at("runtimeArtifacts").get<std::vector<std::string>>();
    const auto canonical=nativeSessionConfiguration(packet.at("session").get<std::string>(),seed.get<uint32_t>(),verifiedAssetId,measuredBuildId,artifacts);
    if(nlohmann::json(packet)!=nlohmann::json(canonical))throw std::invalid_argument("Session differs from verified asset or measured runtime");
    return canonical;
}
}
