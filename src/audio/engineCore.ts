/* ------------------------------------------------------------------ *
 * VRAC+ voice engine.
 *
 * Pure DSP: no DOM, no AudioWorklet globals. The AudioWorklet wrapper in
 * `worklet.ts` owns the message port; this class can also be driven from
 * Node tests with fake output blocks.
 *
 * Signal flow per voice:
 *   sample layer  ─┐
 *                  ├─ tone/EQ ─ warp ─ saturate ─ pan ─ bus
 *   model layer   ─┘
 *
 * The global master chain (filter, tone, limiter, volume) lives in the main
 * thread so it only needs to run once, after the eight stereo buses are
 * summed for monitoring. It is replicated for exports.
 * ------------------------------------------------------------------ */

import type { Family, GlobalParams, PadParams } from '../types';
import { PAD_COUNT, clamp, createDefaultPads, dbToGain, lerp, panGains, smoothstep } from '../types';
import { Biquad, TWO_PI } from './dsp';
import { Rng } from './rng';

export const MAX_VOICES = 32;
export const BUS_COUNT = 8;
const HALF_PI = Math.PI * 0.5;

export interface EngineSample {
  left: Float32Array;
  right: Float32Array;
  sampleRate: number;
}

export interface EngineParams {
  pads: PadParams[];
  global: GlobalParams;
}

export interface MeterSnapshot {
  buses: number[];
  voices: number;
}

interface KickModel {
  fStart: number;
  fEnd: number;
  pitchTau: number;
  bodyTauMul: number;
  click: number;
  drive: number;
  sub: number;
}

interface SnareModel {
  tone1: number;
  tone2: number;
  toneMix: number;
  noiseMix: number;
  noiseTauMul: number;
  bodyTauMul: number;
  center: number;
  q: number;
  snap: number;
  softAttack: number;
}

interface HatModel {
  decayMul: number;
  metal: number;
  noise: number;
  metalFreq: number;
  hp: number;
}

interface ClapModel {
  bursts: number;
  spacing: number;
  tailMul: number;
  center: number;
  q: number;
  wide: number;
}

export const KICK_MODELS: KickModel[] = [
  { fStart: 158, fEnd: 47, pitchTau: 0.042, bodyTauMul: 0.5, click: 0.1, drive: 1.4, sub: 0.14 },
  { fStart: 248, fEnd: 53, pitchTau: 0.016, bodyTauMul: 0.3, click: 0.5, drive: 2.1, sub: 0.06 },
  { fStart: 93, fEnd: 41, pitchTau: 0.12, bodyTauMul: 0.86, click: 0.05, drive: 1.05, sub: 0.24 },
  { fStart: 184, fEnd: 55, pitchTau: 0.028, bodyTauMul: 0.42, click: 0.34, drive: 3.4, sub: 0.12 },
];

export const SNARE_MODELS: SnareModel[] = [
  { tone1: 183, tone2: 327, toneMix: 0.45, noiseMix: 1, noiseTauMul: 0.34, bodyTauMul: 0.25, center: 2400, q: 0.85, snap: 0.65, softAttack: 0 },
  { tone1: 228, tone2: 386, toneMix: 0.62, noiseMix: 0.86, noiseTauMul: 0.2, bodyTauMul: 0.16, center: 3300, q: 0.95, snap: 0.8, softAttack: 0 },
  { tone1: 156, tone2: 271, toneMix: 0.18, noiseMix: 1, noiseTauMul: 0.55, bodyTauMul: 0.4, center: 1500, q: 0.5, snap: 0.18, softAttack: 0.018 },
  { tone1: 427, tone2: 691, toneMix: 0.9, noiseMix: 0.25, noiseTauMul: 0.09, bodyTauMul: 0.09, center: 4600, q: 1.6, snap: 0.55, softAttack: 0 },
];

export const HAT_MODELS: HatModel[] = [
  { decayMul: 0.52, metal: 0.62, noise: 0.5, metalFreq: 6200, hp: 7000 },
  { decayMul: 2.7, metal: 0.55, noise: 0.4, metalFreq: 4800, hp: 4800 },
  { decayMul: 1.15, metal: 0.95, noise: 0.18, metalFreq: 8200, hp: 6200 },
  { decayMul: 1.7, metal: 0.16, noise: 1, metalFreq: 3800, hp: 3800 },
];

