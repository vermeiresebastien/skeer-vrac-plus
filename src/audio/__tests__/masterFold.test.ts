import { describe, expect, it } from 'vitest';
import { MASTER_FOLD_DOMAIN, makeMasterFoldCurve, masterFold } from '../masterChain';

describe('master fold', () => {
  it('leaves a full-scale sample at full scale', () => {
    expect(masterFold(1)).toBeCloseTo(1, 10);
    expect(masterFold(-1)).toBeCloseTo(-1, 10);
  });

  it('is linear at the bottom and never exceeds its asymptote', () => {
    // Native divides tanh(1.1x) by tanh(1.1), so unity in gives unity out and the small-signal
    // slope is 1.1/tanh(1.1) = 1.374 — a fixed makeup gain, not a limiter. The web edition has to
    // carry the same slope or the two editions differ by 2.8 dB before the master fader.
    expect(masterFold(0.01) / 0.01).toBeCloseTo(1.1 / Math.tanh(1.1), 3);
    expect(masterFold(8)).toBeLessThan(1 / Math.tanh(1.1));
    expect(masterFold(8)).toBeGreaterThan(1.24);
  });

  it('keeps the curve monotonic and gain-preserving across the shaper domain', () => {
    const curve = makeMasterFoldCurve(4097);
    expect(curve.length).toBe(4097);
    // Index 2048 is input 0; the endpoints map to +/- MASTER_FOLD_DOMAIN.
    expect(curve[2048]).toBeCloseTo(0, 6);
    expect(curve[0]).toBeCloseTo(-masterFold(MASTER_FOLD_DOMAIN) / MASTER_FOLD_DOMAIN, 6);
    expect(curve[4096]).toBeCloseTo(masterFold(MASTER_FOLD_DOMAIN) / MASTER_FOLD_DOMAIN, 6);
    for (let i = 1; i < curve.length; i++) expect(curve[i]).toBeGreaterThan(curve[i - 1]);
    // Undoing the domain scale must reproduce the same curve native evaluates directly.
    expect(curve[4096] * MASTER_FOLD_DOMAIN).toBeCloseTo(masterFold(MASTER_FOLD_DOMAIN), 6);
  });
});
