/* ---------------------------------------








   ---------------------------------------
*/
#include "musyx/musyx.h"
#include "musyx/sal.h"
#include <float.h>
#include <math.h>
#include <string.h>

static void DLsetdelay(_SND_REVSTD_DELAYLINE* dl, s32 lag) {
  dl->outPoint = dl->inPoint - (lag * sizeof(f32));
  while (dl->outPoint < 0) {
    dl->outPoint += dl->length;
  }
}

static bool DLcreate(_SND_REVSTD_DELAYLINE* dl, s32 len) {
  dl->length = (s32)len * sizeof(f32);
  dl->inputs = (f32*)salMalloc(len * sizeof(f32));
  if (!dl->inputs) return FALSE;
  memset(dl->inputs, 0, len * sizeof(f32));
  dl->lastOutput = 0.f;
  DLsetdelay(dl, len >> 1);
  dl->inPoint = 0;
  dl->outPoint = 0;
  return TRUE;
}

static void DLdelete(_SND_REVSTD_DELAYLINE* dl) {
  if (dl->inputs) salFree(dl->inputs);
  memset(dl, 0, sizeof(*dl));
}

void ReverbSTDFree(_SND_REVSTD_WORK* rv);

bool ReverbSTDCreate(_SND_REVSTD_WORK* rv, float coloration, float time, float mix, float damping,
                     float predelay) {
  static u32 lens[4] = {1789, 1999, 433, 149};
  u8 i; // r31
  u8 k; // r29
  _SND_REVSTD_WORK next = {0};

  if (!rv || !isfinite(coloration) || !isfinite(time) || !isfinite(mix)
      || !isfinite(damping) || !isfinite(predelay)
      || (coloration < 0.f || coloration > 1.f) || (time < .01f || time > 10.f) ||
      (mix < 0.f || mix > 1.f) || (damping < 0.f || damping > 1.f) ||
      (predelay < 0.f || predelay > 0.1f)) {
    return FALSE;
  }

  /* Construct separately: allocation failure must not publish a partial
   * effect, or destroy the live effect being replaced by Modify. */
  for (k = 0; k < 3; ++k) {
    for (i = 0; i < 2; ++i) {
      if (!DLcreate(&next.C[i + k * 2], lens[i] + 2)) goto failed;
      DLsetdelay(&next.C[i + k * 2], lens[i]);
      next.combCoef[i + k * 2] = powf(10.f, (int)(lens[i] * -3) / (time * 32000.f));
    }
    for (i = 0; i < 2; ++i) {
      if (!DLcreate(&next.AP[i + k * 2], lens[i + 2] + 2)) goto failed;
      DLsetdelay(&next.AP[i + k * 2], lens[i + 2]);
    }
    next.lpLastout[k] = 0.f;
  }

  next.allPassCoeff = coloration;
  next.level = mix;
  next.damping = damping;
  if (next.damping < 0.05f) {
    next.damping = 0.05f;
  }

  next.damping = 1.f - (next.damping * 0.8f + 0.05f);

  if (predelay != 0.f) {
    next.preDelayTime = (int)(predelay * 32000.f);
    for (k = 0; k < 3; ++k) {
      if (!next.preDelayTime) break;
      next.preDelayLine[k] = salMalloc(next.preDelayTime * sizeof(f32));
      if (!next.preDelayLine[k]) goto failed;
      memset(next.preDelayLine[k], 0, next.preDelayTime * sizeof(f32));
      next.preDelayPtr[k] = next.preDelayLine[k];
    }
  } else {
    next.preDelayTime = 0;
    for (k = 0; k < 3; ++k) {
      next.preDelayPtr[k] = NULL;
      next.preDelayLine[k] = NULL;
    }
  }

  *rv = next;
  return TRUE;
failed:
  ReverbSTDFree(&next);
  return FALSE;
}

bool ReverbSTDModify(_SND_REVSTD_WORK* rv, float coloration, float time, float mix, float damping,
                     float predelay) {
  _SND_REVSTD_WORK next = {0};
  if (!rv || !ReverbSTDCreate(&next, coloration, time, mix, damping, predelay)) return FALSE;
  ReverbSTDFree(rv);
  *rv = next;
  return TRUE;
}

