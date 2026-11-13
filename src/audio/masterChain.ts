/* Global master section: DJ-style filter, tone tilt, limiter and output gain.
 * Built once for the live context and replicated inside export renders. */

import type { GlobalParams } from '../types';
import { clamp, dbToGain } from '../types';

function makeSoftClipCurve(samples = 2048): Float32Array {
  const curve = new Float32Array(samples);
  for (let i = 0; i < samples; i++) {
    const x = (i / (samples - 1)) * 2 - 1;
    curve[i] = Math.tanh(x * 1.35) / Math.tanh(1.35);
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
  private readonly compressor: DynamicsCompressorNode;
  private readonly shaper: WaveShaperNode;
  private readonly volume: GainNode;

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

    this.compressor = ctx.createDynamicsCompressor();
    this.compressor.threshold.value = -7;
    this.compressor.knee.value = 8;
    this.compressor.ratio.value = 5;
    this.compressor.attack.value = 0.003;
    this.compressor.release.value = 0.16;

    this.shaper = ctx.createWaveShaper();
    this.shaper.curve = makeSoftClipCurve() as Float32Array<ArrayBuffer>;
    this.shaper.oversample = '2x';

    this.volume = ctx.createGain();
    this.volume.gain.value = 0.7;

    this.input.connect(this.lowpass);
    this.lowpass.connect(this.highpass);
    this.highpass.connect(this.toneLow);
    this.toneLow.connect(this.toneHigh);
    this.toneHigh.connect(this.compressor);
    this.compressor.connect(this.shaper);
    this.shaper.connect(this.volume);
    this.volume.connect(this.output);
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
  }

  connect(destination: AudioNode): void {
    this.output.connect(destination);
  }
}
