#pragma once
#include "port/native_rtc_asset_host.hpp"
namespace partyboard::net {
// UI-thread owner: only the worker opens/hashes the private disc. Cancelling
// joins the worker before releasing its stream/result. No partial manifest is
// published and no completion callback enters the UI from the worker.
class NativeAssetPreparation {
    struct State {
        std::mutex mutex;
        std::shared_ptr<NativeSharedAsset> result;
        std::string error;
        bool done=false;
    };
    std::shared_ptr<State> state_=std::make_shared<State>();
    std::jthread worker_;
public:
    NativeAssetPreparation(std::filesystem::path path,std::string publicName){
        worker_=std::jthread([state=state_,path=std::move(path),name=std::move(publicName)](std::stop_token stop){
            try{
                auto result=std::make_shared<NativeSharedAsset>(path,name,stop);
                std::lock_guard lock(state->mutex);
                if(!stop.stop_requested())state->result=std::move(result);
                state->done=true;
            }catch(const std::exception& error){std::lock_guard lock(state->mutex);state->error=error.what();state->done=true;}
        });
    }
    NativeAssetPreparation(const NativeAssetPreparation&)=delete;
    NativeAssetPreparation& operator=(const NativeAssetPreparation&)=delete;
    bool done()const{std::lock_guard lock(state_->mutex);return state_->done;}
    std::string error()const{std::lock_guard lock(state_->mutex);return state_->error;}
    std::shared_ptr<NativeSharedAsset> result()const{std::lock_guard lock(state_->mutex);return state_->result;}
    void close(){worker_.request_stop();if(worker_.joinable())worker_.join();std::lock_guard lock(state_->mutex);state_->result.reset();}
    ~NativeAssetPreparation(){close();}
};
}
