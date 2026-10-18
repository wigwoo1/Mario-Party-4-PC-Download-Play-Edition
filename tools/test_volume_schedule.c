#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "musyx_volume_schedule.h"
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
#include "original-volume-schedule-reference.h"
int main(void) {
    unsigned target,last,count=0;
    for(target=0;target<32768;++target) for(last=0;last<32768;last+=197) {
        u16 a=(u16)last,b=a,da=0x1234,db=da;
        u16 ga=a,gb=b,fa[5]={1,2,4,8,16},fb[5];
        unsigned ra,rb;
        PartyboardMusyxVolumeSchedule(&da,&a,(u16)target);
        sal_setup_dspvol(&db,&b,(u16)target);
        assert(a==b && da==db);
        memcpy(fb,fa,sizeof fa);
        ra=PartyboardMusyxVolumeResidual(&ga,&da,&a,(u16)target,fa,0x400);
        rb=salCheckVolErrorAndResetDelta(&gb,&db,&b,(u16)target,fb,0x400);
        assert(ra==rb && ga==gb && da==db && a==b && !memcmp(fa,fb,sizeof fa));
        ++count;
    }
    printf("%u original gain schedule/residual comparisons PASS\n",count);
    return 0;
}
