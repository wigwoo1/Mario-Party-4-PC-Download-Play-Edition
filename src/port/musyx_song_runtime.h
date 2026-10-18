#ifndef PARTYBOARD_MUSYX_SONG_RUNTIME_H
#define PARTYBOARD_MUSYX_SONG_RUNTIME_H
#include "musyx/seq.h"
#include "musyx_arrangement_convert.h"
#include "msm_container.h"
typedef struct {
    PartyboardMusyxOwnedArrangement arrangement;
    uint32_t sequence;
} PartyboardMusyxSong;
/* Caller must load original synthesis dependencies before starting and retain
 * them until stop. irq_call matches original seqPlaySong context. */
static inline int PartyboardMusyxSongStart(PartyboardMusyxSong *destination,
        const uint8_t *raw,size_t bytes,uint16_t group,uint16_t song,
        SND_PLAYPARA *parameters,uint8_t irq_call,uint8_t studio) {
    PartyboardMusyxSong result={0};
    if(!destination || destination->arrangement.data) return 0;
    if(!PartyboardMusyxArrangementConvert(&result.arrangement,raw,bytes)) return 0;
    result.sequence=seqPlaySong(group,song,result.arrangement.data,parameters,irq_call,studio);
    if(result.sequence==0xffffffffu) {
        PartyboardMusyxArrangementRelease(&result.arrangement);return 0;
    }
    *destination=result;return 1;
}
static inline int PartyboardMusyxSongStop(PartyboardMusyxSong *song) {
    if(!song || !song->arrangement.data) return 0;
    seqStop(song->sequence);
    PartyboardMusyxArrangementRelease(&song->arrangement);
    song->sequence=0xffffffffu;return 1;
}
/* MSM default playback; callers supply the actual currently loaded group
 * indices. Original synthesis group eligibility remains checked by seqPlaySong. */
static inline int PartyboardMusyxSongStartContainer(PartyboardMusyxSong *destination,
        const PartyboardMsmContainer *container,size_t song_index,
        const unsigned *loaded,size_t loaded_count,uint8_t irq_call,uint8_t studio) {
    PartyboardMsmContainerSong descriptor;
    const uint8_t *sequence;
    size_t bytes,i;
    SND_PLAYPARA parameters={0};
    if(!destination || (!loaded && loaded_count)
            || !PartyboardMsmContainerGetSong(&descriptor,container,song_index)
            || descriptor.volume<0 || descriptor.volume>127) return 0;
    if(descriptor.song_group>=0) {
        for(i=0;i<loaded_count && loaded[i]!=(unsigned)descriptor.song_group;++i) {}
        if(i==loaded_count) return 0;
    }
    if(!PartyboardMsmContainerResolveSong(&sequence,&bytes,container,&descriptor)) return 0;
    parameters.flags=SND_PLAYPARA_VOLUME;
    parameters.volume.target=(u8)descriptor.volume;
    return PartyboardMusyxSongStart(destination,sequence,bytes,(uint16_t)descriptor.group,
        (uint16_t)descriptor.song,&parameters,irq_call,studio);
}
#endif
