#ifndef PARTYBOARD_MUSYX_DEPOP_H
#define PARTYBOARD_MUSYX_DEPOP_H
#include <stdint.h>

/* Original hw_dspctrl AddDpop/DoDepopFade host arithmetic. This does not
 * generate the DSP's per-voice residual or apply the studio output ramp. */
static inline int32_t PartyboardMusyxDepopAdd(int32_t sum,int16_t residual) {
    int64_t value=(int64_t)sum+residual;
    if(value>0x7fffff) value=0x7fffff;
    if(value< -0x7fffff) value= -0x7fffff;
    return (int32_t)value;
}
static inline int PartyboardMusyxDepopFade(int32_t *sum,int32_t *start,int16_t *delta) {
    int32_t step;
    if(!sum || !start || !delta || sum==start || *sum>0x7fffff || *sum< -0x7fffff) return 0;
    step= -(*sum/160);
    if(step>20) step=20;
    if(step< -20) step= -20;
    *start=*sum;
    *delta=(int16_t)step;
    *sum+=step*160;
    return 1;
}
#endif
