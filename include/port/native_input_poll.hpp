#pragma once
#include "recoverable_pad_exchange.hpp"
#include "net_diagnostic.h"
namespace partyboard::net {
// Game-thread callback adapter. IO pumps bounded authenticated transport/control
// mailboxes and UI events, captures real paused state and monotonic milliseconds.
// Recovery authorization remains with that coordinator; this never resumes it.
// send(false) means no packet was accepted; retry must not duplicate delivery.
template<class IO>
bool runNativeInputPoll(RecoverablePadExchange& exchange,PADStatus pads[4],PartyBoardInputState initial,IO& io){
    const auto stopped=[&](const char* reason){if constexpr(requires{io.stopped(reason);})io.stopped(reason);return false;};
    try{
        if(!pads||!exchange.originalSessionBound())return stopped("unbound-session-or-null-pads");
        if(exchange.nextFrame()!=initial.frame){exchange.cancel();return stopped("frame-mismatch");}
        if constexpr(requires{io.prepareLocalPads(initial,pads);})io.prepareLocalPads(initial,pads);
        if(!exchange.begin(pads,{initial.checkpoint,initial.frand,initial.rand8},io.now()))return stopped("begin-rejected");
        size_t sent=0;auto epoch=exchange.epoch(),revision=exchange.revision();
        for(;;){
            if(!io.pump()){exchange.cancel();return stopped("io-pump-stopped");}
            const auto now=io.now();const auto phase=exchange.tick(now);
            if(phase==ReconnectPolicy::State::ReturnToMenu)return stopped("recovery-return-to-menu");
            const auto current=io.capture();
            if(current.frame!=initial.frame||current.checkpoint!=initial.checkpoint||current.frand!=initial.frand||current.rand8!=initial.rand8){exchange.cancel();return stopped("paused-state-drift");}
            if(epoch!=exchange.epoch()||revision!=exchange.revision()){epoch=exchange.epoch();revision=exchange.revision();sent=0;}
            if(phase==ReconnectPolicy::State::Paused){
                const auto result=exchange.consumePausedReplay({current.checkpoint,current.frand,current.rand8},pads,now);
                if(result==Exchange::Stopped)return stopped("paused-replay-stopped");
                if(result==Exchange::Ready){
                    if constexpr(requires{io.accepted(initial,pads);})io.accepted(initial,pads);
                    return true;
                }
            }
            if(phase==ReconnectPolicy::State::Running){
                auto packets=exchange.retainedPackets();
                while(sent<packets.size()){if(!io.send(packets[sent]))break;++sent;}
                if(sent==packets.size()){
                    // A channel can close during send before its close callback.
                    // Observe that transport event before committing this poll.
                    if(!io.pump()){exchange.cancel();return stopped("io-pump-stopped-after-send");}
                    if(exchange.tick(io.now())!=ReconnectPolicy::State::Running){io.wait();continue;}
                    const auto afterSend=io.capture();
                    if(afterSend.frame!=initial.frame||afterSend.checkpoint!=initial.checkpoint||afterSend.frand!=initial.frand||afterSend.rand8!=initial.rand8){exchange.cancel();return stopped("state-drift-after-send");}
                    const auto result=exchange.poll({afterSend.checkpoint,afterSend.frand,afterSend.rand8},pads,io.now());
                    if(result==Exchange::Ready){
                        // Observe only the accepted, merged input at this engine
                        // boundary; retries and paused polls are not gameplay.
                        if constexpr(requires{io.accepted(initial,pads);})
                            io.accepted(initial,pads);
                        return true;
                    }
                    if(result==Exchange::Stopped)return stopped("exchange-poll-stopped");
                }
            }
            io.wait();
        }
    }catch(...){exchange.cancel();return stopped("poll-exception");}
}
// Non-owning game-thread binding. The coordinator must keep this object, IO and
// exchange alive until the stopped engine successfully clears its callback.
// Installation remains explicit so destruction cannot silently detach a running
// engine or leave a callback referring to a moved binding.
template<class IO>
class NativeInputPollBinding {
    RecoverablePadExchange& exchange;
    IO& io;
public:
    NativeInputPollBinding(RecoverablePadExchange& value,IO& provider):exchange(value),io(provider){}
    NativeInputPollBinding(const NativeInputPollBinding&)=delete;
    NativeInputPollBinding& operator=(const NativeInputPollBinding&)=delete;
    NativeInputPollBinding(NativeInputPollBinding&&)=delete;
    NativeInputPollBinding& operator=(NativeInputPollBinding&&)=delete;
    bool installOriginalSession(const RecoverySession& identity){
        if(!exchange.originalSessionMatches(identity)||exchange.nextFrame()!=0||
            PartyBoardNativeInstallInputExchange(&sample,this)!=1)return false;
        const auto lanes=exchange.localPadSlots();
        if(PartyBoardNativeSetOwnedGlyphLanes(&sample,this,lanes.data())==1)return true;
        PartyBoardNativeClearOwnedInputExchange(&sample,this);return false;
    }
    bool detachStopped(){return PartyBoardNativeClearOwnedInputExchange(&sample,this)==1;}
    static int sample(void* context,PADStatus pads[4],const PartyBoardInputState* state){
        if(!context||!pads||!state)return 0;
        auto& binding=*static_cast<NativeInputPollBinding*>(context);
        return runNativeInputPoll(binding.exchange,pads,*state,binding.io)?1:0;
    }
};
}

