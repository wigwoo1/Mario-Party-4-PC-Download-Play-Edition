#pragma once
#include "port/native_session_package.hpp"
#include <mutex>
#include <thread>
#include <memory>
#ifdef _WIN32
namespace partyboard::net {
// UI polls this owner. Hashing and new-session entropy remain on its worker;
// cancellation joins before disposal and never publishes a partial package.
class NativePackagePreparation {
    struct State {std::mutex mutex;std::optional<nlohmann::ordered_json> result;std::string error;bool done=false;};
    std::shared_ptr<State> state_=std::make_shared<State>();
    std::jthread worker_;
public:
    NativePackagePreparation(std::vector<NativeRuntimeArtifact> artifacts,std::string expectedBuildId,std::string measuredAssetId,std::optional<uint32_t> diagnosticSeed=std::nullopt){
        worker_=std::jthread([state=state_,files=std::move(artifacts),build=std::move(expectedBuildId),asset=std::move(measuredAssetId),diagnosticSeed](std::stop_token stop){
            try{
                auto result=createNativeHostSessionPackage(files,build,asset,stop,diagnosticSeed);
                std::lock_guard lock(state->mutex);
                if(!stop.stop_requested())state->result=std::move(result);
                state->done=true;
            }catch(const std::exception& error){std::lock_guard lock(state->mutex);state->error=error.what();state->done=true;}
        });
    }
    NativePackagePreparation(const NativePackagePreparation&)=delete;
    NativePackagePreparation& operator=(const NativePackagePreparation&)=delete;
    bool done()const{std::lock_guard lock(state_->mutex);return state_->done;}
    std::string error()const{std::lock_guard lock(state_->mutex);return state_->error;}
    std::optional<nlohmann::ordered_json> result()const{std::lock_guard lock(state_->mutex);return state_->result;}
    void close(){worker_.request_stop();if(worker_.joinable())worker_.join();std::lock_guard lock(state_->mutex);state_->result.reset();}
    ~NativePackagePreparation(){close();}
};
}
#endif
