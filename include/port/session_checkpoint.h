#ifndef PARTYBOARD_SESSION_CHECKPOINT_H
#define PARTYBOARD_SESSION_CHECKPOINT_H
#include "game/gamework_data.h"

/* Session schema 1. Explicit semantic fields, never structure padding or
 * pointers. Legacy trace-v3 hashing is separate. Dynamic overlay objects and
 * reserved unknown bytes are not covered by this global-state component. */
static inline u32 PartyBoardSessionField(u32 hash, u32 value) {
    int byte;
    for (byte = 0; byte < 4; ++byte) {
        hash = (hash ^ (value & 255)) * 16777619u;
        value >>= 8;
    }
    return hash;
}
/* Optional read-only diagnostic visitor follows the identical semantic schema. */
static inline u32 PartyBoardGlobalStateCheckpointFields(u32 hash,
    const SystemState *system, const PlayerConfig config[4],
    const PlayerState players[4], void (*visit)(void *, const char *, u32), void *context) {
    int i, j;
#define SESSION_FIELD(value) do { const u32 field = (u32)(value); hash = PartyBoardSessionField(hash, field); if (visit) visit(context, #value, field); } while (0)
#define SYSTEM_FIELD(name) SESSION_FIELD(system->name)
    SESSION_FIELD(0x50424701u);
    SYSTEM_FIELD(party); SYSTEM_FIELD(team); SYSTEM_FIELD(diff_story);
    SYSTEM_FIELD(save_mode); SYSTEM_FIELD(mess_speed); SYSTEM_FIELD(mg_list);
    SYSTEM_FIELD(show_com_mg); SYSTEM_FIELD(explain_mg); SYSTEM_FIELD(bonus_star);
    SYSTEM_FIELD(turn); SYSTEM_FIELD(max_turn); SYSTEM_FIELD(star_flag);
    SYSTEM_FIELD(star_total); SYSTEM_FIELD(board); SYSTEM_FIELD(star_pos);
    SYSTEM_FIELD(last5_effect); SYSTEM_FIELD(player_curr); SYSTEM_FIELD(storyCharBit);
    SYSTEM_FIELD(storyChar); SYSTEM_FIELD(block_pos); SYSTEM_FIELD(mess_delay);
    SYSTEM_FIELD(bowser_event); SYSTEM_FIELD(bowser_loss); SYSTEM_FIELD(lucky_value);
    SYSTEM_FIELD(mg_next); SYSTEM_FIELD(mg_type); SYSTEM_FIELD(unk_38);
    for (i = 0; i < 32; ++i) SESSION_FIELD(system->board_data[i]);
    for (i = 0; i < 3; ++i) for (j = 0; j < 16; ++j) SESSION_FIELD(system->flag[i][j]);
    for (i = 0; i < 4; ++i) {
        const PlayerState *player = &players[i];
#define CONFIG_FIELD(name) SESSION_FIELD(config[i].name)
#define PLAYER_FIELD(name) SESSION_FIELD(player->name)
        CONFIG_FIELD(character); CONFIG_FIELD(pad_idx); CONFIG_FIELD(diff);
        CONFIG_FIELD(group); CONFIG_FIELD(iscom);
        PLAYER_FIELD(ticket_player); PLAYER_FIELD(draw_ticket); PLAYER_FIELD(auto_size);
        PLAYER_FIELD(character); PLAYER_FIELD(com); PLAYER_FIELD(diff);
        PLAYER_FIELD(player_idx); PLAYER_FIELD(spark); PLAYER_FIELD(team);
        PLAYER_FIELD(handicap); PLAYER_FIELD(port);
        for (j = 0; j < 3; ++j) SESSION_FIELD(player->items[j]);
        PLAYER_FIELD(team_backup); PLAYER_FIELD(bowser_suit); PLAYER_FIELD(rank);
        PLAYER_FIELD(num_dice); PLAYER_FIELD(size); PLAYER_FIELD(show_next);
        PLAYER_FIELD(jump); PLAYER_FIELD(moving); PLAYER_FIELD(color); PLAYER_FIELD(roll);
        PLAYER_FIELD(space_curr); PLAYER_FIELD(space_prev); PLAYER_FIELD(space_next);
        PLAYER_FIELD(space_shock); PLAYER_FIELD(blue_count); PLAYER_FIELD(red_count);
        PLAYER_FIELD(question_count); PLAYER_FIELD(fortune_count); PLAYER_FIELD(bowser_count);
        PLAYER_FIELD(battle_count); PLAYER_FIELD(mushroom_count); PLAYER_FIELD(warp_count);
        PLAYER_FIELD(coins); PLAYER_FIELD(coins_mg); PLAYER_FIELD(coins_total);
        PLAYER_FIELD(coins_max); PLAYER_FIELD(coins_battle); PLAYER_FIELD(coin_collect);
        PLAYER_FIELD(coin_win); PLAYER_FIELD(stars); PLAYER_FIELD(stars_max);
#undef CONFIG_FIELD
#undef PLAYER_FIELD
    }
#undef SYSTEM_FIELD
#undef SESSION_FIELD
    return hash;
}
static inline u32 PartyBoardGlobalStateCheckpoint(u32 hash,
    const SystemState *system, const PlayerConfig config[4], const PlayerState players[4]) {
    return PartyBoardGlobalStateCheckpointFields(hash, system, config, players, 0, 0);
}
/* Content choices, records and progression participate in session state.
 * Device sound/rumble preferences, creation wall time, padding and unk_00
 * are excluded. This component is not a complete overlay/heap checkpoint. */
static inline u32 PartyBoardGameStatCheckpoint(u32 hash, const GameStat *stat) {
    int i, j;
#define STAT_FIELD(name) hash = PartyBoardSessionField(hash, (u32)stat->name)
    hash = PartyBoardSessionField(hash, 0x50425301u);
    STAT_FIELD(language); STAT_FIELD(total_stars);
    for (i = 0; i < 2; ++i) { STAT_FIELD(mg_custom[i]); STAT_FIELD(mg_avail[i]); }
    for (i = 0; i < 15; ++i) STAT_FIELD(mg_record[i]);
    for (i = 0; i < 9; ++i) {
        for (j = 0; j < 8; ++j) STAT_FIELD(board_win_count[i][j]);
        STAT_FIELD(board_play_count[i]); STAT_FIELD(board_max_stars[i]); STAT_FIELD(board_max_coins[i]);
    }
    for (i = 0; i < 60; ++i) STAT_FIELD(present[i]);
    STAT_FIELD(musicAllF); STAT_FIELD(customPackEnable); STAT_FIELD(veryHardUnlock);
    STAT_FIELD(open_w06); STAT_FIELD(party_continue); STAT_FIELD(story_continue);
#define PAUSE_FIELDS(name) STAT_FIELD(name.save_mode); STAT_FIELD(name.mess_speed); STAT_FIELD(name.mg_list); STAT_FIELD(name.show_com_mg); STAT_FIELD(name.explain_mg)
    PAUSE_FIELDS(story_pause); PAUSE_FIELDS(party_pause);
#undef PAUSE_FIELDS
#undef STAT_FIELD
    return hash;
}
#endif
