#include "game/saveload.h"
#include "game/card.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifndef PARTYBOARD_NATIVE_GAME_LIBRARY
GameStat GWGameStat;
SystemState GWSystem;
PlayerState GWPlayer[4];
PlayerConfig GWPlayerCfg[4];
extern s32 SaveEnableF;
#define REQUEST_SAVE() (SaveEnableF=1)
#define CHECK_DISABLED() assert(!SaveEnableF)
#else
#define REQUEST_SAVE() SLSaveFlagSet(1)
#define CHECK_DISABLED() assert(!SLSaveFlagGet())
#endif
int main(void) {
    REQUEST_SAVE();GWGameStat.party_continue=1;GWGameStat.story_continue=1;
    assert(!SLSaveFlagGet());
    CHECK_DISABLED();
    assert(!GWGameStat.party_continue && !GWGameStat.story_continue);
    for(int slot=-1;slot<=2;++slot) {
        REQUEST_SAVE();
        assert(SLFormat((s16)slot)==CARD_RESULT_READY);CHECK_DISABLED();
    }
    REQUEST_SAVE();
    assert(SLSave()==CARD_RESULT_READY);CHECK_DISABLED();
#ifdef PARTYBOARD_NATIVE_GAME_LIBRARY
    /* Restoring an old save must not overwrite a live session either. */
    SystemState expected_system=GWSystem;
    PlayerState expected_players[4];
    PlayerConfig expected_config[4];
    GameStat expected_stat=GWGameStat;
    memcpy(expected_players,GWPlayer,sizeof(GWPlayer));
    memcpy(expected_config,GWPlayerCfg,sizeof(GWPlayerCfg));
    SLLoadGameStat();SLLoadBoard();SLLoadBoardStory();
    assert(!memcmp(&expected_stat,&GWGameStat,sizeof(GWGameStat)));
    assert(!memcmp(&expected_system,&GWSystem,sizeof(GWSystem)));
    assert(!memcmp(expected_players,GWPlayer,sizeof(GWPlayer)));
    assert(!memcmp(expected_config,GWPlayerCfg,sizeof(GWPlayerCfg)));
    CHECK_DISABLED();
#endif
#ifndef PARTYBOARD_NATIVE_GAME_LIBRARY
    HuCardInit();
    assert(HuCardCheck()==0);
    for (int slot=-1;slot<=2;++slot) {
        assert(HuCardSlotCheck((s16)slot)==CARD_RESULT_NOCARD);
        assert(HuCardMount((s16)slot)==CARD_RESULT_NOCARD);
        assert(HuCardFormat((s16)slot)==CARD_RESULT_NOCARD);
        CARDFileInfo info,expected_info;
        unsigned char data[32],expected_data[32];
        u32 free_bytes=123,free_files=456;
        memset(&info,0x37,sizeof(info));expected_info=info;
        memset(data,0x48,sizeof(data));memcpy(expected_data,data,sizeof(data));
        assert(HuCardOpen((s16)slot,"unused",&info)==CARD_RESULT_NOCARD);
        assert(HuCardCreate((s16)slot,"unused",8192,&info)==CARD_RESULT_NOCARD);
        assert(HuCardRead(&info,data,sizeof(data),0)==CARD_RESULT_NOCARD);
        assert(HuCardWrite(&info,data,sizeof(data),0)==CARD_RESULT_NOCARD);
        assert(HuCardClose(&info)==CARD_RESULT_NOCARD);
        assert(HuCardDelete((s16)slot,"unused")==CARD_RESULT_NOCARD);
        assert(HuCardSectorSizeGet((s16)slot)==CARD_RESULT_NOCARD);
        assert(HuCardFreeSpaceGet((s16)slot,&free_bytes,&free_files)==CARD_RESULT_NOCARD);
        HuCardUnMount((s16)slot);
        assert(!memcmp(&info,&expected_info,sizeof(info)));
        assert(!memcmp(data,expected_data,sizeof(data)));
        assert(free_bytes==123 && free_files==456);
        REQUEST_SAVE();
        assert(SLCardMount((s16)slot)==CARD_RESULT_NOCARD);CHECK_DISABLED();
    }
    for (int message=0;message<=12;++message) {
        REQUEST_SAVE();
        assert(SLMessOut((s16)message)==-1);CHECK_DISABLED();
    }
    REQUEST_SAVE();
    assert(SLLoad()==CARD_RESULT_READY);CHECK_DISABLED();
    GWGameStat.open_w06=1;
    GWGameStat.mg_avail[0]=0xffffffff;
    memset(&GWSystem,0x35,sizeof(GWSystem));
    memset(GWPlayer,0x46,sizeof(GWPlayer));
    memset(GWPlayerCfg,0x57,sizeof(GWPlayerCfg));
    SystemState expected_system=GWSystem;
    PlayerState expected_players[4];
    PlayerConfig expected_config[4];
    memcpy(expected_players,GWPlayer,sizeof(GWPlayer));
    memcpy(expected_config,GWPlayerCfg,sizeof(GWPlayerCfg));
    SLLoadGameStat();SLLoadBoard();SLLoadBoardStory();
    assert(GWGameStat.open_w06==1 && GWGameStat.mg_avail[0]==0xffffffff);
    assert(!memcmp(&expected_system,&GWSystem,sizeof(GWSystem)));
    assert(!memcmp(expected_players,GWPlayer,sizeof(GWPlayer)));
    assert(!memcmp(expected_config,GWPlayerCfg,sizeof(GWPlayerCfg)));
    CHECK_DISABLED();
#endif
    puts("Actual save and format entry points retain disabled saving PASS");
    return 0;
}
