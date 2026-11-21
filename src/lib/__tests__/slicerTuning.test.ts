import { describe, expect, it } from 'vitest';
/*  The native tuning header, read as text — the same `?raw` idiom `importLimits.test.ts` uses, and
    for the same reason: this project has no Node types, so `node:fs` would fail `tsc -b`, the only
    type gate. The check is one-directional by nature — it reads the C++ text and compares this
    edition's mirror, so a native-only edit to any constant reddens `npm test`. */
import nativeHeader from '../../../native/Source/Engine/SlicerTuning.h?raw';
import { MAX_SLICES } from '../../types';
import {
  DEDUP_GATE_SECONDS,
  FLOOR_RATIO,
  MIN_GAP_SECONDS,
  ONSET_BIAS_SECONDS,
  TARGET_BASE,
  TARGET_SPAN,
} from '../../audio/slicerTuning';

function nativeConstant(name: string): number {
  const found = new RegExp(`constexpr\\s+\\S+\\s+${name}\\s*=\\s*([0-9.]+)`).exec(nativeHeader);
  if (found == null) throw new Error(`${name} is not stated in Engine/SlicerTuning.h`);
  return Number(found[1]);
}

describe('slicer tuning', () => {
  it('matches the native edition, number for number', () => {
    expect(nativeConstant('kTargetBase')).toBe(TARGET_BASE);
    expect(nativeConstant('kTargetSpan')).toBe(TARGET_SPAN);
    expect(nativeConstant('kMinGapSeconds')).toBe(MIN_GAP_SECONDS);
    expect(nativeConstant('kFloorRatio')).toBe(FLOOR_RATIO);
    expect(nativeConstant('kOnsetBiasSeconds')).toBe(ONSET_BIAS_SECONDS);
    expect(nativeConstant('kDedupGateSeconds')).toBe(DEDUP_GATE_SECONDS);
  });

  it('keeps the slice count inside the product ceiling at every sensitivity', () => {
    const countAt = (sens: number) => Math.round(TARGET_BASE + TARGET_SPAN * sens);
    expect(countAt(0)).toBe(8);
    expect(countAt(1)).toBe(54);
    /*  If this ever fails, `Math.min(target, MAX_SLICES)` in the detector is no longer inert at the
        top of the knob and the sensitivity the user sees stops meaning "more slices". */
    expect(countAt(1)).toBeLessThanOrEqual(MAX_SLICES);
  });

  /*  The bias is a subtraction at the use site, and the regex above can only read an unsigned
      literal — so a sign flipped in one edition would pass the parity test silently. */
  it('states the onset bias as a positive pull-back, in both editions', () => {
    expect(ONSET_BIAS_SECONDS).toBeGreaterThan(0);
    expect(ONSET_BIAS_SECONDS).toBeLessThan(MIN_GAP_SECONDS);
    expect(nativeHeader).toMatch(/kOnsetBiasSeconds\s*=\s*0\.006\s*;/);
  });
});