export const CLAP_MODELS: ClapModel[] = [
  { bursts: 4, spacing: 0.0115, tailMul: 0.62, center: 1350, q: 1.1, wide: 0.6 },
  { bursts: 3, spacing: 0.008, tailMul: 0.4, center: 1850, q: 1.3, wide: 0.4 },
  { bursts: 5, spacing: 0.017, tailMul: 1.2, center: 920, q: 0.85, wide: 1 },
  { bursts: 2, spacing: 0.0055, tailMul: 0.3, center: 2800, q: 1.5, wide: 0.25 },
];

const FAMILY_GAIN: Record<Family, number> = {
  kick: 0.92,
  snare: 0.58,
  hat: 0.4,
  clap: 0.62,
};

/** Level trim per model, so moving around the bank doesn't jump in volume. */
const MODEL_TRIM: Record<Family, number[]> = {
  kick: [1, 1, 1, 1],
  snare: [1, 1.02, 1.06, 0.9],
  hat: [1.22, 1.16, 1.5, 0.6],
  clap: [1.04, 1, 1.12, 0.92],
};

const METAL_TABLE_SIZE = 4096;
const METAL_TABLE = (() => {
  const table = new Float32Array(METAL_TABLE_SIZE);
  const ratios = [1, 1.4471, 1.617, 1.9265, 2.5028, 2.6637];
  const phases = [0, 1.7, 3.1, 4.4, 5.2, 2.2];
  for (let i = 0; i < METAL_TABLE_SIZE; i++) {
    const p = (i / METAL_TABLE_SIZE) * TWO_PI;
    let sum = 0;
    for (let k = 0; k < ratios.length; k++) {
      sum += Math.sin(p * ratios[k] + phases[k]);
    }
    table[i] = sum / ratios.length;
  }
  return table;
})();

function readMetal(phase: number): number {
  const p = phase - Math.floor(phase / METAL_TABLE_SIZE) * METAL_TABLE_SIZE;
  const i0 = p | 0;
  const i1 = (i0 + 1) % METAL_TABLE_SIZE;
  const f = p - i0;
  return METAL_TABLE[i0] * (1 - f) + METAL_TABLE[i1] * f;
}

/* ------------------------------------------------------------------ *
 * Voice
 * ------------------------------------------------------------------ */

class Voice {
  active = false;
  muted = false;
  padIndex = 0;
  bus = 0;
  family: Family = 'kick';
  model = 0;
  t = 0;
  len = 0.3;
  velocity = 0.8;
  velGain = 1;
  pitchRatio = 1;
  srcPos = 0;
  srcInc = 1;
  srcMix = 0;
  modelMix = 0;
  gain = 1;
  panL = Math.SQRT1_2;
  panR = Math.SQRT1_2;
  width = 1;
  impact = 0;
  warp = 0;
  superAmount = 0;
  toneShift = 0;
  maxLife = 1;
  aliveFrames = 0;
  chokeGain = 1;
  choking = false;
  killRate = 0;
  hasSample = false;
  sample: EngineSample | null = null;
  sampleRate = 48000;
  seed = 1;
  pitchOffset = 0;

  // oscillator / noise state
  kickPhase = 0;
  tone1Phase = 0;
  tone2Phase = 0;
  metalPhase = 0;
  warpLowL = 0;
  warpLowR = 0;
  warpHighL = 0;
  warpHighR = 0;

  // filters
  readonly toneL = new Biquad('peaking', 900, 48000, 1.1, 0);
  readonly toneR = new Biquad('peaking', 900, 48000, 1.1, 0);
  readonly colorL = new Biquad('bandpass', 2000, 48000, 1);
  readonly colorR = new Biquad('bandpass', 2000, 48000, 1);
  readonly clickL = new Biquad('highpass', 1800, 48000, 0.7);
  readonly clickR = new Biquad('highpass', 1800, 48000, 0.7);
  readonly eqLowL = new Biquad('lowshelf', 120, 48000, 0.7, 0);
  readonly eqLowR = new Biquad('lowshelf', 120, 48000, 0.7, 0);
  readonly eqMidL = new Biquad('peaking', 1100, 48000, 0.8, 0);
  readonly eqMidR = new Biquad('peaking', 1100, 48000, 0.8, 0);
  readonly eqHighL = new Biquad('highshelf', 6800, 48000, 0.7, 0);
  readonly eqHighR = new Biquad('highshelf', 6800, 48000, 0.7, 0);

