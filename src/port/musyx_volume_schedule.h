#ifndef PARTYBOARD_MUSYX_VOLUME_SCHEDULE_H
#define PARTYBOARD_MUSYX_VOLUME_SCHEDULE_H
#include <stdint.h>
/* Original host-side 160-frame ramp and residual correction. Reset flags
 * stop a +/-1 delta at the indexed 32-frame period; caller applies them. */
static inline void PartyboardMusyxVolumeSchedule(uint16_t *delta,
        uint16_t *last,uint16_t target) {
    *delta=(uint16_t)(((int16_t)target-(int16_t)*last)/160);
    *last=(uint16_t)(*last+(int16_t)*delta*160);
}
static inline unsigned PartyboardMusyxVolumeResidual(uint16_t *gain,uint16_t *delta,
        uint16_t *last,uint16_t target,uint16_t reset_flags[5],uint16_t mask) {
    int16_t difference=(int16_t)((int16_t)target-(int16_t)*last);
    if(target!=*last) {
        if(difference>=32 && difference<160) {
            unsigned periods=(unsigned)difference/32;
            reset_flags[periods]|=mask;
            *delta=1;*last=(uint16_t)(*last+periods*32);
            return 1;
        }
        if(difference<=-32 && difference>-160) {
            unsigned periods=(unsigned)(-difference)/32;
            reset_flags[periods]|=mask;
            *delta=65535;*last=(uint16_t)(*last-periods*32);
            return 1;
        }
        if(target==0 && difference>-32) *gain=*last=0;
    }
    *delta=0;
    return 0;
}
#endif
