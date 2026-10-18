#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "musyx/synth.h"
#include "musyx/macros.h"
#include "musyx/hardware.h"
#include "musyx/dspvoice.h"
#include "musyx/seq.h"
#include "../src/port/musyx_arrangement_convert.h"
#include "../src/port/musyx_voice_source.h"

int main(void) {
  DSPvoice voices[4] = {0};
  u32 end_command[2] = {0, 0};
  u32 sample_commands[4] = {0x710, 0, 0, 0};
  u32 wait_commands[4] = {7, 4u << 16, 0, 0};
  SDIR_DATA directory[2] = {0};
  unsigned char pcm[4] = {0x12, 0x34, 0xff, 0xfe};
  PartyboardMusyxSampleMemory memory = {0};
  PartyboardMsmSample source = {0};
  int16_t decoded[2];
  u32 id;
  unsigned i;
  unsigned char arrangement_raw[436]={0};
  PartyboardMusyxOwnedArrangement arrangement={0};
  PAGE pages[1]={{0}};
  salHooks.malloc = malloc;
  salHooks.free = free;
  dspVoice = voices;
  synthInfo.voiceNum = 4;
  synthInfo.maxMusic = 4;
  synthInfo.maxSFX = 4;
  synthInit(32000, 4);
  assert(synthVoice && synthInfo.mixFrq == 32000);
  for (i = 0; i < 4; ++i) assert(synthVoice[i].id == 0xffffffff);
  assert(dataInsertMacro(123, end_command));
  id = synthStartSound(123, 64, 4, 0xc0, 127, 64, 255, 255, 0, 0,
                      255, 23, 0, 0, 0);
  assert(id != 0xffffffff);
  macHandle(256);
  for (i = 0; i < 4; ++i) assert(synthVoice[i].addr == NULL);
  dataRemoveMacro(123); /* Original API returns FALSE even after removal. */
  assert(dataGetMacro(123) == NULL);
  directory[0].id = 7;
  directory[0].header.info = (60u << 24) | 32000;
  directory[0].header.length = (2u << 24) | 2;
  directory[1].id = 65535;
  assert(dataInsertSDir(directory, pcm));
  assert(dataAddSampleReference(7));
  assert(PartyboardMusyxSampleMemoryAdd(&memory, pcm, sizeof(pcm)));
  assert(dataInsertMacro(124, sample_commands));
  id = synthStartSound(124, 64, 4, 0xc0, 127, 64, 255, 255, 0, 0,
                      255, 23, 0, 0, 0);
  assert(id != 0xffffffff);
  macHandle(256);
  for (i = 0; i < 4 && voices[i].smp_id != 7; ++i) {}
  assert(i < 4);
  assert(PartyboardMusyxVoiceSourceOpen(&source, &voices[i], &memory));
  assert(PartyboardMsmSampleRead(&source, decoded, 2) == 2);
  assert(decoded[0] == 0x1234 && decoded[1] == -2);
  dataRemoveMacro(124);
  assert(dataGetMacro(124) == NULL);
  dataRemoveSampleReference(7);
  assert(dataRemoveSDir(directory));
  assert(PartyboardMusyxSampleMemoryRemove(&memory, pcm));
  assert(dataInsertMacro(125, wait_commands));
  id = synthStartSound(125, 64, 4, 0xc0, 127, 64, 255, 255, 0, 0,
                      255, 23, 0, 0, 0);
  assert(id != 0xffffffff);
  for (i = 0; i < 4 && synthVoice[i].addr == NULL; ++i) {}
  assert(i < 4);
  macHandle(256);
  assert(synthVoice[i].addr != NULL);
  assert(synthVoice[i].wait - synthVoice[i].macStartTime == 1024);
  macHandle(256); macHandle(256); macHandle(256);
  assert(synthVoice[i].addr != NULL);
  macHandle(256);
  assert(synthVoice[i].addr == NULL);
  dataRemoveMacro(125);
  assert(dataGetMacro(125) == NULL);
  /* One terminated track exercises the real sequencer without mock notes. */
  arrangement_raw[3]=88;
  arrangement_raw[6]=1;arrangement_raw[7]=88; /* pattern table344 */
  arrangement_raw[10]=1;arrangement_raw[11]=92; /* channels348 */
  arrangement_raw[19]=120;
  arrangement_raw[90]=1;arrangement_raw[91]=156; /* track412 */
  arrangement_raw[420]=255;arrangement_raw[421]=255;
  pages[0].index=255;
  assert(PartyboardMusyxArrangementConvert(&arrangement,arrangement_raw,sizeof(arrangement_raw)));
  seqInit();
  id=seqStartPlay(pages,pages,NULL,(u32*)arrangement.data,NULL,0,7);
  assert(id!=0xffffffff);
  for(i=0;i<8 && seqInstance[i].arrbase!=(ARR*)arrangement.data;++i) {}
  assert(i<8 && seqInstance[i].state==1);
  assert(seqInstance[i].section[0].bpm==120*1024);
  seqHandle(256);
  assert(seqInstance[i].state==0);
  /* Also verify explicit stop for a separately allocated sequence. */
  id=seqStartPlay(pages,pages,NULL,(u32*)arrangement.data,NULL,0,7);
  assert(id!=0xffffffff);
  for(i=0;i<8 && !(seqInstance[i].state==1 && seqInstance[i].arrbase==(ARR*)arrangement.data);++i) {}
  assert(i<8);
  seqStop(id);
  assert(seqInstance[i].state==0);
  PartyboardMusyxArrangementRelease(&arrangement);
  synthExit();
  puts("Original synthesizer init, macro allocation and end execution PASS");
  return 0;
}
