#ifndef PARTYBOARD_PDT_PLAYER_H
#define PARTYBOARD_PDT_PLAYER_H
#include "pdt_stream.h"

typedef struct {
    PartyboardPdtStream source;
    unsigned remainder, fade_frames, fade_remaining;
    int active, failed;
} PartyboardPdtPlayer;

static inline int PartyboardPdtPlayerStart(PartyboardPdtPlayer *player,
        const uint8_t *container, size_t bytes, unsigned id) {
    PartyboardPdtPlayer result = {0};
    if (!PartyboardPdtStreamOpen(&result.source, container, bytes, id)) return 0;
    result.active = 1;
    *player = result;
    return 1;
}

static inline void PartyboardPdtPlayerFade(PartyboardPdtPlayer *player, unsigned frames) {
    if (!frames) player->active = 0;
    player->fade_frames = player->fade_remaining = frames;
}

/* Called exactly once per simulation tick. Device queue must not determine
 * game-visible completion. Output may be discarded when a device stalls. */
static inline size_t PartyboardPdtPlayerTick(PartyboardPdtPlayer *player,
        unsigned ticks_per_second, int16_t *pcm, size_t capacity_frames) {
    unsigned wanted, i;
    size_t produced;
    if (!player->active || !ticks_per_second) return 0;
    wanted = (player->remainder + player->source.frequency) / ticks_per_second;
    if (wanted > capacity_frames) return 0;
    player->remainder = (player->remainder + player->source.frequency) % ticks_per_second;
    produced = PartyboardPdtStreamReadStereo(&player->source, pcm, wanted);
    for (i = 0; i < produced * 2; ++i) {
        int64_t scaled = (int64_t)pcm[i] * player->source.volume;
        scaled /= 127;
        if (player->fade_frames) scaled = scaled * player->fade_remaining / player->fade_frames;
        pcm[i] = (int16_t)scaled;
    }
    if (produced < wanted) {
        player->active = 0;
        player->failed = player->source.channels[0].failed || player->source.channels[1].failed;
    }
    if (!player->source.channels[0].looping
        && player->source.channels[0].position == player->source.channels[0].bytes
        && player->source.channels[0].pending_position == 14) player->active = 0;
    if (player->fade_remaining && !--player->fade_remaining) player->active = 0;
    return produced;
}
#endif