#ifdef __MWERKS__
static const float value0_3 = 0.3f;
static const float value0_6 = 0.6f;
static const double i2fMagic = 4.503601774854144E15;
/* clang-format off */
static asm void HandleReverb(long* sptr, struct _SND_REVSTD_WORK* rv) {
#if 1
  nofralloc
  stwu r1, -0x90(r1)
  stmw r17, 8(r1)
  stfd f14, 0x58(r1)
  stfd f15, 0x60(r1)
  stfd f16, 0x68(r1)
  stfd f17, 0x70(r1)
  stfd f18, 0x78(r1)
  stfd f19, 0x80(r1)
  stfd f20, 0x88(r1)
  lis r31, value0_3@ha
  lfs f6, value0_3@l(r31)
  lis r31, value0_6@ha
  lfs f9, value0_6@l(r31)
  lis r31, i2fMagic@ha
  lfd f5, i2fMagic@l(r31)
  lfs f2, 0xf0(r4)
  lfs f11, 0x11c(r4)
  lfs f8, 0x118(r4)
  fmuls f3, f8, f9
  fsubs f4, f9, f3
  lis r30, 0x4330
  stw r30, 0x50(r1)
  li r5, 0
  
lbl_803B56C8:
  slwi r31, r5, 3
  add r31, r31, r4
  lfs f19, 0xf4(r31)
  lfs f20, 0xf8(r31)
  slwi r31, r5, 2
  add r31, r31, r4
  lfs f7, 0x10c(r31)
  lwz r27, 0x124(r31)
  lwz r28, 0x130(r31)
  lwz r31, 0x120(r4)
  addi r22, r31, -1
  slwi r22, r22, 2
  add r22, r22, r27
  cmpwi cr7, r31, 0
  mulli r31, r5, 0x28
  addi r29, r4, 0x78
  add r29, r29, r31
  addi r30, r4, 0
  add r30, r30, r31
  lwz r21, 0(r29)
  lwz r20, 4(r29)
  lwz r19, 0x14(r29)
  lwz r18, 0x18(r29)
  lfs f15, 0x10(r29)
  lfs f16, 0x24(r29)
  lwz r26, 8(r29)
  lwz r25, 0x1c(r29)
  lwz r7, 0xc(r29)
  lwz r8, 0x20(r29)
  lwz r12, 0(r30)
  lwz r11, 4(r30)
  lwz r10, 0x14(r30)
  lwz r9, 0x18(r30)
  lfs f17, 0x10(r30)
  lfs f18, 0x24(r30)
  lwz r24, 8(r30)
  lwz r23, 0x1c(r30)
  lwz r17, 0xc(r30)
  lwz r6, 0x20(r30)
  lwz r30, 0(r3)
  xoris r30, r30, 0x8000
  stw r30, 0x54(r1)
  lfd f12, 0x50(r1)
  fsubs f12, f12, f5
  li r31, 0x9f
  mtctr r31

lbl_803B5780:
  fmr f13, f12
  beq cr7, lbl_803B57A0
  lfs f13, 0(r28)
  addi r28, r28, 4
  cmpw r28, r22
  stfs f12, -4(r28)
  bne+ lbl_803B57A0
  mr r28, r27
  
lbl_803B57A0:

  fmadds f8, f19, f15, f13
  lwzu r29, 4(r3)
  fmadds f9, f20, f16, f13
  stfsx f8, r7, r21
  addi r21, r21, 4
  stfsx f9, r8, r19
  lfsx f14, r7, r20
  addi r20, r20, 4
  lfsx f16, r8, r18
  cmpw r21, r26
  cmpw cr1, r20, r26
  addi r19, r19, 4
  addi r18, r18, 4
  fmr f15, f14
  cmpw cr5, r19, r25
  fadds f14, f14, f16
  cmpw cr6, r18, r25
  bne+ lbl_803B57EC
  li r21, 0

lbl_803B57EC:
  xoris r29, r29, 0x8000
  fmadds f9, f2, f17, f14
  bne+ cr1, lbl_803B57FC
  li r20, 0

lbl_803B57FC:
  stw r29, 0x54(r1)
  bne+ cr5, lbl_803B5808
  li r19, 0

lbl_803B5808:
  stfsx f9, r17, r12
  fnmsubs f14, f2, f9, f17
  addi r12, r12, 4
  bne+ cr6, lbl_803B581C
  li r18, 0

lbl_803B581C:
  lfsx f17, r17, r11
  cmpw cr5, r12, r24
  addi r11, r11, 4
  cmpw cr6, r11, r24
  bne+ cr5, lbl_803B5834
  li r12, 0

lbl_803B5834:
  bne+ cr6, lbl_803B583C
  li r11, 0

lbl_803B583C:
  fmuls f14, f14, f6
  lfd f10, 0x50(r1)
  fmadds f14, f11, f7, f14
  fmadds f9, f2, f18, f14
  fmr f7, f14
  stfsx f9, r6, r10
  fnmsubs f14, f2, f9, f18
  fmuls f8, f4, f12
  lfsx f18, r6, r9
  addi r10, r10, 4
  addi r9, r9, 4
  fmadds f14, f3, f14, f8
  cmpw cr5, r10, r23
  cmpw cr6, r9, r23
  fctiwz f14, f14
  bne+ cr5, lbl_803B5880
  li r10, 0

lbl_803B5880:
  bne+ cr6, lbl_803B5888
  li r9, 0

lbl_803B5888:
  li r31, -4
  fsubs f12, f10, f5
  stfiwx f14, r3, r31
  bdnz lbl_803B5780
  fmr f13, f12
  beq cr7, lbl_803B58B8
  lfs f13, 0(r28)
  addi r28, r28, 4
  cmpw r28, r22
  stfs f12, -4(r28)
  bne+ lbl_803B58B8
  mr r28, r27

lbl_803B58B8:
  fmadds f8, f19, f15, f13
  fmadds f9, f20, f16, f13
  stfsx f8, r7, r21
  addi r21, r21, 4
  stfsx f9, r8, r19
  lfsx f14, r7, r20
  addi r20, r20, 4
  lfsx f16, r8, r18
  cmpw r21, r26
  cmpw cr1, r20, r26
  addi r19, r19, 4
  addi r18, r18, 4
  fmr f15, f14
  cmpw cr5, r19, r25
  fadds f14, f14, f16
  cmpw cr6, r18, r25
  bne+ lbl_803B5900
  li r21, 0

lbl_803B5900:
  fmadds f9, f2, f17, f14
  bne+ cr1, lbl_803B590C
  li r20, 0

lbl_803B590C:
  bne+ cr5, lbl_803B5914
  li r19, 0

lbl_803B5914:
  stfsx f9, r17, r12
  fnmsubs f14, f2, f9, f17
  addi r12, r12, 4
  bne+ cr6, lbl_803B5928
  li r18, 0

lbl_803B5928:
  lfsx f17, r17, r11
  cmpw cr5, r12, r24
  addi r11, r11, 4
  cmpw cr6, r11, r24
  bne+ cr5, lbl_803B5940
  li r12, 0

lbl_803B5940:
  bne+ cr6, lbl_803B5948
  li r11, 0

lbl_803B5948:

  fmuls f14, f14, f6
  fmadds f14, f11, f7, f14
  mulli r31, r5, 0x28
  fmadds f9, f2, f18, f14
  fmr f7, f14
  addi r29, r4, 0x78
  add r29, r29, r31
  stfsx f9, r6, r10
  fnmsubs f14, f2, f9, f18
  fmuls f8, f4, f12
  lfsx f18, r6, r9
  addi r10, r10, 4
  
  addi r9, r9, 4  
  fmadds f14, f3, f14, f8
  cmpw cr5, r10, r23
  cmpw cr6, r9, r23
  
  fctiwz f14, f14
  bne+ cr5, lbl_803B5994
  li r10, 0

lbl_803B5994:
  bne+ cr6, lbl_803B599C
  li r9, 0

lbl_803B599C:
  addi r30, r4, 0
  add r30, r30, r31
  stfiwx f14, r0, r3
  stw r21, 0(r29)
  stw r20, 4(r29)
  stw r19, 0x14(r29)
  stw r18, 0x18(r29)
  addi r3, r3, 4
  stfs f15, 0x10(r29)
  stfs f16, 0x24(r29)
  slwi r31, r5, 2
  add r31, r31, r4
  addi r5, r5, 1
  stw r12, 0(r30)
  stw r11, 4(r30)
  stw r10, 0x14(r30)
  stw r9, 0x18(r30)
  cmpwi r5, 3
  stfs f17, 0x10(r30)
  stfs f18, 0x24(r30)
  stfs f7, 0x10c(r31)
  stw r28, 0x130(r31)
  bne lbl_803B56C8
  lfd f14, 0x58(r1)
  lfd f15, 0x60(r1)
  lfd f16, 0x68(r1)
  lfd f17, 0x70(r1)
  lfd f18, 0x78(r1)
  lfd f19, 0x80(r1)
  lfd f20, 0x88(r1)
  lmw r17, 8(r1)
  addi r1, r1, 0x90

  blr
#endif
}
#else
/* clang-format on */
static s32 ReverbSTDInteger(f32 value) {
  /* PowerPC fctiwz: truncate finite values, saturate overflow and map NaN to
   * INT32_MIN. Avoid undefined host float-to-int conversions. */
  if (isnan(value) || value < -2147483648.f) return (-2147483647 - 1);
  if (value >= 2147483648.f) return 2147483647;
  return (s32)value;
}

