#include "game/gamework.h"
#include "game/gamework_data.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    setvbuf(stdout,NULL,_IONBF,0);
    puts("Production profile test entered");
    /* Exercise the production reset repeatedly after a locked/continued profile. */
    for (int reset=0; reset<2; ++reset) {
        memset(&GWGameStat,0,sizeof(GWGameStat));
        GWGameStat.party_continue=GWGameStat.story_continue=1;
        GWSystem.save_mode=0;
        GWGameStatReset();
        puts("Production profile reset returned");
        for (unsigned id=0; id<64; ++id) assert(GWGameStat.mg_avail[id/32] & (1u<<(id%32)));
        for (unsigned i=0; i<sizeof(GWGameStat.present); ++i) assert(GWGameStat.present[i]==1);
        assert(GWGameStat.open_w06 && GWGameStat.veryHardUnlock && GWGameStat.musicAllF);
        assert(!GWGameStat.party_continue && !GWGameStat.story_continue);
        assert(GWSaveModeGet()==1);
        for (int requested=0; requested<4; ++requested) {
            GWSaveModeSet(requested);
            assert(GWSaveModeGet()==1);
        }
    }
    puts("Actual production reset unlocks every minigame, present, bonus board, difficulty and music; saves remain disabled PASS");
    return 0;
}
