/* Output stage: the last thing the signal sees before the host or the speaker.

   An adjustable soft clipper, then an explicit output trim, then a hard safety ceiling:

     |x| <= knee   ->  x                        (bit-exact; the slope at the knee is 1)
     |x| >  knee   ->  sign(x) * (knee + headroom * tanh ((|x| - knee) / headroom))

   The knee drops from full scale to 0.349 as SOFT CLIP goes from 0 to 1, so the higher the
   setting the earlier the peaks bend, while quiet material is never touched. The curve is
   continuous, monotonic and asymptotic to OUTPUT_CEILING; OUT GAIN then trims it and the
   clamp guarantees nothing above full scale ever leaves the app.

   This is the same math as sliceforge::OutputStage in native/Source/Engine/Dsp.h — the two
   editions are parallel implementations of one engine design, so a change here belongs there
   too. It is kept DOM-free so the tests can call it directly. */

import { clamp, dbToGain } from '../types';

export const OUTPUT_CEILING = 0.999;
export const MAX_KNEE_DROP = 0.65;
export const OUT_GAIN_RANGE: [number, number] = [-24, 12];

/** Old saved kits predate these two fields, so a missing or corrupt value means bypassed. */
function orDefault(value: number | undefined, fallback: number): number {
  return typeof value === 'number' && Number.isFinite(value) ? value : fallback;
}

/** The clipper's threshold: 0.999 (effectively bypassed) at clip 0, 0.349 at clip 1. */
export function outputKnee(clip: number): number {
  return OUTPUT_CEILING - MAX_KNEE_DROP * clamp(orDefault(clip, 0), 0, 1);
}

/** Soft clipper alone: never attenuates below the knee, never reaches the ceiling above it. */
export function softClip(x: number, clip: number): number {
  const knee = outputKnee(clip);
  const headroom = OUTPUT_CEILING - knee;
  const magnitude = Math.abs(x);
  if (headroom <= 0 || magnitude <= knee) return x;
  const bend = knee + headroom * Math.tanh((magnitude - knee) / headroom);
  return x < 0 ? -bend : bend;
}

export function clampToCeiling(x: number): number {
  if (x > OUTPUT_CEILING) return OUTPUT_CEILING;
  if (x < -OUTPUT_CEILING) return -OUTPUT_CEILING;
  return x;
}

/** The whole stage for one sample: clip, then trim, then ceiling. */
export function applyOutputStage(x: number, clip: number, outGainDb: number): number {
  const db = clamp(orDefault(outGainDb, 0), OUT_GAIN_RANGE[0], OUT_GAIN_RANGE[1]);
  return clampToCeiling(softClip(x, clip) * dbToGain(db));
}

/** WaveShaper transfer curve for the whole stage; the domain is the full -1..1 input range,
    and a WaveShaper already maps anything beyond it onto these endpoints. */
export function buildOutputCurve(clip: number, outGainDb: number, samples = 4096): Float32Array {
  const curve = new Float32Array(samples);
  for (let i = 0; i < samples; i++) {
    const x = (i / (samples - 1)) * 2 - 1;
    curve[i] = applyOutputStage(x, clip, outGainDb);
  }
  return curve;
}

/** Reassigning a WaveShaper curve allocates, so the live graph compares this key first. */
export function outputCurveKey(clip: number, outGainDb: number): string {
  const c = clamp(orDefault(clip, 0), 0, 1);
  const g = clamp(orDefault(outGainDb, 0), OUT_GAIN_RANGE[0], OUT_GAIN_RANGE[1]);
  return `${c.toFixed(4)}|${g.toFixed(3)}`;
}