static f32 DLread(_SND_REVSTD_DELAYLINE* dl) {
  f32 sample = dl->inputs[dl->outPoint / 4];
  dl->outPoint += 4;
  if (dl->outPoint == dl->length) dl->outPoint = 0;
  dl->lastOutput = sample;
  return sample;
}

static void DLwrite(_SND_REVSTD_DELAYLINE* dl, f32 sample) {
  dl->inputs[dl->inPoint / 4] = sample;
  dl->inPoint += 4;
  if (dl->inPoint == dl->length) dl->inPoint = 0;
}

static void HandleReverbChannel(s32* samples, _SND_REVSTD_WORK* rv, unsigned channel) {
  _SND_REVSTD_DELAYLINE* comb = &rv->C[channel * 2];
  _SND_REVSTD_DELAYLINE* ap = &rv->AP[channel * 2];
  f32 wet = rv->level * 0.6f, dry = 0.6f - wet;
  f32 lowpass = rv->lpLastout[channel];
  unsigned i;
  for (i = 0; i < 160; ++i) {
    f32 input = (f32)samples[i], delayed = input;
    f32 value, written;
    if (rv->preDelayTime) {
      /* Preserve the assembly's time-1 wrap for normal lengths. One-sample
       * predelay is bounded instead of walking beyond its allocation. */
      s32 length = rv->preDelayTime > 1 ? rv->preDelayTime - 1 : 1;
      f32* cursor = rv->preDelayPtr[channel];
      delayed = *cursor;
      *cursor++ = input;
      if (cursor >= rv->preDelayLine[channel] + length) cursor = rv->preDelayLine[channel];
      rv->preDelayPtr[channel] = cursor;
    }
    DLwrite(&comb[0], fmaf(rv->combCoef[channel * 2], comb[0].lastOutput, delayed));
    DLwrite(&comb[1], fmaf(rv->combCoef[channel * 2 + 1], comb[1].lastOutput, delayed));
    value = DLread(&comb[0]) + DLread(&comb[1]);
    written = fmaf(rv->allPassCoeff, ap[0].lastOutput, value);
    DLwrite(&ap[0], written);
    value = -fmaf(rv->allPassCoeff, written, -ap[0].lastOutput);
    DLread(&ap[0]);
    value *= 0.3f;
    lowpass = fmaf(rv->damping, lowpass, value);
    written = fmaf(rv->allPassCoeff, ap[1].lastOutput, lowpass);
    DLwrite(&ap[1], written);
    value = -fmaf(rv->allPassCoeff, written, -ap[1].lastOutput);
    DLread(&ap[1]);
    samples[i] = ReverbSTDInteger(fmaf(wet, value, dry * input));
  }
  rv->lpLastout[channel] = lowpass;
}
#endif

void ReverbSTDCallback(s32* left, s32* right, s32* surround, _SND_REVSTD_WORK* rv) {
#ifdef __MWERKS__
  HandleReverb(left, rv);
#else
  HandleReverbChannel(left, rv, 0);
  HandleReverbChannel(right, rv, 1);
  HandleReverbChannel(surround, rv, 2);
#endif
}

void ReverbSTDFree(_SND_REVSTD_WORK* rv) {
  u8 i; // r31
  for (i = 0; i < 6; ++i) {
    DLdelete(&rv->AP[i]);
  }
  for (i = 0; i < 6; ++i) {
    DLdelete(&rv->C[i]);
  }
  if (rv->preDelayTime != 0) {
    for (i = 0; i < 3; ++i) {
      if (rv->preDelayLine[i]) salFree(rv->preDelayLine[i]);
    }
  }
  memset(rv, 0, sizeof(*rv));
}
