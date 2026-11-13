/* Musical randomiser for the pads. It is deliberately opinionated: a random
 * kit still needs a kick, a snare, hats and a clap, and levels/pan shouldn't
 * end up unusable. Volume, mute/solo, bus and sample starts are left alone. */

import { Rng } from '../audio/rng';
import type { GlobalParams, PadParams } from '../types';
import { FAMILIES, MODEL_BANK, clamp } from '../types';

const ROLE_POOLS: Record<number, PadParams['family'][]> = {
  0: ['kick', 'kick', 'kick', 'snare'],
  1: ['kick', 'kick', 'snare', 'clap'],
  2: ['snare', 'snare', 'clap', 'kick'],
  3: ['clap', 'clap', 'snare', 'hat'],
  4: ['snare', 'snare', 'clap', 'hat'],
  5: ['snare', 'clap', 'snare', 'hat'],
  6: ['hat', 'hat', 'snare', 'clap'],
  7: ['hat', 'hat', 'snare', 'clap'],
  8: ['hat', 'hat', 'clap', 'snare'],
  9: ['hat', 'hat', 'clap', 'snare'],
  10: ['clap', 'clap', 'snare', 'hat'],
  11: ['clap', 'clap', 'snare', 'hat'],
  12: ['hat', 'hat', 'clap', 'snare'],
  13: ['hat', 'hat', 'clap', 'snare'],
  14: ['snare', 'clap', 'snare', 'hat'],
  15: ['clap', 'clap', 'snare', 'hat'],
};

const LENGTH_RANGE: Record<PadParams['family'], [number, number]> = {
  kick: [0.18, 0.6],
  snare: [0.1, 0.38],
  hat: [0.04, 0.32],
  clap: [0.1, 0.48],
};

function familyFor(index: number, rng: Rng, previous: PadParams): PadParams['family'] {
  const pool = ROLE_POOLS[index] ?? FAMILIES;
  // 55 % chance of keeping the musical role the pad already had.
  if (rng.chance(0.55) && pool.includes(previous.family)) return previous.family;
  return rng.pick(pool);
}

export function randomizePads(pads: PadParams[], seed = Date.now()): PadParams[] {
  const rng = new Rng(seed);
  return pads.map((pad, index) => {
    const family = familyFor(index, rng, pad);
    const modelCount = MODEL_BANK[family].length;
    const [lo, hi] = LENGTH_RANGE[family];
    const isHat = family === 'hat';
    const rand = rng.float(0, 1);
    return {
      ...pad,
      family,
      model: rng.int(0, modelCount - 1),
      drumify: clamp(0.34 + rng.float(0, 0.62), 0, 1),
      length: lo + (hi - lo) * Math.pow(rand, 0.75),
      pitch: rng.chance(0.35) ? rng.int(-4, 4) : 0,
      pan: isHat || family === 'clap' ? rng.float(-0.36, 0.36) : rng.float(-0.12, 0.12),
      choke: isHat ? 2 : 0,
      warp: rng.float(0, 0.65),
      impact: rng.float(0.1, 0.8),
      instability: rng.float(0, 0.3),
      toneShift: rng.chance(0.4) ? rng.float(-3, 3) : 0,
      super: rng.float(0, 0.55),
      eqLow: rng.chance(0.35) ? rng.float(-3, 3) : 0,
      eqMid: rng.chance(0.35) ? rng.float(-3, 3) : 0,
      eqHigh: rng.chance(0.35) ? rng.float(-3, 3) : 0,
      velocityAmount: rng.float(0.35, 0.9),
    };
  });
}

export function randomizeGlobal(global: GlobalParams, seed = Date.now() + 77): GlobalParams {
  const rng = new Rng(seed);
  return {
    ...global,
    drumify: clamp(global.drumify + rng.float(-40, 40), 20, 180),
    tone: clamp(global.tone + rng.float(-3, 3), -12, 12),
    filter: clamp(global.filter + rng.float(-0.12, 0.12), -0.5, 0.5),
    width: clamp(global.width + rng.float(-0.2, 0.2), 0, 1),
    length: clamp(global.length * rng.float(0.85, 1.15), 0.25, 2),
  };
}
