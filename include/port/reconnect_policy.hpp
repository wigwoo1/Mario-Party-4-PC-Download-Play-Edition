#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace partyboard::net {
// Mirrors web/reconnect-policy.mjs. Only the session coordinator may mark an
// authenticated, reconciled replacement recovered and authorize an all-player
// resume. This policy never authenticates traffic or advances simulation.
class ReconnectPolicy {
public:
    enum class State { Running, Paused, ReturnToMenu };
    explicit ReconnectPolicy(std::array<uint32_t,4> owners):owners_(owners) {
        bool active=false;for(auto owner:owners)active|=owner!=0;
        if(!active)throw std::invalid_argument("Active owners required");
    }
    State state() const { return state_; }
    uint64_t epoch() const { return epoch_; }
    uint64_t revision() const { return revision_; }
    const std::array<bool,4>& missingSlots() const { return missing_; }
    State update(uint64_t now) {
        if(state_==State::ReturnToMenu)return state_;
        // Match the browser's exact integer clock domain; reject clock reversal.
        if(now>9007199254740991ULL || (hasLast_&&now<last_))return state_=State::ReturnToMenu;
        last_=now;hasLast_=true;
        if(state_==State::Paused&&now-started_>=60000)state_=State::ReturnToMenu;
        return state_;
    }
    bool disconnected(uint32_t peer,uint64_t now) {
        return disconnected(peer,now,now);
    }
    // A transport can detect loss long after the current input poll blocked.
    // Charge that local wait to the same sixty-second interruption budget.
    // observed time still advances monotonically; the anchor is not a clock update.
    bool disconnected(uint32_t peer,uint64_t now,uint64_t interruptedAt) {
        if(update(now)==State::ReturnToMenu||!owns(peer))return false;
        if(interruptedAt>now){cancel();return false;}
        if(revision_==std::numeric_limits<uint64_t>::max()||
           (state_==State::Running&&epoch_==std::numeric_limits<uint64_t>::max())) {
            cancel();return false;
        }
        ++revision_;
        if(state_==State::Running){state_=State::Paused;started_=interruptedAt;++epoch_;}
        for(unsigned i=0;i<4;++i)if(owners_[i]==peer)missing_[i]=true;
        return update(now)!=State::ReturnToMenu;
    }
    bool markRecovered(uint32_t peer,uint64_t epoch,uint64_t revision,uint64_t now) {
        if(update(now)!=State::Paused||epoch!=epoch_||revision!=revision_||!owns(peer))return false;
        for(unsigned i=0;i<4;++i)if(owners_[i]==peer)missing_[i]=false;
        return true;
    }
    bool readyToResume(uint64_t now) {
        if(update(now)!=State::Paused)return false;
        for(auto missing:missing_)if(missing)return false;
        return true;
    }
    bool resume(uint64_t epoch,uint64_t revision,uint64_t now) {
        if(!readyToResume(now)||epoch!=epoch_||revision!=revision_)return false;
        state_=State::Running;return true;
    }
    uint64_t remainingMs(uint64_t now) { return update(now)==State::Paused?60000-(now-started_):0; }
    void cancel() { state_=State::ReturnToMenu; }
private:
    bool owns(uint32_t peer) const { if(peer)for(auto owner:owners_)if(owner==peer)return true;return false; }
    std::array<uint32_t,4> owners_;
    std::array<bool,4> missing_{};
    State state_{State::Running};
    uint64_t last_{},started_{},epoch_{},revision_{};
    bool hasLast_{};
};
}