  // scratch values produced by the model renderer each sample
  private mL = 0;
  private mR = 0;

  /** Fast xorshift noise in -1..1. */
  private noise(): number {
    let x = this.seed;
    x ^= x << 13;
    x ^= x >>> 17;
    x ^= x << 5;
    this.seed = x >>> 0;
    return (this.seed / 2147483648) - 1;
  }

  private resetFilters(sr: number): void {
    this.toneL.reset(); this.toneR.reset();
    this.colorL.reset(); this.colorR.reset();
    this.clickL.reset(); this.clickR.reset();
    this.eqLowL.reset(); this.eqLowR.reset();
    this.eqMidL.reset(); this.eqMidR.reset();
    this.eqHighL.reset(); this.eqHighR.reset();
    // Re-create coefficients at this sample rate.
    this.toneL.configure('peaking', 900, sr, 1.1, 0);
    this.toneR.configure('peaking', 900, sr, 1.1, 0);
    this.colorL.configure('bandpass', 2200, sr, 1);
    this.colorR.configure('bandpass', 2200, sr, 1);
    this.clickL.configure('highpass', 1800, sr, 0.7);
    this.clickR.configure('highpass', 1800, sr, 0.7);
    this.eqLowL.configure('lowshelf', 120, sr, 0.7, 0);
    this.eqLowR.configure('lowshelf', 120, sr, 0.7, 0);
    this.eqMidL.configure('peaking', 1100, sr, 0.8, 0);
    this.eqMidR.configure('peaking', 1100, sr, 0.8, 0);
    this.eqHighL.configure('highshelf', 6800, sr, 0.7, 0);
    this.eqHighR.configure('highshelf', 6800, sr, 0.7, 0);
  }

