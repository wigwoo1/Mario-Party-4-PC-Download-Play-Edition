#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#include "../src/port/msm_container.h"
#include "../src/port/musyx_arrangement.h"
#include "../src/port/musyx_arrangement_convert.h"
int main(void) {
    uint8_t header[4], *raw;
    size_t bytes, count, i;
    PartyboardMsmContainer container;
    PartyboardMsmContainerInfo info;
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    if (fread(header, 1, 4, stdin) != 4) return 2;
    bytes = PartyboardMsmU32(header);
    if (bytes < 96 || bytes > 64 * 1024 * 1024) return 2;
    raw = malloc(bytes);
    if (!raw) return 3;
    if (fread(raw, 1, bytes, stdin) != bytes) return 4;
    if (!PartyboardMsmContainerOpen(&container, raw, bytes)) return 5;
    for (i = 0; i < 10; ++i)
        printf("%u %u\n", container.section[i].offset, container.section[i].bytes);
    count = container.section[PARTYBOARD_MSM_GROUPS].bytes / 32;
    printf("%zu\n", count);
    for (i = 0; i < count; ++i) {
        PartyboardMsmContainerGroup group;
        if (!PartyboardMsmContainerGetGroup(&group, &container, i)) return 6;
        printf("%u %d %d %zu %zu %zu %zu\n", group.id, group.stack, group.subgroup,
            (size_t)(group.data - raw), group.data_bytes,
            (size_t)(group.samples - raw), group.sample_bytes);
    }
    if (!PartyboardMsmContainerGetInfo(&info, &container)) return 7;
    printf("%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u\n",
        info.voices, info.music, info.effects, info.groups, info.songs, info.sound_effects,
        info.music_channels, info.stack_a, info.stack_b, info.surround,
        info.minimum_memory, info.sample_memory, info.group_buffer_a, info.group_buffer_b,
        info.dummy_music_bytes, info.base_count);
    for (i = 0; i < info.base_count; ++i) printf("%u\n", info.base_groups[i]);
    printf("%d %d\n", info.aux_a, info.aux_b);
    count = container.section[PARTYBOARD_MSM_AUX].bytes / 40;
    printf("%zu\n", count);
    for (i = 0; i < count; ++i) {
        PartyboardMsmContainerAux aux;
        unsigned j;
        if (!PartyboardMsmContainerGetAux(&aux, &container, i)) return 8;
        printf("%u", aux.type);
        for (j = 0; j < 9; ++j) printf(" %u", aux.parameter[j]);
        printf("\n");
    }
    count = container.section[PARTYBOARD_MSM_EFFECTS].bytes / 16;
    printf("%zu\n", count);
    for (i = 0; i < count; ++i) {
        PartyboardMsmContainerEffect effect;
        if (!PartyboardMsmContainerGetEffect(&effect, &container, i)) return 9;
        printf("%u %u %d %d %d %u %u %u %d %d\n",
            effect.group, effect.effect, effect.volume, effect.pan, effect.pitch_bend,
            effect.span, effect.reverb, effect.chorus, effect.emitter, effect.emitter_component);
    }
    count = container.section[PARTYBOARD_MSM_SONGS].bytes / 16;
    printf("%zu\n", count);
    for (i = 0; i < count; ++i) {
        PartyboardMsmContainerSong song;
        if (!PartyboardMsmContainerGetSong(&song, &container, i)) return 10;
        printf("%u %u %u %u %d %d\n", song.group, song.song,
            song.offset, song.bytes, song.song_group, song.volume);
        {
            const uint8_t *sequence = NULL;
            size_t sequence_bytes = 0;
            int resolved = PartyboardMsmContainerResolveSong(&sequence, &sequence_bytes, &container, &song);
            printf("%d %zu %zu\n", resolved, resolved ? (size_t)(sequence - raw) : 0, sequence_bytes);
            if (resolved) {
                PartyboardMusyxArrangement arrangement;
                unsigned j;
                if (!PartyboardMusyxArrangementRead(&arrangement, sequence, sequence_bytes)) {
                    unsigned k, invalid_channels=0;
                    uint32_t channel_offset=PartyboardMsmU32(sequence+8);
                    if (channel_offset <= sequence_bytes && sequence_bytes-channel_offset >=64)
                        for(k=0;k<64;++k) if(sequence[channel_offset+k]>=16) ++invalid_channels;
                    fprintf(stderr,"Header extent %zu channels outside16: %u\n",sequence_bytes,invalid_channels);
                    fprintf(stderr, "Invalid arrangement header at song %zu\n", i); return 11;
                }
                {
                    PartyboardMusyxOwnedArrangement owned={0};
                    uint32_t value;
                    if(!PartyboardMusyxArrangementConvert(&owned,sequence,sequence_bytes)) {
                        fprintf(stderr,"Arrangement conversion rejected song %zu\n",i);return 19;
                    }
                    memcpy(&value,owned.data+16,4);
                    if(value!=arrangement.info) return 20;
                    for(j=0;j<64;++j) {
                        memcpy(&value,owned.data+arrangement.tracks+j*4,4);
                        if(value!=arrangement.track[j]) return 20;
                    }
                    {
                        size_t byte;
                        for(byte=0;byte<owned.bytes;++byte) printf("%u\n",owned.data[byte]);
                    }
                    if(PartyboardMusyxArrangementConvert(&owned,sequence,sequence_bytes)) return 21;
                    PartyboardMusyxArrangementRelease(&owned);PartyboardMusyxArrangementRelease(&owned);
                }
                printf("%u %u %u %u %u %u\n", arrangement.tracks, arrangement.patterns,
                    arrangement.channels, arrangement.master, arrangement.info, arrangement.sections);
                for (j=0;j<16;++j) printf("%u\n", arrangement.loops[j]);
                for (j=0;j<64;++j) printf("%u\n", arrangement.track[j]);
                {
                    size_t master_count=0, position=arrangement.master, m;
                    if(position) for(;;) {
                        PartyboardMusyxMasterEntry master;
                        if(!PartyboardMusyxMasterEntryRead(&master,sequence,sequence_bytes,position)) return 18;
                        ++master_count;position+=8;
                        if(master.end) break;
                    }
                    printf("%zu\n",master_count);
                    position=arrangement.master;
                    for(m=0;m<master_count;++m) {
                        PartyboardMusyxMasterEntry master;
                        if(!PartyboardMusyxMasterEntryRead(&master,sequence,sequence_bytes,position)) return 18;
                        printf("%u %u %u\n",master.time,master.bpm,master.end);position+=8;
                    }
                }
                for (j=0;j<64;++j) {
                    size_t position=arrangement.track[j], entries=0, k;
                    if (!position) { printf("0\n"); continue; }
                    for (;;) {
                        PartyboardMusyxTrackEntry entry;
                        if (!PartyboardMusyxTrackEntryRead(&entry,sequence,sequence_bytes,position+entries*12)) return 12;
                        ++entries;
                        if (entry.pattern==65535 || entry.pattern==65534) break;
                    }
                    printf("%zu\n",entries);
                    for(k=0;k<entries;++k) {
                        PartyboardMusyxTrackEntry entry;
                        if (!PartyboardMusyxTrackEntryRead(&entry,sequence,sequence_bytes,position+k*12)) return 12;
                        if(entry.pattern==65534 && entry.loop_index>=entries) return 13;
                        printf("%u %u %u %u %u %d %d\n",entry.time,entry.program,entry.velocity,
                            entry.pattern,entry.loop_index,entry.transpose,entry.velocity_add);
                        if(entry.pattern<65534) {
                            PartyboardMusyxPattern pattern;
                            size_t table=(size_t)arrangement.patterns+entry.pattern*4, note_position, note_count=0, n;
                            uint32_t pattern_offset;
                            if(table>sequence_bytes || sequence_bytes-table<4) return 14;
                            pattern_offset=PartyboardMsmU32(sequence+table);
                            if(!PartyboardMusyxPatternRead(&pattern,sequence,sequence_bytes,pattern_offset)) return 15;
                            note_position=pattern.notes;
                            for(;;) {
                                PartyboardMusyxNote note;
                                if(!PartyboardMusyxNoteRead(&note,sequence,sequence_bytes,note_position)) return 16;
                                ++note_count;note_position+=note.bytes;
                                if(note.end) break;
                            }
                            printf("%u %u %u %u %zu\n",pattern_offset,pattern.header_length,pattern.pitch_stream,pattern.modulation_stream,note_count);
                            note_position=pattern.notes;
                            for(n=0;n<note_count;++n) {
                                PartyboardMusyxNote note;
                                if(!PartyboardMusyxNoteRead(&note,sequence,sequence_bytes,note_position)) return 16;
                                printf("%u %u %u %u %u %u\n",note.time,note.key,note.velocity,note.length,note.bytes,note.end);
                                note_position+=note.bytes;
                            }
                            for(n=0;n<2;++n) {
                                size_t stream_position=n ? pattern.modulation_stream : pattern.pitch_stream, stream_count=0, s;
                                if(!stream_position) { printf("0\n");continue; }
                                note_position=stream_position;
                                for(;;) {
                                    PartyboardMusyxStreamEntry stream;
                                    if(!PartyboardMusyxStreamEntryRead(&stream,sequence,sequence_bytes,note_position)) return 17;
                                    ++stream_count;note_position+=stream.bytes;
                                    if(stream.end) break;
                                }
                                printf("%zu\n",stream_count);
                                for(s=0;s<stream_count;++s) {
                                    PartyboardMusyxStreamEntry stream;
                                    if(!PartyboardMusyxStreamEntryRead(&stream,sequence,sequence_bytes,stream_position)) return 17;
                                    printf("%u %d %u %u\n",stream.time,stream.value,stream.bytes,stream.end);
                                    stream_position+=stream.bytes;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    free(raw);
    return 0;
}
