#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#include "musyx/synth.h"
#include "musyx/macros.h"
#include "../src/port/musyx_group.h"
#include "../src/port/musyx_group_runtime.h"
#include "../src/port/musyx_voice_source.h"
#include "../src/port/musyx_pcm_envelope.h"
#include "../src/port/musyx_effect_parameters.h"

static void write_u32(uint32_t value) {
  uint8_t bytes[4] = {(uint8_t)(value >> 24), (uint8_t)(value >> 16),
                      (uint8_t)(value >> 8), (uint8_t)value};
  if (fwrite(bytes, 1, 4, stdout) != 4) abort();
}

void hwSetTimeOffset(u8 offset);

int main(int argc, char **argv) {
  uint8_t header[8], *samples[16] = {0};
  PartyboardMusyxGroup groups[16] = {0};
  PartyboardMusyxSampleMemory memory = {0};
  DSPvoice voices[64] = {0};
  GROUP_DATA *project = NULL;
  unsigned count, target, i, started = 0, selected = 0;
  int16_t decoded[64][32];
  size_t decoded_count[64] = {0};
  int16_t enveloped[64][32];
  size_t envelope_count[64] = {0};
  uint32_t release_blocks[64] = {0}, release_finished[64] = {0};
  int synth_control = argc == 2 && strcmp(argv[1], "--synth-control") == 0;
#ifdef _WIN32
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
  _set_error_mode(_OUT_TO_STDERR);
#endif
  salHooks.malloc = malloc; salHooks.free = free;
  dspVoice = voices;
  synthInfo.voiceNum = synthInfo.maxMusic = synthInfo.maxSFX = 64;
  synthInfo.numSamples = 32;
  synthInit(32000, 64);
  sndActive = 1; /* Fixture initializes synth directly; no audio device started. */
  dataInitStack();
  if (fread(header, 1, 4, stdin) != 4) return 2;
  count = PartyboardMsmU32(header);
  if (!count || count > 16) return 2;
  if (fread(header, 1, 4, stdin) != 4) return 2;
  target = PartyboardMsmU32(header);
  if (target >= count) return 2;
  for (i = 0; i < count; ++i) {
    size_t bytes, sample_bytes;
    uint8_t *raw;
    if (fread(header, 1, 8, stdin) != 8) return 2;
    bytes = PartyboardMsmU32(header); sample_bytes = PartyboardMsmU32(header + 4);
    if (!bytes || bytes > 67108864 || !sample_bytes || sample_bytes > 67108864) return 2;
    raw = malloc(bytes); samples[i] = malloc(sample_bytes);
    if (!raw || !samples[i]) return 3;
    if (fread(raw, 1, bytes, stdin) != bytes || fread(samples[i], 1, sample_bytes, stdin) != sample_bytes) return 4;
    {
      size_t project_offset;
      uint16_t group_id;
      if (bytes < 16) return 5;
      project_offset = PartyboardMsmU32(raw + 4);
      if (project_offset > bytes || bytes - project_offset < 6) return 5;
      group_id = PartyboardMsmU16(raw + project_offset + 4);
      if (PartyboardMusyxGroupPush(&groups[i], &memory, raw, bytes,
            samples[i], sample_bytes, 65535)) return 11;
      if (groups[i].project || PartyboardMusyxSampleMemoryAvailable(&memory, samples[i])) return 11;
      if (!PartyboardMusyxGroupPush(&groups[i], &memory, raw, bytes,
            samples[i], sample_bytes, group_id)) return 6;
      free(raw);
    }
  }
  project = (GROUP_DATA *)groups[target].project;
  if (project->type == 1) {
    uint8_t *table = (uint8_t *)project + project->data.fx.tableOff;
    uint16_t effects;
    memcpy(&effects, table, 2);
    if (effects) {
      FX_TAB *effect = (FX_TAB *)(table + 4);
      PartyboardMsmContainerEffect descriptor={0};
      u32 id;
      descriptor.effect=effect->id;descriptor.volume=127;descriptor.pan=64;
      id=PartyboardMusyxEffectStartNonPositional(&descriptor,127,64,0,0,0,0);
      if (id == 0xffffffff) return 7;
      started = 1;
      for (i = 0; i < 20; ++i) {
        hwSetTimeOffset((u8)(i % 5));
        if (synth_control) synthHandle(256);
        else macHandle(256);
      }
      for (i = 0; i < 64; ++i) if (voices[i].smp_info.addr) {
        PartyboardMsmSample source;
        if (!PartyboardMusyxVoiceSourceOpen(&source, &voices[i], &memory)) return 8;
        decoded_count[i] = PartyboardMsmSampleRead(&source, decoded[i], 32);
        if (!decoded_count[i]) return 8;
        if (synth_control) {
          PartyboardMusyxPcmEnvelope envelope;
          memcpy(enveloped[i], decoded[i], decoded_count[i] * sizeof(int16_t));
          if (PartyboardMusyxPcmEnvelopeStart(&envelope, &voices[i].adsr)) {
            envelope_count[i] = PartyboardMusyxPcmEnvelopeApply(&envelope,
                enveloped[i], decoded_count[i]);
            /* Finish the current control block before scheduling release. */
            if (envelope.remaining) {
              int16_t padding[32] = {0};
              PartyboardMusyxPcmEnvelopeApply(&envelope, padding, envelope.remaining);
            }
            adsrRelease(&envelope.adsr);
            while (!envelope.finished && release_blocks[i] < 10000) {
              int16_t block[32] = {0};
              PartyboardMusyxPcmEnvelopeApply(&envelope, block, 32);
              ++release_blocks[i];
            }
            release_finished[i] = envelope.finished != 0;
          }
        }
        ++selected;
      }
    }
  }
  /* Private output goes only to the validation driver's in-memory pipe. */
  write_u32(count); write_u32(started); write_u32(selected);
  for (i = 0; i < 64; ++i) if (decoded_count[i]) {
    size_t j;
    write_u32(i); write_u32(voices[i].smp_id);
    write_u32(voices[i].smp_info.offset); write_u32((uint32_t)decoded_count[i]);
    if (synth_control) {
      unsigned slot;
      write_u32(voices[i].state); write_u32(voices[i].studio);
      write_u32(voices[i].flags);
      write_u32(voices[i].srcTypeSelect); write_u32(voices[i].srcCoefSelect);
      for (slot = 0; slot < 5; ++slot) {
        write_u32(voices[i].changed[slot]); write_u32(voices[i].pitch[slot]);
      }
      write_u32(voices[i].volL); write_u32(voices[i].volR); write_u32(voices[i].volS);
      write_u32(voices[i].volLa); write_u32(voices[i].volRa); write_u32(voices[i].volSa);
      write_u32(voices[i].volLb); write_u32(voices[i].volRb); write_u32(voices[i].volSb);
    }
    for (j = 0; j < decoded_count[i]; ++j) {
      uint16_t value = (uint16_t)decoded[i][j];
      uint8_t bytes[2] = {(uint8_t)(value >> 8), (uint8_t)value};
      if (fwrite(bytes, 1, 2, stdout) != 2) return 10;
    }
    if (synth_control) {
      write_u32((uint32_t)envelope_count[i]);
      write_u32(release_blocks[i]); write_u32(release_finished[i]);
      for (j = 0; j < envelope_count[i]; ++j) {
        uint16_t value = (uint16_t)enveloped[i][j];
        uint8_t bytes[2] = {(uint8_t)(value >> 8), (uint8_t)value};
        if (fwrite(bytes, 1, 2, stdout) != 2) return 10;
      }
    }
  }
  synthKillAllVoices(0);
  for (i = count; i; --i) {
    if (!PartyboardMusyxGroupPop(&groups[i - 1], &memory, samples[i - 1])) return 9;
    if (PartyboardMusyxGroupPop(&groups[i - 1], &memory, samples[i - 1])) return 11;
    free(samples[i - 1]);
  }
  synthExit();
  return 0;
}