  reset(
    pad: PadParams,
    global: GlobalParams,
    velocity: number,
    seed: number,
    sample: EngineSample | null,
    sampleRate: number,
  ): void {
    this.active = true;
    this.muted = false;
    this.padIndex = pad.id;
    this.bus = clamp(pad.bus - 1, 0, BUS_COUNT - 1) | 0;
    this.family = pad.family;
    this.model = clamp(pad.model | 0, 0, 3);
    this.t = 0;
    this.aliveFrames = 0;
    this.chokeGain = 1;
    this.choking = false;
    this.killRate = 0;
    this.sample = sample;
    this.sampleRate = sampleRate;
    this.seed = seed >>> 0 || 0x1234567;

    const inst = clamp(pad.instability, 0, 1);
    const v = clamp(velocity, 0, 1);
    this.velocity = v;
    const r = () => this.noise();

    const velAmt = clamp(pad.velocityAmount, 0, 1);
    this.velGain = lerp(1, Math.pow(v, 1.35), velAmt);
    const velocityLength = 1 + (v - 0.5) * 0.38 * velAmt;
    this.len = clamp(pad.length * global.length * velocityLength, 0.012, 4);

    const cents = pad.pitch + global.transpose + global.fineTune / 100;
    const pitchJitter = r() * inst * 0.55; // semitones
    this.pitchRatio = Math.pow(2, (cents + pitchJitter) / 12);

    this.impact = clamp(pad.impact, 0, 1);
    this.warp = clamp(pad.warp, 0, 1);
    this.superAmount = clamp(pad.super, 0, 1);
    this.width = 0.35 + clamp(global.width, 0, 1) * 1.3;
    this.toneShift = clamp(pad.toneShift + (v - 0.5) * 9 * velAmt, -24, 24);

    const mixAmount = clamp(pad.drumify * (global.drumify / 100), 0, 1);
    const originalGain = dbToGain(pad.originalGain);
    const drumifiedGain = dbToGain(pad.drumifiedGain);
    const modelTrim = MODEL_TRIM[pad.family][clamp(pad.model | 0, 0, 3)];
    this.hasSample = Boolean(sample && sample.left.length > 1);
    if (!this.hasSample) {
      this.srcMix = 0;
      this.modelMix = drumifiedGain * FAMILY_GAIN[pad.family] * modelTrim;
    } else {
      this.srcMix = Math.cos(mixAmount * HALF_PI) * originalGain * 0.92;
      this.modelMix = Math.sin(mixAmount * HALF_PI) * drumifiedGain * FAMILY_GAIN[pad.family] * modelTrim;
    }

    if (this.hasSample && sample) {
      const start = clamp(pad.sampleStart + r() * 0.003 * inst, 0, Math.max(0, sample.left.length / sample.sampleRate - 0.002));
      this.srcPos = start * sample.sampleRate;
      this.srcInc = this.pitchRatio * (sample.sampleRate / sampleRate);
    }

    const pan = clamp(pad.pan + r() * 0.16 * inst, -1, 1);
    const [pl, pr] = panGains(pan);
    this.panL = pl;
    this.panR = pr;
    this.gain = dbToGain(pad.volume + r() * 1.4 * inst);

    this.maxLife = clamp(
      this.len * (pad.family === 'kick' ? 5.4 : pad.family === 'hat' ? 5.2 : pad.family === 'clap' ? 3.6 : 3.6) + 0.06,
      0.08,
      6,
    );

    // --- model-specific filter setup ---------------------------------
    this.pitchOffset = pitchJitter;
    this.tone1Phase = 0;
    this.tone2Phase = 0;
    this.kickPhase = 0;
    this.metalPhase = 0;
    this.warpLowL = 0; this.warpLowR = 0;
    this.warpHighL = 0; this.warpHighR = 0;
    this.resetFilters(sampleRate);

    const toneCenter = clamp(880 * Math.pow(2, this.toneShift / 12), 90, 13000);
    const toneGain = this.toneShift === 0 ? 0 : 4.5 * Math.sign(this.toneShift) * Math.min(1, Math.abs(this.toneShift) / 4);
    this.toneL.configure('peaking', toneCenter, sampleRate, 1.15, toneGain);
    this.toneR.configure('peaking', toneCenter, sampleRate, 1.15, toneGain);

    this.eqLowL.configure('lowshelf', 120, sampleRate, 0.7, pad.eqLow);
    this.eqLowR.configure('lowshelf', 120, sampleRate, 0.7, pad.eqLow);
    this.eqMidL.configure('peaking', 1200, sampleRate, 0.8, pad.eqMid);
    this.eqMidR.configure('peaking', 1200, sampleRate, 0.8, pad.eqMid);
    this.eqHighL.configure('highshelf', 7000, sampleRate, 0.7, pad.eqHigh);
    this.eqHighR.configure('highshelf', 7000, sampleRate, 0.7, pad.eqHigh);

    switch (this.family) {
      case 'kick': {
        this.clickL.configure('highpass', 1500, sampleRate, 0.7);
        this.clickR.configure('highpass', 1500, sampleRate, 0.7);
        break;
      }
      case 'snare': {
        const m = SNARE_MODELS[this.model];
        this.colorL.configure('bandpass', m.center, sampleRate, m.q);
        this.colorR.configure('bandpass', m.center, sampleRate, m.q);
        this.clickL.configure('highpass', 2500, sampleRate, 0.7);
        this.clickR.configure('highpass', 2500, sampleRate, 0.7);
        break;
      }
      case 'hat': {
        const m = HAT_MODELS[this.model];
        this.colorL.configure('highpass', m.hp, sampleRate, 0.7);
        this.colorR.configure('highpass', m.hp, sampleRate, 0.7);
        break;
      }
      case 'clap': {
        const m = CLAP_MODELS[this.model];
        this.colorL.configure('bandpass', m.center, sampleRate, m.q);
        this.colorR.configure('bandpass', m.center, sampleRate, m.q);
        break;
      }
    }
  }

  choke(fast = false): void {
    if (!this.active) return;
    this.choking = true;
    this.killRate = 1 / (0.012 * this.sampleRate);
    if (fast) this.killRate = 1 / (0.004 * this.sampleRate);
  }

