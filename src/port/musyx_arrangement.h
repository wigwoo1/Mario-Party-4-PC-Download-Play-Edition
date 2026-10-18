#ifndef PARTYBOARD_MUSYX_ARRANGEMENT_H
#define PARTYBOARD_MUSYX_ARRANGEMENT_H
#include "msm_dsp_sample.h"
typedef struct {
    uint32_t tracks, patterns, channels, master, info, loops[16], sections;
    uint32_t track[64];
} PartyboardMusyxArrangement;
typedef struct {
    uint32_t time;
    unsigned program, velocity, pattern, loop_index;
    int transpose, velocity_add;
} PartyboardMusyxTrackEntry;
typedef struct {
    uint32_t header_length, pitch_stream, modulation_stream;
    size_t notes;
} PartyboardMusyxPattern;
typedef struct {
    unsigned time, key, velocity, length, bytes, end;
} PartyboardMusyxNote;
typedef struct { unsigned time, bytes, end; int value; } PartyboardMusyxStreamEntry;
typedef struct { uint32_t time, bpm; unsigned end; } PartyboardMusyxMasterEntry;
static inline int PartyboardMusyxMasterEntryRead(PartyboardMusyxMasterEntry *destination,
        const uint8_t *raw, size_t bytes, size_t offset) {
    PartyboardMusyxMasterEntry result={0};
    if(!destination || !raw || offset>bytes || bytes-offset<4) return 0;
    result.time=PartyboardMsmU32(raw+offset);
    result.end=result.time==0xffffffffu;
    if(!result.end) {
        if(bytes-offset<8) return 0;
        result.bpm=PartyboardMsmU32(raw+offset+4);
    }
    *destination=result;return 1;
}
static inline int PartyboardMusyxStreamEntryRead(PartyboardMusyxStreamEntry *destination,
        const uint8_t *raw, size_t bytes, size_t offset) {
    PartyboardMusyxStreamEntry result={0};
    unsigned first, value;
    size_t position=offset;
    if(!destination || !raw || offset>bytes || bytes-offset<2) return 0;
    if(raw[offset]==128 && raw[offset+1]==0) {
        result.end=1;result.bytes=2;*destination=result;return 1;
    }
    first=raw[position++];
    result.time=first&127;
    if(first&128) result.time=(result.time<<8)|raw[position++];
    if(position>=bytes) return 0;
    first=raw[position++];value=first&127;
    if(first&128) {
        if(position>=bytes) return 0;
        value=(value<<8)|raw[position++];
        result.value=(value&16384) ? (int)value-32768 : (int)value;
    } else result.value=(value&64) ? (int)value-128 : (int)value;
    result.bytes=(unsigned)(position-offset);*destination=result;return 1;
}
static inline int PartyboardMusyxPatternRead(PartyboardMusyxPattern *destination,
        const uint8_t *raw, size_t bytes, size_t offset) {
    PartyboardMusyxPattern result={0};
    if (!destination || !raw || offset>bytes || bytes-offset<16) return 0;
    result.header_length=PartyboardMsmU32(raw+offset);
    result.pitch_stream=PartyboardMsmU32(raw+offset+4);
    result.modulation_stream=PartyboardMsmU32(raw+offset+8);
    if ((result.pitch_stream && (result.pitch_stream>bytes || bytes-result.pitch_stream<2))
            || (result.modulation_stream && (result.modulation_stream>bytes || bytes-result.modulation_stream<2))) return 0;
    result.notes=offset+12; /* original &pptr->noteData, not a serialized offset */
    *destination=result;return 1;
}
static inline int PartyboardMusyxNoteRead(PartyboardMusyxNote *destination,
        const uint8_t *raw, size_t bytes, size_t offset) {
    PartyboardMusyxNote result={0};
    if (!destination || !raw || offset>bytes || bytes-offset<4) return 0;
    result.time=PartyboardMsmU16(raw+offset);result.key=raw[offset+2];result.velocity=raw[offset+3];
    result.end=result.key==255 && result.velocity==255;
    result.bytes=result.end || (result.key&128) || !(result.key|result.velocity) ? 4 : 6;
    if(bytes-offset<result.bytes) return 0;
    if(result.bytes==6) result.length=PartyboardMsmU16(raw+offset+4);
    *destination=result;return 1;
}
static inline int PartyboardMusyxTrackEntryRead(PartyboardMusyxTrackEntry *destination,
        const uint8_t *raw, size_t bytes, size_t offset) {
    PartyboardMusyxTrackEntry result = {0};
    const uint8_t *entry;
    if (!destination || !raw || offset > bytes || bytes-offset < 12) return 0;
    entry=raw+offset;
    result.time=PartyboardMsmU32(entry);
    result.program=entry[4];result.velocity=entry[5];
    result.pattern=PartyboardMsmU16(entry+8);
    if (result.pattern==65534) result.loop_index=PartyboardMsmU16(entry+10);
    else {
        result.transpose=entry[10]<128 ? entry[10] : (int)entry[10]-256;
        result.velocity_add=entry[11]<128 ? entry[11] : (int)entry[11]-256;
    }
    *destination=result;return 1;
}
static inline int PartyboardMusyxArrangementRead(PartyboardMusyxArrangement *destination,
        const uint8_t *raw, size_t bytes) {
    PartyboardMusyxArrangement result = {0};
    unsigned i;
    if (!destination || !raw || bytes < 88) return 0;
    result.tracks=PartyboardMsmU32(raw); result.patterns=PartyboardMsmU32(raw+4);
    result.channels=PartyboardMsmU32(raw+8); result.master=PartyboardMsmU32(raw+12);
    result.info=PartyboardMsmU32(raw+16); result.sections=PartyboardMsmU32(raw+84);
    for (i=0;i<16;++i) result.loops[i]=PartyboardMsmU32(raw+20+i*4);
    if (result.tracks > bytes || bytes-result.tracks < 256
            || result.channels > bytes || bytes-result.channels < 64
            || result.patterns > bytes || bytes-result.patterns < 4
            || (result.master && (result.master > bytes || bytes-result.master < 4))) return 0;
    if ((result.info & 0x80000000u)
            && (result.sections > bytes || bytes-result.sections < 64)) return 0;
    for (i=0;i<64;++i) {
        result.track[i]=PartyboardMsmU32(raw+result.tracks+i*4);
        if (result.track[i] && (result.track[i] > bytes || bytes-result.track[i] < 12)) return 0;
        if (result.track[i] && raw[result.channels+i] >= 16) return 0;
        if (result.track[i] && (result.info & 0x80000000u) && raw[result.sections+i] >= 16) return 0;
    }
    *destination=result;
    return 1;
}
#endif
