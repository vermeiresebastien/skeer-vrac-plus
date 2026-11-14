import { describe, expect, it } from 'vitest';
import {
  OUT_GAIN_RANGE,
  OUTPUT_CEILING,
  applyOutputStage,
  buildOutputCurve,
  outputCurveKey,
  outputKnee,
  softClip,
} from '../outputStage';
import { dbToGain } from '../../types';

/* The same curve the native sliceforge::OutputStage implements — these numbers are quoted
   in native/Tests/SmokeTest.cpp, so if one side moves the other has to move with it. */

describe('output stage', () => {
  it('starts bypassed and changes nothing below the ceiling', () => {
    expect(outputKnee(0)).toBeCloseTo(OUTPUT_CEILING, 6);
    for (const value of [0, 0.1234567, -0.7, 0.9989]) {
      expect(applyOutputStage(value, 0, 0)).toBe(value);
    }
  });

  it('still caps a sample above full scale when bypassed', () => {
    expect(applyOutputStage(1.4, 0, 0)).toBe(OUTPUT_CEILING);
    expect(applyOutputStage(-1.4, 0, 0)).toBe(-OUTPUT_CEILING);
  });

  it('moves the knee down as clip increases', () => {
    expect(outputKnee(1)).toBeCloseTo(0.349, 4);
    expect(outputKnee(0.5)).toBeCloseTo(0.674, 4);
    expect(outputKnee(-3)).toBeCloseTo(OUTPUT_CEILING, 6);
    expect(outputKnee(9)).toBeCloseTo(0.349, 4);
  });

  it('never attenuates below the knee', () => {
    const knee = outputKnee(1);
    expect(softClip(knee * 0.5, 1)).toBe(knee * 0.5);
    expect(softClip(-knee, 1)).toBe(-knee);
  });

  it('leaves the knee with a slope of one', () => {
    const knee = outputKnee(1);
    const step = 1e-5;
    const slope = (softClip(knee + step, 1) - knee) / step;
    expect(slope).toBeGreaterThan(0.99);
    expect(slope).toBeLessThan(1.01);
  });

  it('is monotonic and stays under the ceiling', () => {
    let previous = -Infinity;
    for (let x = 0; x <= 2.0001; x += 0.05) {
      const y = softClip(x, 1);
      expect(y).toBeGreaterThan(previous);
      expect(y).toBeLessThanOrEqual(OUTPUT_CEILING);
      previous = y;
    }
  });

  it('folds a full-scale peak to 0.844 at full clip', () => {
    expect(softClip(OUTPUT_CEILING, 1)).toBeCloseTo(0.844, 3);
    expect(softClip(-OUTPUT_CEILING, 1)).toBeCloseTo(-0.844, 3);
  });

  it('applies OUT GAIN after the clipper', () => {
    const clipped = softClip(0.5, 1);
    expect(applyOutputStage(0.5, 1, 6)).toBeCloseTo(clipped * dbToGain(6), 6);
    expect(applyOutputStage(0.5, 1, -6)).toBeCloseTo(clipped * dbToGain(-6), 6);
  });

  it('clamps OUT GAIN to its range and runs the ceiling last', () => {
    // +12 dB is the top of the range, so any request above it must behave like the top.
    expect(applyOutputStage(0.2, 1, 40)).toBeCloseTo(applyOutputStage(0.2, 1, OUT_GAIN_RANGE[1]), 9);
    expect(applyOutputStage(0.2, 1, -40)).toBeCloseTo(applyOutputStage(0.2, 1, OUT_GAIN_RANGE[0]), 9);
    // and whatever the trim asks for, the ceiling gets the last word.
    expect(applyOutputStage(0.5, 1, OUT_GAIN_RANGE[1])).toBe(OUTPUT_CEILING);
    expect(applyOutputStage(0.5, 1, -40)).toBeCloseTo(softClip(0.5, 1) * dbToGain(-24), 9);
  });

  it('survives a corrupt value from an older saved kit', () => {
    expect(applyOutputStage(0.5, undefined as unknown as number, undefined as unknown as number)).toBe(0.5);
    expect(applyOutputStage(0.5, Number.NaN, Number.NaN)).toBe(0.5);
  });

  it('bakes the stage into a WaveShaper curve', () => {
    // A curve is a Float32Array, so the ceiling lands as its float32 neighbour.
    const curve = buildOutputCurve(0, 0, 513);
    expect(curve.length).toBe(513);
    expect(curve[0]).toBeCloseTo(-OUTPUT_CEILING, 6);
    expect(curve[512]).toBeCloseTo(OUTPUT_CEILING, 6);
    expect(curve[256]).toBe(0);
    // x = 0.5 at index 256.5 of a 513-point curve spanning -1..1
    expect(curve[384]).toBeCloseTo(applyOutputStage(0.5, 0, 0), 6);

    const hot = buildOutputCurve(1, 12, 513);
    let monotonic = true;
    let previous = -Infinity;
    for (let i = 0; i < hot.length; i++) {
      if (hot[i] < previous || Math.abs(hot[i]) > OUTPUT_CEILING + 1e-6) monotonic = false;
      previous = hot[i];
    }
    expect(monotonic).toBe(true);
    expect(hot[512]).toBeCloseTo(OUTPUT_CEILING, 6);
    // x = 0.171875 at index 300: under the knee, so only the +12 dB trim moves it.
    expect(hot[300]).toBeGreaterThan(0.6);
    expect(hot[300]).toBeLessThan(OUTPUT_CEILING);
  });

  it('only rebuilds the curve when the stage actually moves', () => {
    expect(outputCurveKey(0.25, 3)).toBe(outputCurveKey(0.25, 3));
    expect(outputCurveKey(0.25, 3)).not.toBe(outputCurveKey(0.26, 3));
    expect(outputCurveKey(0.25, 3)).not.toBe(outputCurveKey(0.25, 2.9));
    expect(outputCurveKey(Number.NaN, Number.NaN)).toBe(outputCurveKey(0, 0));
  });
});