  private renderModel(): void {
    const t = this.t;
    const sr = this.sampleRate;
    const len = this.len;
    const ratio = this.pitchRatio;
    const nL = this.noise();
    const nR = this.noise();
    let mL = 0;
    let mR = 0;

    switch (this.family) {
      case 'kick': {
        const m = KICK_MODELS[this.model];
        const f = (m.fEnd + (m.fStart - m.fEnd) * Math.exp(-t / m.pitchTau)) * ratio;
        this.kickPhase += (TWO_PI * f) / sr;
        if (this.kickPhase > TWO_PI * 64) this.kickPhase %= TWO_PI * 4;
        const tau = Math.max(0.008, len * m.bodyTauMul);
        const env = Math.exp(-t / tau);
        let body = Math.sin(this.kickPhase) * env;
        body += Math.sin(this.kickPhase * 2 + 0.3) * env * 0.09;
        body += Math.sin(this.kickPhase * 3.02) * env * 0.04;
        const sub = Math.sin(this.kickPhase * 0.5) * Math.exp(-t / (tau * 1.6)) * m.sub;
        const clickAmt = m.click + this.impact * 0.55;
        const click = this.clickL.process(nL) * Math.exp(-t / 0.0028) * clickAmt;
        const click2 = this.clickR.process(nR) * Math.exp(-t / 0.0028) * clickAmt;
        mL = Math.tanh((body + sub + click) * m.drive) / Math.tanh(m.drive);
        mR = Math.tanh((body + sub + click2) * m.drive) / Math.tanh(m.drive);
        break;
      }
      case 'snare': {
        const m = SNARE_MODELS[this.model];
        const noiseTau = Math.max(0.008, len * m.noiseTauMul);
        const bodyTau = Math.max(0.006, len * m.bodyTauMul);
        const noiseEnv = Math.exp(-t / noiseTau);
        const bodyEnv = Math.exp(-t / bodyTau);
        const attack = m.softAttack > 0 ? smoothstep(0, m.softAttack, t) : 1;
        const noiseL = this.colorL.process(nL) * 2.6 * m.noiseMix * noiseEnv * attack;
        const noiseR = this.colorR.process(nR) * 2.6 * m.noiseMix * noiseEnv * attack;
        this.tone1Phase += (TWO_PI * m.tone1 * ratio) / sr;
        this.tone2Phase += (TWO_PI * m.tone2 * ratio) / sr;
        const body =
          (Math.sin(this.tone1Phase) * 0.7 + Math.sin(this.tone2Phase) * 0.42) *
          bodyEnv * m.toneMix * attack;
        const snapAmt = m.snap + this.impact * 0.45;
        const snap = Math.exp(-t / 0.0045) * snapAmt;
        const snapL = this.clickL.process(nL) * snap;
        const snapR = this.clickR.process(nR) * snap;
        const width = this.superAmount * 0.45;
        const center = body * 0.6 + snapL * 0.26;
        mL = noiseL + center + (noiseR - noiseL) * width * 0.5 + snapL * 0.35;
        mR = noiseR + center + (noiseL - noiseR) * width * 0.5 + snapR * 0.35;
        break;
      }
      case 'hat': {
        const m = HAT_MODELS[this.model];
        const tau = Math.max(0.006, len * 0.45 * m.decayMul);
        const env = Math.exp(-t / tau) * Math.min(1, t / 0.0007 + 0.15);
        this.metalPhase += ((m.metalFreq * ratio) / sr) * METAL_TABLE_SIZE;
        const metal = readMetal(this.metalPhase);
        const noiseL = this.colorL.process(nL) * 2.2;
        const noiseR = this.colorR.process(nR) * 2.2;
        const width = m.noise * (0.35 + this.superAmount * 0.6);
        mL = (metal * m.metal + noiseL * m.noise + (noiseR - noiseL) * 0.18) * env;
        mR = (metal * m.metal + noiseR * m.noise + (noiseL - noiseR) * 0.18) * env;
        mL *= 1 + width * 0.15;
        mR *= 1 + width * 0.15;
        break;
      }
      case 'clap': {
        const m = CLAP_MODELS[this.model];
        const spacing = m.spacing * (0.9 + clamp(this.impact, 0, 1) * 0.15);
        let burst = 0;
        for (let b = 0; b < m.bursts; b++) {
          const bt = t - b * spacing;
          if (bt >= 0) burst += Math.exp(-bt / 0.0045);
        }
        burst = Math.min(1.7, burst);
        const tail = Math.exp(-t / Math.max(0.01, len * m.tailMul));
        const env = burst * 0.95 + tail * 0.24;
        const noiseL = this.colorL.process(nL) * 2.8 * env;
        const noiseR = this.colorR.process(nR) * 2.8 * env;
        const width = m.wide * (0.4 + this.superAmount * 0.4);
        mL = noiseL + (noiseR - noiseL) * width * 0.4;
        mR = noiseR + (noiseL - noiseR) * width * 0.4;
        break;
      }
    }

    this.mL = mL;
    this.mR = mR;
  }

