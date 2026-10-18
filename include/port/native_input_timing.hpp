#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>

namespace partyboard::net {
// Read-only host diagnostics. These times never enter PAD packets, checkpoints,
// RNG, simulation clocks or the recovery policy's monotonic milliseconds.
struct NativeInputTiming {
    struct Span {
        uint64_t calls=0;
        double totalMs=0,maxMs=0;
        std::array<uint64_t,4> buckets{}; // <2ms, [2,8), [8,20), >=20.
    };
    bool enabled=false;
    uint64_t startedPolls=0,completedPolls=0;
    Span pump,send,wait,poll;
};
class NativeInputTimer {
public:
    using Clock=std::function<double()>;
    static bool requested() {
        const auto* value=std::getenv("PARTYBOARD_INPUT_TIMING");
        return value&&value[0]=='1'&&value[1]=='\0';
    }
    explicit NativeInputTimer(bool enabled=requested(),Clock clock={}) : clock_(std::move(clock)) {
        data_.enabled=enabled;
        if(enabled&&!clock_)clock_=[] {return std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now().time_since_epoch()).count();};
    }
    class Scope {
        NativeInputTimer& timer_;
        NativeInputTiming::Span& span_;
        double start_=0;
    public:
        Scope(NativeInputTimer& timer,NativeInputTiming::Span& span):timer_(timer),span_(span){
            if(timer_.data_.enabled)start_=timer_.clock_();
        }
        ~Scope(){if(timer_.data_.enabled)record(span_,timer_.clock_()-start_);}
        Scope(const Scope&)=delete;
        Scope& operator=(const Scope&)=delete;
    };
    Scope pump(){return Scope(*this,data_.pump);}
    Scope send(){return Scope(*this,data_.send);}
    Scope wait(){return Scope(*this,data_.wait);}
    void beginPoll(){if(data_.enabled){pollStarted_=clock_();pollPending_=true;++data_.startedPolls;}}
    void accepted(){if(data_.enabled&&pollPending_){record(data_.poll,clock_()-pollStarted_);++data_.completedPolls;pollPending_=false;}}
    const NativeInputTiming& snapshot()const noexcept{return data_;}
private:
    static void record(NativeInputTiming::Span& span,double ms)noexcept{
        if(ms<0)ms=0;
        ++span.calls;span.totalMs+=ms;if(ms>span.maxMs)span.maxMs=ms;
        ++span.buckets[ms<2?0:ms<8?1:ms<20?2:3];
    }
    Clock clock_;
    NativeInputTiming data_;
    double pollStarted_=0;
    bool pollPending_=false;
};
}
