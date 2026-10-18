#ifndef PARTYBOARD_MUSYX_ARRANGEMENT_CONVERT_H
#define PARTYBOARD_MUSYX_ARRANGEMENT_CONVERT_H
#include <stdlib.h>
#include "musyx_arrangement.h"
#include "musyx_arrangement_layout.h"
static inline int PartyboardMusyxArrangementClassify(uint8_t *map,
        const uint8_t *raw, size_t bytes) {
    PartyboardMusyxArrangement arr;
    unsigned track;
    size_t position;
    if(!PartyboardMusyxArrangementRead(&arr,raw,bytes)) return 0;
#define MARK(o,n,w) do { if(!PartyboardMusyxLayoutMark(map,bytes,(o),(n),(w))) return 0; } while(0)
    MARK(0,88,4);MARK(arr.tracks,256,4);MARK(arr.channels,64,1);
    if(arr.info&0x80000000u) MARK(arr.sections,64,1);
    if(arr.master) {
        position=arr.master;
        for(;;) {
            PartyboardMusyxMasterEntry entry;
            if(!PartyboardMusyxMasterEntryRead(&entry,raw,bytes,position)) return 0;
            MARK(position,entry.end ? 4 : 8,4);
            if(entry.end) break;
            position+=8;
        }
    }
    for(track=0;track<64;++track) {
        size_t entries=0;
        if(!arr.track[track]) continue;
        position=arr.track[track];
        for(;;) {
            PartyboardMusyxTrackEntry entry;
            if(!PartyboardMusyxTrackEntryRead(&entry,raw,bytes,position)) return 0;
            ++entries;MARK(position,4,4);MARK(position+4,4,1);MARK(position+8,2,2);
            MARK(position+10,2,entry.pattern==65534 ? 2 : 1);
            if(entry.pattern==65534) {
                if(entry.loop_index>=entries) return 0;
                break;
            }
            if(entry.pattern==65535) break;
            {
                PartyboardMusyxPattern pattern;
                size_t table=(size_t)arr.patterns+entry.pattern*4, note;
                uint32_t pattern_offset;
                unsigned stream_index;
                if(table>bytes || bytes-table<4) return 0;
                MARK(table,4,4);pattern_offset=PartyboardMsmU32(raw+table);
                if(!PartyboardMusyxPatternRead(&pattern,raw,bytes,pattern_offset)) return 0;
                MARK(pattern_offset,12,4);
                note=pattern.notes;
                for(;;) {
                    PartyboardMusyxNote value;
                    if(!PartyboardMusyxNoteRead(&value,raw,bytes,note)) return 0;
                    MARK(note,2,2);MARK(note+2,2,1);
                    if(value.bytes==6) MARK(note+4,2,2);
                    note+=value.bytes;
                    if(value.end) break;
                }
                for(stream_index=0;stream_index<2;++stream_index) {
                    note=stream_index ? pattern.modulation_stream : pattern.pitch_stream;
                    if(!note) continue;
                    for(;;) {
                        PartyboardMusyxStreamEntry value;
                        if(!PartyboardMusyxStreamEntryRead(&value,raw,bytes,note)) return 0;
                        MARK(note,value.bytes,1);note+=value.bytes;
                        if(value.end) break;
                    }
                }
            }
            position+=12;
        }
    }
#undef MARK
    return 1;
}
typedef struct { uint8_t *data; size_t bytes; } PartyboardMusyxOwnedArrangement;
static inline int PartyboardMusyxArrangementConvert(PartyboardMusyxOwnedArrangement *destination,
        const uint8_t *raw,size_t bytes) {
    uint8_t *map,*converted;
    if(!destination || destination->data || !raw || bytes<88 || bytes>64*1024*1024) return 0;
    map=(uint8_t*)calloc(bytes,1);converted=(uint8_t*)malloc(bytes);
    if(!map || !converted) { free(map);free(converted);return 0; }
    if(!PartyboardMusyxArrangementClassify(map,raw,bytes)
            || !PartyboardMusyxLayoutApply(converted,raw,map,bytes)) {
        free(map);free(converted);return 0;
    }
    free(map);destination->data=converted;destination->bytes=bytes;return 1;
}
static inline void PartyboardMusyxArrangementRelease(PartyboardMusyxOwnedArrangement *owned) {
    if(owned) { free(owned->data);owned->data=NULL;owned->bytes=0; }
}
#endif
