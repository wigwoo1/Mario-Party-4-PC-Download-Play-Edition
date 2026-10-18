#pragma once
#include "port/asset_transfer.hpp"
#include <rtc/rtc.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <memory>

namespace partyboard::net {
// Prepare on an asset worker before creating guest owners. Shared file access
// is serialized across guests because NativeAsset retains one seekable stream.
struct NativeSharedAsset {
    NativeAsset asset;
    std::mutex reads;
    NativeSharedAsset(const std::filesystem::path& path,const std::string& publicName,std::stop_token stop={}):asset(path,publicName,stop){}
};
class NativeRtcAssetHost {
    struct State {
        std::mutex mutex;std::condition_variable changed;
        std::deque<std::string> controls;
        bool opened=false,closed=false,stopped=false,complete=false;
        uint64_t sentBytes=0;
        std::string error;
    };
    std::shared_ptr<State> state_=std::make_shared<State>();
    std::shared_ptr<rtc::DataChannel> channel_;
    std::jthread worker_;
public:
    NativeRtcAssetHost(std::shared_ptr<NativeSharedAsset> asset,std::shared_ptr<rtc::DataChannel> channel):channel_(std::move(channel)){
        if(!asset||!channel_||channel_->label()!="partyboard-assets-v1")throw std::invalid_argument("Dedicated native asset channel required");
        const auto state=state_;const std::weak_ptr<State> weak=state;
        channel_->onOpen([weak]{if(auto state=weak.lock()){std::lock_guard lock(state->mutex);state->opened=true;state->changed.notify_all();}});
        channel_->onClosed([weak]{if(auto state=weak.lock()){std::lock_guard lock(state->mutex);state->closed=true;state->changed.notify_all();}});
        channel_->onError([weak](std::string error){if(auto state=weak.lock()){std::lock_guard lock(state->mutex);state->error=std::move(error);state->closed=true;state->changed.notify_all();}});
        channel_->onMessage([weak](rtc::message_variant message){if(auto state=weak.lock()){
            std::lock_guard lock(state->mutex);if(state->closed||state->stopped)return;
            const auto text=std::get_if<std::string>(&message);
            if(!text||text->empty()||text->size()>2048||state->controls.size()>=18){state->error="Native asset control bounds exceeded";state->closed=true;}
            else state->controls.push_back(*text);
            state->changed.notify_all();
        }});
        {std::lock_guard lock(state->mutex);state->opened=channel_->isOpen();}
        worker_=std::jthread([state,asset,channel=channel_](std::stop_token token){
            std::stop_callback wake(token,[&]{state->changed.notify_all();});
            try{
                NativeAssetHost sender(asset->asset);
                bool advertised=false;
                for(;;){
                    std::string control;
                    {std::unique_lock lock(state->mutex);state->changed.wait(lock,[&]{return token.stop_requested()||state->stopped||state->closed||(state->opened&&!advertised)||!state->controls.empty();});
                     if(token.stop_requested()||state->stopped||state->closed){sender.cancel();return;}
                     if(!advertised){lock.unlock();channel->send(asset->asset.manifest().dump());advertised=true;continue;}
                     control=std::move(state->controls.front());state->controls.pop_front();}
                    std::vector<std::vector<uint8_t>> packets;
                    {std::lock_guard lock(asset->reads);packets=sender.control(control);}
                    for(const auto& packet:packets){
                        {std::lock_guard lock(state->mutex);if(state->stopped||state->closed){sender.cancel();return;}}
                        channel->send(reinterpret_cast<const rtc::byte*>(packet.data()),packet.size());
                        {std::lock_guard lock(state->mutex);state->sentBytes+=packet.size()-40;}
                    }
                    if(sender.complete()){std::lock_guard lock(state->mutex);state->complete=true;return;}
                }
            }catch(const std::exception& error){std::lock_guard lock(state->mutex);state->error=error.what();state->closed=true;}
        });
    }
    NativeRtcAssetHost(const NativeRtcAssetHost&)=delete;
    NativeRtcAssetHost& operator=(const NativeRtcAssetHost&)=delete;
    bool complete()const{std::lock_guard lock(state_->mutex);return state_->complete;}
    // Payload bytes handed to transport, not guest verification/readiness.
    uint64_t sentBytes()const{std::lock_guard lock(state_->mutex);return state_->sentBytes;}
    std::string error()const{std::lock_guard lock(state_->mutex);return state_->error;}
    void close(){
        if(!channel_)return;
        {std::lock_guard lock(state_->mutex);state_->stopped=true;state_->changed.notify_all();}
        channel_->resetCallbacks();worker_.request_stop();if(worker_.joinable())worker_.join();
        channel_->close();channel_.reset();
    }
    ~NativeRtcAssetHost(){close();}
};
}
