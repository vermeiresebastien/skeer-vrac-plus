/* Global master section: DJ-style filter, tone tilt, master fold and output gain, then the
 * output stage (soft clipper -> OUT GAIN -> ceiling) on the way to the destination.
 * Built once for the live context and replicated inside export renders. */

import type { GlobalParams } from '../types';
import { clamp, dbToGain } from '../types';
import { buildOutputCurve, outputCurveKey } from './outputStage';

/** The master fold, the same expression as sliceforge::MasterChain::process in
 *  native/Source/Engine/Dsp.h: tanh(x * 1.1) normalised so a full-scale sample leaves the stage
 *  at full scale. Native evaluates it for any input and tops out near 1.25, which is exactly the
 *  overshoot its output ceiling exists to catch — a WaveShaper can only be driven from -1..1, so
 *  the graph pre-divides by MASTER_FOLD_DOMAIN and post-multiplies back. That makes the curve
 *  exact for |x| <= 2; past the domain the shaper holds the endpoint, within 2% of the same
 *  asymptote. */
export const MASTER_FOLD_DRIVE = 1.1;
export const MASTER_FOLD_DOMAIN = 2;

/** One sample of the fold, for offline renders that have no graph to build. */
export function masterFold(x: number): number {
  return Math.tanh(x * MASTER_FOLD_DRIVE) / Math.tanh(MASTER_FOLD_DRIVE);
}

export function makeMasterFoldCurve(samples = 4096): Float32Array {
  const curve = new Float32Array(samples);
  for (let i = 0; i < samples; i++) {
    const u = (i / (samples - 1)) * 2 - 1;
    curve[i] = masterFold(u * MASTER_FOLD_DOMAIN) / MASTER_FOLD_DOMAIN;
  }
  return curve;
}

export class MasterChain {
  readonly input: GainNode;
  readonly output: GainNode;
  private readonly ctx: BaseAudioContext;
  private readonly lowpass: BiquadFilterNode;
  private readonly highpass: BiquadFilterNode;
  private readonly toneLow: BiquadFilterNode;
  private readonly toneHigh: BiquadFilterNode;
  private readonly foldIn: GainNode;
  private readonly shaper: WaveShaperNode;
  private readonly foldOut: GainNode;
  private readonly volume: GainNode;
  private readonly outShape: WaveShaperNode;
  private outCurveKey = '';

  constructor(ctx: BaseAudioContext) {
    this.ctx = ctx;
    this.input = ctx.createGain();
    this.output = ctx.createGain();

    this.lowpass = ctx.createBiquadFilter();
    this.lowpass.type = 'lowpass';
    this.lowpass.frequency.value = 20000;
    this.lowpass.Q.value = 0.72;

    this.highpass = ctx.createBiquadFilter();
    this.highpass.type = 'highpass';
    this.highpass.frequency.value = 20;
    this.highpass.Q.value = 0.72;

    this.toneLow = ctx.createBiquadFilter();
    this.toneLow.type = 'lowshelf';
    this.toneLow.frequency.value = 280;
    this.toneLow.gain.value = 0;

    this.toneHigh = ctx.createBiquadFilter();
    this.toneHigh.type = 'highshelf';
    this.toneHigh.frequency.value = 3400;
    this.toneHigh.gain.value = 0;

    this.foldIn = ctx.createGain();
    this.foldIn.gain.value = 1 / MASTER_FOLD_DOMAIN;

    this.shaper = ctx.createWaveShaper();
    this.shaper.curve = makeMasterFoldCurve() as Float32Array<ArrayBuffer>;
    this.shaper.oversample = '2x';

    this.foldOut = ctx.createGain();
    this.foldOut.gain.value = MASTER_FOLD_DOMAIN;

    this.volume = ctx.createGain();
    this.volume.gain.value = 0.7;

    this.outShape = ctx.createWaveShaper();
    this.outShape.curve = buildOutputCurve(0, 0) as Float32Array<ArrayBuffer>;
    this.outShape.oversample = '2x';

    // No compressor anywhere in this chain: sliceforge::MasterChain has no dynamics stage at all,
    // so a fixed one here would squeeze the web mix in a way the native reference never does, and
    // the user has no control to bypass it with. Bending peaks is SOFT CLIP's job.
    this.input.connect(this.lowpass);
    this.lowpass.connect(this.highpass);
    this.highpass.connect(this.toneLow);
    this.toneLow.connect(this.toneHigh);
    this.toneHigh.connect(this.foldIn);
    this.foldIn.connect(this.shaper);
    this.shaper.connect(this.foldOut);
    this.foldOut.connect(this.volume);
    this.volume.connect(this.outShape);
    this.outShape.connect(this.output);
  }

  setGlobal(global: GlobalParams, immediate = false): void {
    const now = this.ctx.currentTime;
    const setParam = (param: AudioParam, value: number, timeConstant = 0.03) => {
      if (immediate) param.value = value;
      else {
        param.cancelScheduledValues(now);
        param.setTargetAtTime(value, now, timeConstant);
      }
    };

    const filter = clamp(global.filter, -1, 1);
    let lowpass = 20000;
    let highpass = 20;
    if (filter < 0) {
      lowpass = 20000 * Math.pow(120 / 20000, -filter);
      if (lowpass < 15000) highpass = Math.max(20, lowpass * 0.18);
    } else if (filter > 0) {
      highpass = 20 * Math.pow(8200 / 20, filter);
      lowpass = Math.min(20000, highpass * 12);
    }
    setParam(this.lowpass.frequency, clamp(lowpass, 80, 20500), 0.035);
    setParam(this.highpass.frequency, clamp(highpass, 18, 12000), 0.035);

    const tone = clamp(global.tone, -12, 12);
    setParam(this.toneLow.gain, -tone * 0.36, 0.04);
    setParam(this.toneHigh.gain, tone * 0.58, 0.04);
    setParam(this.volume.gain, dbToGain(clamp(global.volume, -40, 6)), 0.045);

    // The output stage is one static curve — clip, trim, ceiling — and a WaveShaper cannot be
    // target-automated, so it is rebuilt only when SOFT CLIP or OUT GAIN actually moves.
    const key = outputCurveKey(global.clip, global.outGain);
    if (key !== this.outCurveKey) {
      this.outCurveKey = key;
      this.outShape.curve = buildOutputCurve(global.clip, global.outGain) as Float32Array<ArrayBuffer>;
    }
  }

  connect(destination: AudioNode): void {
    this.output.connect(destination);
  }
}
