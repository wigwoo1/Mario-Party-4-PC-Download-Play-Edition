/* ---------------------------------------








   ---------------------------------------
*/

#include "musyx/musyx.h"

#include "musyx/assert.h"
#include <string.h>

extern bool ReverbHICreate(_SND_REVHI_WORK* rev, f32 coloration, f32 time, f32 mix, f32 damping,
                           f32 preDelay, f32 crosstalk);
extern void ReverbHIFree(_SND_REVHI_WORK* rev);
extern bool ReverbHIModify(_SND_REVHI_WORK* rev, f32 coloration, f32 time, f32 mix, f32 damping,
                           f32 preDelay, f32 crosstalk);
extern void ReverbHICallback(s32* left, s32* right, s32* surround, SND_AUX_REVERBHI* rev);

void sndAuxCallbackReverbHI(u8 reason, SND_AUX_INFO* info, void* user) {
  switch (reason) {
  case SND_AUX_REASON_BUFFERUPDATE:
    if (((SND_AUX_REVERBHI*)user)->tempDisableFX == 0) {
      ReverbHICallback(info->data.bufferUpdate.left, info->data.bufferUpdate.right,
                       info->data.bufferUpdate.surround, (SND_AUX_REVERBHI*)user);
    }

    break;
  case SND_AUX_REASON_PARAMETERUPDATE:
    break;
  default:
    MUSY_ASSERT(FALSE);
    break;
  }
}

bool sndAuxCallbackUpdateSettingsReverbHI(SND_AUX_REVERBHI* rev) {
  bool result;
  u8 wasDisabled = rev->tempDisableFX;
  rev->tempDisableFX = TRUE;
  result = ReverbHIModify(&rev->rv, rev->coloration, rev->time, rev->mix, rev->damping, rev->preDelay,
                 rev->crosstalk);
  rev->tempDisableFX = wasDisabled;
  return result;
}

bool sndAuxCallbackPrepareReverbHI(SND_AUX_REVERBHI* rev) {
  bool result;
  /* Fresh owner only; live settings changes use UpdateSettings. */
  rev->tempDisableFX = TRUE;
  memset(&rev->rv, 0, sizeof rev->rv);
  result = ReverbHICreate(&rev->rv, rev->coloration, rev->time, rev->mix, rev->damping, rev->preDelay,
                        rev->crosstalk);
  if (result) rev->tempDisableFX = FALSE;
  return result;
}

bool sndAuxCallbackShutdownReverbHI(SND_AUX_REVERBHI* rev) {
  rev->tempDisableFX = TRUE;
  ReverbHIFree(&rev->rv);
  return TRUE;
}