  renderTo(outL: Float32Array, outR: Float32Array, frame: number): void {
    if (!this.active) return;
    const t = this.t;
    const sr = this.sampleRate;

    // ---- original sample layer -------------------------------------
    let srcL = 0;
    let srcR = 0;
    if (this.hasSample && this.sample && this.srcMix > 0.00001) {
      const sample = this.sample;
      const pos = this.srcPos;
      const i0 = pos | 0;
      if (i0 >= 0 && i0 + 1 < sample.left.length) {
        const f = pos - i0;
        const l = sample.left[i0] * (1 - f) + sample.left[i0 + 1] * f;
        const r = sample.right[i0] * (1 - f) + sample.right[i0 + 1] * f;
        const attack = Math.min(1, t / 0.0012);
        const decay = Math.exp(-t / Math.max(0.008, this.len * 0.62));
        const release = 1 - smoothstep(this.len * 0.95, this.len * 1.35, t);
        const env = attack * decay * release;
        srcL = l * env * this.srcMix;
        srcR = r * env * this.srcMix;
        this.srcPos += this.srcInc;
      }
    }

    // ---- model layer -----------------------------------------------
    let mL = 0;
    let mR = 0;
    if (this.modelMix > 0.00001) {
      this.renderModel();
      mL = this.mL * this.modelMix;
      mR = this.mR * this.modelMix;
    }

    let xL = srcL + mL;
    let xR = srcR + mR;

    // ---- impact transient shaping ---------------------------------
    if (this.impact > 0.001) {
      const boost = 1 + this.impact * 1.8 * Math.exp(-t / 0.0045);
      xL *= boost;
      xR *= boost;
    }

    // ---- warp: three-band dynamic reshaping -----------------------
    if (this.warp > 0.001) {
      const aLow = this.warpLowCoef;
      const aHigh = this.warpHighCoef;
      this.warpLowL += aLow * (xL - this.warpLowL);
      this.warpLowR += aLow * (xR - this.warpLowR);
      this.warpHighL += aHigh * (xL - this.warpHighL);
      this.warpHighR += aHigh * (xR - this.warpHighR);
      const lowL = this.warpLowL;
      const lowR = this.warpLowR;
      const highL = xL - this.warpHighL;
      const highR = xR - this.warpHighR;
      const midL = this.warpHighL - lowL;
      const midR = this.warpHighR - lowR;
      const age = clamp(t / Math.max(0.02, this.len * 1.5), 0, 1);
      const lowGain = 1 + this.warp * 0.95 * (1 - age);
      const highGain = 1 + this.warp * 1.05 * age;
      xL = lowL * lowGain + midL + highL * highGain;
      xR = lowR * lowGain + midR + highR * highGain;
    }

    // ---- tone shift + three-band EQ -------------------------------
    xL = this.eqLowL.process(this.toneL.process(xL));
    xR = this.eqLowR.process(this.toneR.process(xR));
    xL = this.eqMidL.process(xL);
    xR = this.eqMidR.process(xR);
    xL = this.eqHighL.process(xL);
    xR = this.eqHighR.process(xR);

    // ---- super: saturation and width ------------------------------
    if (this.superAmount > 0.001) {
      const drive = 1 + this.superAmount * 3.2;
      const norm = Math.tanh(drive);
      xL = Math.tanh(xL * drive) / norm;
      xR = Math.tanh(xR * drive) / norm;
      const mid = (xL + xR) * 0.5;
      const side = (xL - xR) * 0.5 * (1 + this.superAmount * 0.9);
      xL = mid + side;
      xR = mid - side;
    }

    // ---- global stereo width -------------------------------------
    if (Math.abs(this.width - 1) > 0.001) {
      const mid = (xL + xR) * 0.5;
      const side = (xL - xR) * 0.5 * this.width;
      xL = mid + side;
      xR = mid - side;
    }

    // ---- choke + mute + level -------------------------------------
    if (this.choking) {
      this.chokeGain -= this.killRate;
      if (this.chokeGain <= 0) {
        this.chokeGain = 0;
        this.active = false;
      }
    }
    if (!this.muted && this.chokeGain > 0) {
      const g = this.gain * this.velGain * this.chokeGain;
      outL[frame] += xL * g * this.panL;
      outR[frame] += xR * g * this.panR;
    }

    this.t += 1 / sr;
    this.aliveFrames++;
    if (this.t >= this.maxLife) this.active = false;
  }

