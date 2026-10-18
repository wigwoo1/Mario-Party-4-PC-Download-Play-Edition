#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../src/port/musyx_arrangement.h"
#include "../src/port/musyx_arrangement_convert.h"
static void word(uint8_t *p,uint32_t v) {
    p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;
}
int main(void) {
    uint8_t raw[512]={0};
    PartyboardMusyxArrangement result={0},saved;
    PartyboardMusyxTrackEntry entry={0},saved_entry;
    PartyboardMusyxPattern pattern={0};
    PartyboardMusyxNote note={0},saved_note;
    PartyboardMusyxStreamEntry stream={0},saved_stream;
    PartyboardMusyxMasterEntry master={0},saved_master;
    PartyboardMusyxOwnedArrangement owned={0};
    word(raw,88);word(raw+4,344);word(raw+8,348);
    word(raw+16,120);word(raw+20,1234);word(raw+88,412);
    assert(PartyboardMusyxArrangementRead(&result,raw,sizeof(raw)));
    assert(result.info==120 && result.loops[0]==1234 && result.track[0]==412);
    saved=result;word(raw+88,505);
    assert(!PartyboardMusyxArrangementRead(&result,raw,sizeof(raw)));
    assert(memcmp(&result,&saved,sizeof(result))==0);
    word(raw+88,412);raw[349]=255;
    assert(PartyboardMusyxArrangementRead(&result,raw,sizeof(raw)));
    word(raw+88,412);raw[348]=16;
    assert(!PartyboardMusyxArrangementRead(&result,raw,sizeof(raw)));
    raw[348]=0;word(raw+16,0x80000078);word(raw+84,448);
    assert(PartyboardMusyxArrangementRead(&result,raw,sizeof(raw)));
    raw[448]=16;
    assert(!PartyboardMusyxArrangementRead(&result,raw,sizeof(raw)));
    assert(!PartyboardMusyxArrangementRead(&result,raw,87));
    word(raw+412,123456);raw[420]=0xff;raw[421]=0xfe;
    raw[422]=1;raw[423]=2;
    assert(PartyboardMusyxTrackEntryRead(&entry,raw,sizeof(raw),412));
    assert(entry.time==123456 && entry.pattern==65534 && entry.loop_index==258);
    assert(entry.transpose==0 && entry.velocity_add==0);
    raw[420]=0;raw[421]=7;raw[422]=255;raw[423]=128;
    assert(PartyboardMusyxTrackEntryRead(&entry,raw,sizeof(raw),412));
    assert(entry.pattern==7 && entry.transpose==-1 && entry.velocity_add==-128 && entry.loop_index==0);
    saved_entry=entry;
    assert(!PartyboardMusyxTrackEntryRead(&entry,raw,sizeof(raw),505));
    assert(memcmp(&entry,&saved_entry,sizeof(entry))==0);
    memset(raw+400,0,40);word(raw+400,12);
    raw[412]=1;raw[413]=2;raw[414]=60;raw[415]=100;raw[416]=3;raw[417]=4;
    assert(PartyboardMusyxPatternRead(&pattern,raw,sizeof(raw),400) && pattern.notes==412);
    assert(PartyboardMusyxNoteRead(&note,raw,sizeof(raw),pattern.notes));
    assert(note.time==258 && note.key==60 && note.velocity==100 && note.length==772 && note.bytes==6);
    raw[414]=128;
    assert(PartyboardMusyxNoteRead(&note,raw,416,412) && note.bytes==4 && note.length==0);
    raw[414]=255;raw[415]=255;
    assert(PartyboardMusyxNoteRead(&note,raw,416,412) && note.end && note.bytes==4);
    saved_note=note;raw[414]=60;raw[415]=100;
    assert(!PartyboardMusyxNoteRead(&note,raw,416,412));
    assert(memcmp(&note,&saved_note,sizeof(note))==0);
    raw[400]=129;raw[401]=2;raw[402]=255;raw[403]=254;
    assert(PartyboardMusyxStreamEntryRead(&stream,raw,404,400));
    assert(stream.time==258 && stream.value==-2 && stream.bytes==4 && !stream.end);
    raw[400]=3;raw[401]=127;
    assert(PartyboardMusyxStreamEntryRead(&stream,raw,402,400) && stream.value==-1 && stream.bytes==2);
    raw[400]=128;raw[401]=0;
    assert(PartyboardMusyxStreamEntryRead(&stream,raw,402,400) && stream.end);
    saved_stream=stream;raw[401]=1;
    assert(!PartyboardMusyxStreamEntryRead(&stream,raw,402,400));
    assert(memcmp(&stream,&saved_stream,sizeof(stream))==0);
    word(raw+400,123456);word(raw+404,120*1024);
    assert(PartyboardMusyxMasterEntryRead(&master,raw,408,400));
    assert(master.time==123456 && master.bpm==120*1024 && !master.end);
    word(raw+400,0xffffffffu);
    assert(PartyboardMusyxMasterEntryRead(&master,raw,408,400) && master.end);
    saved_master=master;
    assert(PartyboardMusyxMasterEntryRead(&master,raw,404,400) && master.end);
    word(raw+400,123456);
    assert(!PartyboardMusyxMasterEntryRead(&master,raw,407,400));
    assert(memcmp(&master,&saved_master,sizeof(master))==0);
    memset(raw,0,sizeof(raw));word(raw,88);word(raw+4,344);word(raw+8,348);
    word(raw+16,120);word(raw+88,412);raw[420]=255;raw[421]=255;
    assert(PartyboardMusyxArrangementConvert(&owned,raw,sizeof(raw)));
    { uint32_t tempo; uint16_t end;
      memcpy(&tempo,owned.data+16,4);memcpy(&end,owned.data+420,2);
      assert(tempo==120 && end==65535); }
    assert(!PartyboardMusyxArrangementConvert(&owned,raw,sizeof(raw)));
    PartyboardMusyxArrangementRelease(&owned);PartyboardMusyxArrangementRelease(&owned);
    word(raw+88,505);
    assert(!PartyboardMusyxArrangementConvert(&owned,raw,sizeof(raw)) && !owned.data);
    puts("Bounded arrangement readers and owned conversion PASS");return 0;
}
