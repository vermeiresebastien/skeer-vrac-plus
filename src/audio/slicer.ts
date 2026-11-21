/* Transient detection for the AUTO SLICER.
 *
 * The detector works on a short RMS envelope and its positive flux. It picks
 * the strongest local peaks with a sensitivity-controlled target count and a
 * minimum spacing, which behaves far more predictably on dense foley than a
 * fixed threshold. The result is a list of times in seconds, ascending.
 *
 * The tuning numbers that decide all of that are in `slicerTuning.ts`, not here. */

import type { PadParams } from '../types';
import { MAX_SLICES, PAD_COUNT, clamp } from '../types';
import {
  DEDUP_GATE_SECONDS,
  FLOOR_RATIO,
  MIN_GAP_SECONDS,
  ONSET_BIAS_SECONDS,
  TARGET_BASE,
  TARGET_SPAN,
} from './slicerTuning';

export interface Slice {
  time: number;
  strength: number;
}

export interface SliceResult {
  slices: Slice[];
  envelope: Float32Array;
  hop: number;
  sampleRate: number;
}

const HOP = 128;
const WINDOW = 512;

export function detectSlices(
  left: Float32Array,
  right: Float32Array,
  sampleRate: number,
  sensitivity: number,
): SliceResult {
  const length = Math.min(left.length, right.length);
  const hopCount = Math.max(1, Math.floor(length / HOP));
  const env = new Float32Array(hopCount);

  // RMS envelope, window centred on each hop position.
  for (let h = 0; h < hopCount; h++) {
    const center = h * HOP;
    const start = Math.max(0, center - (WINDOW >> 1));
    const end = Math.min(length, start + WINDOW);
    let sum = 0;
    for (let i = start; i < end; i++) {
      const v = (left[i] + right[i]) * 0.5;
      sum += v * v;
    }
    env[h] = Math.sqrt(sum / Math.max(1, end - start));
  }

  // Positive flux, lightly smoothed by taking the local maximum.
  const flux = new Float32Array(hopCount);
  let maxFlux = 0;
  for (let h = 1; h < hopCount; h++) {
    const rise = Math.max(0, env[h] - env[h - 1]);
    const a = h > 1 ? Math.max(0, env[h - 1] - env[h - 2]) : 0;
    const b = h + 1 < hopCount ? Math.max(0, env[h + 1] - env[h]) : 0;
    const v = Math.max(rise, a * 0.5, b * 0.85);
    flux[h] = v;
    if (v > maxFlux) maxFlux = v;
  }

  const sens = clamp(sensitivity, 0, 1);
  const target = Math.round(TARGET_BASE + TARGET_SPAN * sens);
  const floor = maxFlux * FLOOR_RATIO;

  const candidates: Array<{ index: number; strength: number }> = [];
  for (let h = 2; h < hopCount - 2; h++) {
    const v = flux[h];
    if (v < floor) continue;
    if (v < flux[h - 1] || v < flux[h + 1]) continue;
    if (v < flux[h - 2] && v < flux[h + 2]) continue;
    candidates.push({ index: h, strength: v });
  }
  candidates.sort((a, b) => b.strength - a.strength);

  const chosen: number[] = [];
  const minGapHops = MIN_GAP_SECONDS * (sampleRate / HOP);
  for (const c of candidates) {
    if (chosen.length >= Math.min(target, MAX_SLICES)) break;
    const h = c.index;
    let conflict = false;
    for (const other of chosen) {
      if (Math.abs(other - h) < minGapHops) {
        conflict = true;
        break;
      }
    }
    if (!conflict) chosen.push(h);
  }

  chosen.sort((a, b) => a - b);
  const times: Slice[] = [];
  let last = -1;
  for (const h of chosen) {
    const time = Math.max(0, (h * HOP) / sampleRate - ONSET_BIAS_SECONDS);
    if (time - last < DEDUP_GATE_SECONDS) continue;
    last = time;
    times.push({
      time,
      strength: clamp(flux[h] / (maxFlux || 1), 0, 1),
    });
  }

  return { slices: times, envelope: env, hop: HOP, sampleRate };
}