  // one-pole coefficients, kept in a field so they follow the engine rate
  warpLowCoef = 1 - Math.exp((-TWO_PI * 200) / 48000);
  warpHighCoef = 1 - Math.exp((-TWO_PI * 4200) / 48000);

  setSampleRate(sr: number): void {
    this.sampleRate = sr;
    this.warpLowCoef = 1 - Math.exp((-TWO_PI * 200) / sr);
    this.warpHighCoef = 1 - Math.exp((-TWO_PI * 4200) / sr);
  }
}

/* ------------------------------------------------------------------ *
 * Engine
 * ------------------------------------------------------------------ */

interface PendingTrigger {
  pad: number;
  velocity: number;
  delayFrames: number;
}

export class VracEngine {
  readonly sampleRate: number;
  private pads: PadParams[] = createDefaultPads();
  private global: GlobalParams;
  private sample: EngineSample | null = null;
  private voices: Voice[] = [];
  private pending: PendingTrigger[] = [];
  private rng: Rng;
  private audible: boolean[] = new Array(PAD_COUNT).fill(true);
  readonly busMeters = new Float32Array(BUS_COUNT);
  meterDue = false;
  private blocks = 0;
  private anySolo = false;

  constructor(sampleRate: number, seed = 0x5eed1234) {
    this.sampleRate = sampleRate;
    this.rng = new Rng(seed);
    this.global = {
      fineTune: 0,
      drumify: 100,
      volume: 0,
      transpose: 0,
      length: 1,
      tone: 0,
      filter: 0,
      width: 0.65,
    };
    for (let i = 0; i < MAX_VOICES; i++) {
      const voice = new Voice();
      voice.setSampleRate(sampleRate);
      this.voices.push(voice);
    }
    this.refreshAudible();
  }

  setSample(left: Float32Array, right: Float32Array, sampleRate: number): void {
    const length = Math.min(left.length, right.length);
    if (length < 2) {
      this.sample = null;
      return;
    }
    this.sample = {
      left: left.length === length ? left : left.subarray(0, length),
      right: right.length === length ? right : right.subarray(0, length),
      sampleRate: sampleRate || this.sampleRate,
    };
  }

  setParams(params: EngineParams): void {
    if (params.pads.length >= PAD_COUNT) this.pads = params.pads;
    this.global = params.global;
    this.refreshAudible();
    for (const voice of this.voices) {
      if (voice.active) voice.muted = !this.audible[voice.padIndex];
    }
  }

  private refreshAudible(): void {
    this.anySolo = this.pads.some((p) => p.solo);
    for (let i = 0; i < PAD_COUNT; i++) {
      const pad = this.pads[i];
      this.audible[i] = !pad.mute && (!this.anySolo || pad.solo);
    }
  }

  trigger(padIndex: number, velocity = 0.85, delayFrames = 0): void {
    const index = padIndex | 0;
    if (index < 0 || index >= PAD_COUNT) return;
    if (!this.audible[index]) return;
    if (delayFrames > 0) {
      this.pending.push({ pad: index, velocity, delayFrames });
      return;
    }
    this.startVoice(index, velocity);
  }

