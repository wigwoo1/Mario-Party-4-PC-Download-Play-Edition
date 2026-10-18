#include "game/gamework_data.h"
#include <assert.h>
#include <stdio.h>
SystemState GWSystem;
int main(void) {
    for(int requested=-16;requested<32;++requested) {
        GWSystem.board=2;
        GWSaveModeSet(requested);
        assert(GWSystem.save_mode==1 && GWSaveModeGet()==1);
        assert(GWSystem.board==2);
    }
    for(unsigned restored=0;restored<4;++restored) {
        GWSystem.save_mode=(u16)restored;
        assert(GWSaveModeGet()==1 && GWSystem.save_mode==1);
    }
    puts("Native/browser no-save mode survives setters and restored values PASS");
    return 0;
}
