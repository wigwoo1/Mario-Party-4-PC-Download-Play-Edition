#include "../include/port/reconnect_policy.hpp"
#include <cstdio>
using namespace partyboard::net;
int main() {
    ReconnectPolicy policy({11,22,22,33});
    const auto emit = [&](bool accepted, uint64_t now) {
        const auto remaining = policy.remainingMs(now);
        unsigned missing = 0;
        for (unsigned i=0;i<4;++i) if(policy.missingSlots()[i]) missing |= 1u<<i;
        printf("%u %u %llu %llu %llu %u\n",unsigned(accepted),unsigned(policy.state()),
               (unsigned long long)policy.epoch(),(unsigned long long)policy.revision(),
               (unsigned long long)remaining,missing);
    };
    emit(policy.disconnected(22,100),100);
    emit(policy.disconnected(33,1000),1000);
    emit(policy.markRecovered(22,1,1,1100),1100);
    emit(policy.markRecovered(22,1,2,1200),1200);
    emit(policy.markRecovered(33,1,2,1300),1300);
    emit(policy.resume(1,1,1400),1400);
    emit(policy.disconnected(22,1500),1500);
    emit(policy.markRecovered(22,1,2,1600),1600);
    emit(policy.markRecovered(22,1,3,60099),60099);
    emit(policy.resume(1,3,60099),60099);
    emit(policy.disconnected(11,61000),61000);
    emit(policy.markRecovered(11,2,4,121000),121000);
    emit(policy.resume(2,4,121001),121001);
}
