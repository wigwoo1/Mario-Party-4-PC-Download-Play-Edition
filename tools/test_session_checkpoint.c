#include "port/session_checkpoint.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    SystemState system = {0};
    PlayerConfig config[4] = {{0}};
    PlayerState players[4] = {{0}};
    u32 base = PartyBoardGlobalStateCheckpoint(2166136261u, &system, config, players);
#define CHECK_CHANGE(change, restore) do { change; assert(base != PartyBoardGlobalStateCheckpoint(2166136261u, &system, config, players)); restore; assert(base == PartyBoardGlobalStateCheckpoint(2166136261u, &system, config, players)); } while (0)
    CHECK_CHANGE(system.star_pos = 3, system.star_pos = 0);
    CHECK_CHANGE(system.board_data[31] = 7, system.board_data[31] = 0);
    CHECK_CHANGE(system.flag[2][15] = 9, system.flag[2][15] = 0);
    CHECK_CHANGE(system.unk_38 = 4, system.unk_38 = 0);
    CHECK_CHANGE(config[3].group = 2, config[3].group = 0);
    CHECK_CHANGE(players[3].coins_mg = 5, players[3].coins_mg = 0);
    CHECK_CHANGE(players[2].space_prev = -1, players[2].space_prev = 0);
    CHECK_CHANGE(players[1].moving = 1, players[1].moving = 0);
    CHECK_CHANGE(players[0].ticket_player = 17, players[0].ticket_player = 0);
    system.PC_PADDING_00 = 63; system.PC_PADDING_02 = 127;
    players[0].PC_PADDING_02 = 15; players[0].PC_PADDING_08 = 7;
    assert(base == PartyBoardGlobalStateCheckpoint(2166136261u, &system, config, players));
    GameStat stat = {0};
    u32 statsBase = PartyBoardGameStatCheckpoint(base, &stat);
#define STAT_CHANGE(field) do { stat.field = 1; assert(statsBase != PartyBoardGameStatCheckpoint(base, &stat)); stat.field = 0; assert(statsBase == PartyBoardGameStatCheckpoint(base, &stat)); } while (0)
    for (int i = 0; i < 2; i++) { STAT_CHANGE(mg_custom[i]); STAT_CHANGE(mg_avail[i]); }
    for (int i = 0; i < 15; i++) STAT_CHANGE(mg_record[i]);
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 8; j++) STAT_CHANGE(board_win_count[i][j]);
        STAT_CHANGE(board_play_count[i]); STAT_CHANGE(board_max_stars[i]); STAT_CHANGE(board_max_coins[i]);
    }
    for (int i = 0; i < 60; i++) STAT_CHANGE(present[i]);
    STAT_CHANGE(language); STAT_CHANGE(total_stars); STAT_CHANGE(musicAllF); STAT_CHANGE(customPackEnable);
    STAT_CHANGE(veryHardUnlock); STAT_CHANGE(open_w06); STAT_CHANGE(party_continue); STAT_CHANGE(story_continue);
    STAT_CHANGE(story_pause.save_mode); STAT_CHANGE(story_pause.mess_speed); STAT_CHANGE(story_pause.mg_list);
    STAT_CHANGE(story_pause.show_com_mg); STAT_CHANGE(story_pause.explain_mg);
    STAT_CHANGE(party_pause.save_mode); STAT_CHANGE(party_pause.mess_speed); STAT_CHANGE(party_pause.mg_list);
    STAT_CHANGE(party_pause.show_com_mg); STAT_CHANGE(party_pause.explain_mg);
    stat.sound_mode = 2; stat.rumble = 1; stat.create_time = 123456; stat.PC_PADDING_10E = 3;
    assert(statsBase == PartyBoardGameStatCheckpoint(base, &stat));
#undef STAT_CHANGE
    system.board = 2; system.turn = 7; players[3].space_prev = -1; players[2].coins = 123;
    stat.mg_record[14] = 1800; stat.board_win_count[8][7] = 3; stat.present[59] = 1;
    printf("Cross-platform semantic fixture: %08x\n", PartyBoardGameStatCheckpoint(
        PartyBoardGlobalStateCheckpoint(2166136261u, &system, config, players), &stat));
    puts("PASS semantic session checkpoint: global fields and GameStat content/progression detected; device preferences, creation time and padding excluded.");
    return 0;
}
