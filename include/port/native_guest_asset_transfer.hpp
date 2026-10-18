#pragma once
#include "port/native_asset_receiver.hpp"
#include <rtc/rtc.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>

namespace partyboard::net {
// Feed only the assigned invitation's asset messages. Close/join this owner
// before destroying its staging stream or RTC connection.
class NativeGuestAssetTransfer {
    struct State {
        std::mutex mutex;std::condition_variable changed;
        std::deque<rtc::message_variant> messages;
        bool stopped=false,complete=false;
        uint64_t received=0;
        std::string error;
        std::string assetId;
    };
    std::shared_ptr<State> state_=std::make_shared<State>();
    std::jthread worker_;
public:
    NativeGuestAssetTransfer(std::ostream& staging,std::shared_ptr<rtc::DataChannel> channel){
        if(!channel||channel->label()!="partyboard-assets-v1")throw std::invalid_argument("Assigned asset channel required");
        const auto state=state_;
        worker_=std::jthread([state,channel,&staging](std::stop_token token){
            std::stop_callback wake(token,[state]{state->changed.notify_all();});
            std::unique_ptr<NativeAssetReceiver> receiver;
            try{for(;;){
                rtc::message_variant message;
                {std::unique_lock lock(state->mutex);state->changed.wait(lock,[&]{return token.stop_requested()||state->stopped||!state->error.empty()||!state->messages.empty();});
                 if(token.stop_requested()||state->stopped||!state->error.empty())return;
                 message=std::move(state->messages.front());state->messages.pop_front();}
                if(const auto* text=std::get_if<std::string>(&message)){
                    if(receiver||text->size()>2048)throw std::runtime_error("Unexpected asset manifest");
                    const auto manifest=nlohmann::ordered_json::parse(*text);
                    receiver=std::make_unique<NativeAssetReceiver>(manifest,staging);
                    {std::lock_guard lock(state->mutex);state->assetId=manifest.at("id").get<std::string>();}
                }else{
                    if(!receiver)throw std::runtime_error("Asset packet before manifest");
                    const auto& bytes=std::get<rtc::binary>(message);
                    receiver->accept({reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size()});
                }
                {std::lock_guard lock(state->mutex);if(state->stopped||token.stop_requested()||!state->error.empty())return;state->received=receiver->receivedBytes();}
                if(!channel->isOpen())throw std::runtime_error("Asset channel closed during transfer");
                if(receiver->complete()){
                    channel->send(receiver->completion());
                    std::lock_guard lock(state->mutex);
                    if(!state->stopped&&!token.stop_requested()&&state->error.empty())state->complete=true;
                    return;
                }
                if(receiver->needsRequest())channel->send(receiver->request());
            }}catch(const std::exception& error){std::lock_guard lock(state->mutex);state->error=error.what();}
        });
    }
    void submit(rtc::message_variant message){
        std::lock_guard lock(state_->mutex);
        if(state_->stopped||state_->complete||!state_->error.empty())throw std::runtime_error("Asset worker stopped");
        if(state_->messages.size()>=18||std::visit([](const auto& value){return value.size();},message)>16384){state_->error="Asset receive window exceeded";state_->changed.notify_all();throw std::runtime_error(state_->error);}
        state_->messages.push_back(std::move(message));state_->changed.notify_all();
    }
    bool complete()const{std::lock_guard lock(state_->mutex);return state_->complete&&!state_->stopped&&state_->error.empty();}
    uint64_t receivedBytes()const{std::lock_guard lock(state_->mutex);return state_->received;}
    std::string error()const{std::lock_guard lock(state_->mutex);return state_->error;}
    std::string verifiedAssetId()const{std::lock_guard lock(state_->mutex);if(!state_->complete||state_->stopped||!state_->error.empty())throw std::runtime_error("Verified asset required");return state_->assetId;}
    void close(){
        {std::lock_guard lock(state_->mutex);state_->stopped=true;state_->complete=false;state_->messages.clear();state_->changed.notify_all();}
        worker_.request_stop();if(worker_.joinable())worker_.join();
    }
    ~NativeGuestAssetTransfer(){close();}
};
}
