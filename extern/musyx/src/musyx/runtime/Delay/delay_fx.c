/* ---------------------------------------








   ---------------------------------------
*/
#include "musyx/musyx.h"

#include "musyx/assert.h"
#include "musyx/sal.h"
#include <stdint.h>
#include <string.h>

static s32 DelaySigned(u32 bits) {
  return bits <= 0x7fffffffU ? (s32)bits : (s32)((int64_t)bits - 4294967296LL);
}
static s32 DelayGain(s32 value, u32 gain) {
  /* Original32-bit product wraps before its signed arithmetic shift. Use
   * explicit word conversion/floor division on every host/compiler. */
  s32 product = DelaySigned((u32)value * gain);
  return product >= 0 ? product / 128 : (s32)(-((-(int64_t)product + 127) / 128));
}
static void DelayResetWork(SND_AUX_DELAY* delay) {
  memset(delay->currentSize, 0, sizeof delay->currentSize);
  memset(delay->currentPos, 0, sizeof delay->currentPos);
  memset(delay->currentFeedback, 0, sizeof delay->currentFeedback);
  memset(delay->currentOutput, 0, sizeof delay->currentOutput);
  delay->left = delay->right = delay->sur = NULL;
}

void sndAuxCallbackDelay(u8 reason, SND_AUX_INFO* info, void* user) {
  s32 l;           // r30
  s32 r;           // r29
  s32 s;           // r28
  s32* lBuf;       // r27
  s32* rBuf;       // r26
  s32* sBuf;       // r25
  u32 i;  // r24
  SND_AUX_DELAY* c; // r31
  s32* left;       // r23
  s32* right;      // r22
  s32* sur;        // r21

  switch (reason) {
  case SND_AUX_REASON_BUFFERUPDATE:
    c = (SND_AUX_DELAY*)user;
    if (!c || !info || !c->left || !c->right || !c->sur
        || !info->data.bufferUpdate.left || !info->data.bufferUpdate.right
        || !info->data.bufferUpdate.surround) return;
    for (i = 0; i < 3; ++i)
      if (!c->currentSize[i] || c->currentPos[i] >= c->currentSize[i]
          || c->currentFeedback[i] > 128 || c->currentOutput[i] > 128) return;
    left = info->data.bufferUpdate.left;
    right = info->data.bufferUpdate.right;
    sur = info->data.bufferUpdate.surround;
    lBuf = c->left + (c->currentPos[0] * 160);
    rBuf = c->right + (c->currentPos[1] * 160);
    sBuf = c->sur + (c->currentPos[2] * 160);
    for (i = 0; i < 160; ++i) {

      l = *lBuf;
      r = *rBuf;
      s = *sBuf;

      *lBuf++ = DelaySigned((u32)*left + (u32)DelayGain(l, c->currentFeedback[0]));
      *rBuf++ = DelaySigned((u32)*right + (u32)DelayGain(r, c->currentFeedback[1]));
      *sBuf++ = DelaySigned((u32)*sur + (u32)DelayGain(s, c->currentFeedback[2]));

      *left++ = DelayGain(l, c->currentOutput[0]);
      *right++ = DelayGain(r, c->currentOutput[1]);
      *sur++ = DelayGain(s, c->currentOutput[2]);
    }

    c->currentPos[0] = (c->currentPos[0] + 1) % c->currentSize[0];
    c->currentPos[1] = (c->currentPos[1] + 1) % c->currentSize[1];
    c->currentPos[2] = (c->currentPos[2] + 1) % c->currentSize[2];
    break;
  case SND_AUX_REASON_PARAMETERUPDATE:
    break;
  default:
    MUSY_ASSERT(FALSE);
    break;
  }
}

bool sndAuxCallbackUpdateSettingsDelay(SND_AUX_DELAY* delay) {
  u32 i;
  SND_AUX_DELAY next = {0};
  s32** buffers[3] = {&next.left, &next.right, &next.sur};
  if (!delay) return FALSE;

  for (i = 0; i < 3; ++i) {
    uint64_t blocks, bytes;
    if (delay->delay[i] <= 5 || delay->feedback[i] > 100 || delay->output[i] > 100) return FALSE;
    blocks = ((uint64_t)(delay->delay[i] - 5) * 32 + 159) / 160;
    bytes = blocks * 160 * sizeof(s32);
    if (!blocks || blocks > UINT32_MAX || bytes > SIZE_MAX || bytes > UINT32_MAX) return FALSE;
    next.delay[i] = delay->delay[i]; next.feedback[i] = delay->feedback[i]; next.output[i] = delay->output[i];
    next.currentSize[i] = (u32)blocks;
    next.currentFeedback[i] = (delay->feedback[i] * 128) / 100;
    next.currentOutput[i] = (delay->output[i] * 128) / 100;
  }
  for (i = 0; i < 3; ++i) {
    size_t bytes = (size_t)next.currentSize[i] * 160 * sizeof(s32);
    *buffers[i] = (s32*)salMalloc(bytes);
    if (!*buffers[i]) { sndAuxCallbackShutdownDelay(&next); return FALSE; }
    memset(*buffers[i], 0, bytes);
  }
  sndAuxCallbackShutdownDelay(delay);
  *delay = next;
  return TRUE;
}

bool sndAuxCallbackPrepareDelay(SND_AUX_DELAY* delay) {
  if (!delay) return FALSE;
  /* Fresh owner only. A failed Prepare is safe for callback and Shutdown. */
  DelayResetWork(delay);
  return sndAuxCallbackUpdateSettingsDelay(delay);
}

bool sndAuxCallbackShutdownDelay(SND_AUX_DELAY* delay) {
  if (!delay) return FALSE;
  if (delay->left) salFree(delay->left);
  if (delay->right) salFree(delay->right);
  if (delay->sur) salFree(delay->sur);
  DelayResetWork(delay);
  return TRUE;
}
