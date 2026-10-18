#include "port/native_runtime_build.hpp"
#include "port/native_session_package.hpp"
#include "port/asset_transfer.hpp"
#include "port/native_package_preparation.hpp"
#include <iostream>
int main(){try{
    nlohmann::ordered_json input;std::cin>>input;
    std::vector<partyboard::net::NativeRuntimeArtifact> artifacts;
    const auto& files=input.is_array()?input:input.at("files");
    for(const auto& item:files)artifacts.push_back({item.at("name").get<std::string>(),item.at("path").get<std::string>()});
    std::optional<std::string> expected;
    if(!input.is_array())expected=input.at("expectedBuildId").get<std::string>();
    nlohmann::ordered_json measured;
    if(!input.is_array()&&(input.contains("session")||input.value("newHostSession",false))){
        const bool fresh=input.value("newHostSession",false);
        if(fresh&&(input.contains("session")||input.contains("seed")))
            throw std::invalid_argument("Fresh host session cannot override entropy");
        const auto seed=fresh?nlohmann::ordered_json(0):input.at("seed");
        if(!seed.is_number_integer()||seed.get<int64_t>()<0||seed.get<uint64_t>()>UINT32_MAX)
            throw std::invalid_argument("Invalid native session seed");
        nlohmann::ordered_json assetManifest;
        auto assetId=input.at("assetId").get<std::string>();
        if(input.contains("assetPath")){
            partyboard::net::NativeAsset asset(input.at("assetPath").get<std::string>(),"Game assets");
            const auto manifest=asset.manifest();
            if(manifest.at("id")!=assetId)throw std::runtime_error("Private asset differs from expected identity");
            assetId=manifest.at("id").get<std::string>();
            assetManifest=manifest;
        }
        if(fresh&&input.value("asyncPreparation",false)){
            partyboard::net::NativePackagePreparation preparation(std::move(artifacts),*expected,assetId);
            while(!preparation.done())std::this_thread::sleep_for(std::chrono::milliseconds(10));
            const auto result=preparation.result();if(!result)throw std::runtime_error(preparation.error());
            measured=*result;preparation.close();if(preparation.result())throw std::runtime_error("Closed package preparation retained result");
        }
        else if(fresh)measured=partyboard::net::createNativeHostSessionPackage(std::move(artifacts),*expected,assetId);
        else measured=partyboard::net::measureNativeSessionPackage(std::move(artifacts),*expected,
            input.at("session").get<std::string>(),seed.get<uint32_t>(),assetId);
        if(!assetManifest.is_null())measured["assetManifest"]=std::move(assetManifest);
    }else {
        std::stop_source cancellation;
        if(!input.is_array()&&input.value("cancelMeasurement",false))cancellation.request_stop();
        measured=partyboard::net::fingerprintNativeRuntimeFiles(std::move(artifacts),expected,cancellation.get_token());
    }
    std::cout<<measured.dump()<<std::endl;
}catch(const std::exception& error){std::cerr<<error.what()<<std::endl;return 1;}}