/** Seconds from one slice's onset to the next, and to the end of the material for the last
 * slice. The single statement of where a slice ends — the mirror of `sliceGapSeconds` in
 * `native/Source/Engine/Slicer.cpp`, because both editions used to hand a pad an onset with no
 * end and play the rest of the recording behind it. */
export function sliceGapSeconds(slices: Slice[], index: number, duration: number): number {
  if (index < 0 || index >= slices.length) return 0;
  const start = slices[index].time;
  const end = Math.max(start, index + 1 < slices.length ? slices[index + 1].time : duration);
  return end - start;
}

/** The pad knob's share of a length in seconds: `engineCore` multiplies it back by the global,
 * so this divides it out. No other compensation — velocity's own influence stays the player's. */
export function padLengthFromSeconds(seconds: number, globalLength: number): number {
  return clamp(seconds / Math.max(0.25, globalLength), 0.02, 2);
}

/**
 * Distribute the detected slices across the sixteen pads. Pads keep their
 * chronological order: pad 0 (C1) gets the first hit, pad 15 (D#2) the last.
 * Repeated points are avoided until the slices run out — a four-onset take used
 * to wrap at every fourth pad, which put all sixteen pads on the first quarter
 * of the file and none on the rest of it.
 */
export function padStartsFromSlices(slices: Slice[], pads: PadParams[]): PadParams[] {
  if (slices.length === 0) return pads;
  const n = slices.length;
  return pads.map((pad, i) => {
    let t: number;
    if (n >= PAD_COUNT) t = (i * (n - 1)) / (PAD_COUNT - 1);
    else if (i < n) t = i;
    else t = ((i - n) * (n - 1)) / (PAD_COUNT - n);
    const idx = Math.min(n - 1, Math.round(t));
    return {
      ...pad,
      sampleStart: slices[idx].time,
      sliceIndex: idx,
    };
  });
}

/** Give each pad the length of the slice it was given. Kept apart from the start
 * assignment because the two doors differ: a factory preset ships its own pad
 * lengths and must not have them recomputed, while a take the user dropped on
 * the plugin has none until this says where each slice ends. */
export function assignSliceLengths(
  pads: PadParams[],
  slices: Slice[],
  duration: number,
  globalLength: number,
): PadParams[] {
  if (slices.length === 0) return pads;
  return pads.map((pad) => (pad.sliceIndex < 0
    ? pad
    : {
        ...pad,
        length: padLengthFromSeconds(sliceGapSeconds(slices, pad.sliceIndex, duration), globalLength),
      }));
}

/** An even spread used when no transients could be found. */
export function evenStarts(duration: number, count = PAD_COUNT): Slice[] {
  const usable = Math.max(0.01, duration - 0.05);
  return Array.from({ length: count }, (_, i) => ({
    time: (i / count) * usable + 0.01,
    strength: 0.5,
  }));
}

export function remapStartsAfterReslice(
  pads: PadParams[],
  oldSlices: Slice[],
  newSlices: Slice[],
): PadParams[] {
  if (newSlices.length === 0) return pads;
  return pads.map((pad) => {
    if (pad.sliceIndex < 0 || oldSlices.length === 0) return pad;
    // Find the old slice nearest the pad's stored start, then use the same
    // ordinal in the new detection.
    let nearest = pad.sliceIndex;
    let best = Infinity;
    for (let s = 0; s < oldSlices.length; s++) {
      const d = Math.abs(oldSlices[s].time - pad.sampleStart);
      if (d < best) {
        best = d;
        nearest = s;
      }
    }
    const ratio = oldSlices.length > 1 ? nearest / (oldSlices.length - 1) : 0;
    const target = Math.round(ratio * (newSlices.length - 1));
    const idx = clamp(target, 0, newSlices.length - 1);
    return { ...pad, sampleStart: newSlices[idx].time, sliceIndex: idx };
  });
}
