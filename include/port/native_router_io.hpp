#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <stop_token>

namespace partyboard::net {
// Shared by the adapter and its Windows socket hooks. Nested operation budgets
// cannot extend a worker's overall cleanup deadline. No game clock is involved.
class NativeRouterIoBudget {
    using Clock=std::chrono::steady_clock;
    NativeRouterIoBudget* parent_;
    Clock::time_point deadline_;
    std::stop_token stop_;
    size_t bytesLeft_;
    bool failed_=false;
public:
    inline static thread_local NativeRouterIoBudget* current=nullptr;
    NativeRouterIoBudget(std::stop_token stop={},uint32_t milliseconds=3000,size_t maxBytes=262144)
        :parent_(current),deadline_(Clock::now()+std::chrono::milliseconds(milliseconds)),stop_(stop),bytesLeft_(maxBytes){current=this;}
    NativeRouterIoBudget(const NativeRouterIoBudget&)=delete;
    ~NativeRouterIoBudget(){current=parent_;}
    int remainingMs(){
        const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline_-Clock::now()).count();
        if(failed_||stop_.stop_requested()||remaining<=0){failed_=true;return 0;}
        const int value=static_cast<int>(std::min<int64_t>(remaining,5000));
        return parent_?std::min(value,parent_->remainingMs()):value;
    }
    size_t receiveLimit(size_t wanted){
        if(!bytesLeft_||!remainingMs()){failed_=true;return 0;}
        const auto available=std::min(wanted,bytesLeft_);
        return parent_?parent_->receiveLimit(available):available;
    }
    void consumed(size_t bytes){bytesLeft_-=std::min(bytesLeft_,bytes);if(parent_)parent_->consumed(bytes);}
    bool ok(){return remainingMs()>0;}
};
}