  allOff(fast = true): void {
    this.pending.length = 0;
    for (const voice of this.voices) {
      if (voice.active) voice.choke(fast);
    }
  }

  /** Play a pad without touching the kit's stored state (A/B previews). */
  audition(pad: PadParams, velocity = 0.85): void {
    this.startVoice(clamp(pad.id | 0, 0, PAD_COUNT - 1), velocity, pad);
  }

  private startVoice(index: number, velocity: number, overridePad?: PadParams): void {
    const pad = overridePad ?? this.pads[index];
    if (!pad) return;
    if (pad.choke > 0) {
      for (const voice of this.voices) {
        if (voice.active && this.pads[voice.padIndex]?.choke === pad.choke) {
          voice.choke(false);
        }
      }
    }
    let slot = this.voices.find((v) => !v.active);
    if (!slot) {
      // steal the oldest voice
      slot = this.voices.reduce((oldest, v) => (v.t > oldest.t ? v : oldest), this.voices[0]);
    }
    const seed = Math.floor(this.rng.next() * 0xffffffff) + 1;
    slot.reset(pad, this.global, velocity, seed, this.sample, this.sampleRate);
    slot.muted = overridePad ? false : !this.audible[index];
  }

  process(outputs: Float32Array[][]): number {
    const out0 = outputs[0];
    if (!out0 || !out0[0]) return 0;
    const frames = out0[0].length;
    for (let b = 0; b < outputs.length; b++) {
      const bus = outputs[b];
      if (!bus) continue;
      bus[0]?.fill(0);
      bus[1]?.fill(0);
    }

    // Resolve delayed triggers into offsets inside this block.
    const scheduled: Array<{ offset: number; pad: number; velocity: number }> = [];
    if (this.pending.length > 0) {
      const still: PendingTrigger[] = [];
      for (const trigger of this.pending) {
        if (trigger.delayFrames >= frames) {
          trigger.delayFrames -= frames;
          still.push(trigger);
        } else {
          scheduled.push({
            offset: Math.max(0, trigger.delayFrames | 0),
            pad: trigger.pad,
            velocity: trigger.velocity,
          });
        }
      }
      this.pending = still;
    }
    scheduled.sort((a, b) => a.offset - b.offset);

    let cursor = 0;
    for (const event of scheduled) {
      const offset = Math.min(frames, Math.max(cursor, event.offset));
      if (offset > cursor) this.renderSegment(outputs, cursor, offset);
      this.startVoice(event.pad, event.velocity);
      cursor = offset;
    }
    if (cursor < frames) this.renderSegment(outputs, cursor, frames);

    // Metering.
    for (let b = 0; b < BUS_COUNT; b++) {
      const bus = outputs[b];
      if (!bus) continue;
      let peak = 0;
      const l = bus[0];
      const r = bus[1];
      for (let i = 0; i < frames; i++) {
        const a = Math.abs(l[i]);
        const c = Math.abs(r[i]);
        if (a > peak) peak = a;
        if (c > peak) peak = c;
      }
      this.busMeters[b] = Math.max(peak, this.busMeters[b] * 0.78);
    }
    this.blocks++;
    if (this.blocks % 4 === 0) this.meterDue = true;
    return frames;
  }

  private renderSegment(outputs: Float32Array[][], start: number, end: number): void {
    if (end <= start) return;
    for (const voice of this.voices) {
      if (!voice.active) continue;
      const bus = outputs[voice.bus];
      if (!bus || !bus[0] || !bus[1]) continue;
      for (let i = start; i < end; i++) {
        voice.renderTo(bus[0], bus[1], i);
        if (!voice.active) break;
      }
    }
  }

  get activeVoiceCount(): number {
    let n = 0;
    for (const voice of this.voices) if (voice.active) n++;
    return n;
  }

  getMeter(): MeterSnapshot {
    return {
      buses: Array.from(this.busMeters, (v) => Math.min(1, v)),
      voices: this.activeVoiceCount,
    };
  }

  consumeMeterDue(): boolean {
    if (!this.meterDue) return false;
    this.meterDue = false;
    return true;
  }
}
