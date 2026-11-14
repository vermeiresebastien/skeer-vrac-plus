/* ------------------------------------------------------------------ *
 * The SliceForge factory library.
 *
 * These are not recordings: every source is synthesised deterministically
 * from a seeded recipe of room hits, foley gestures and circuit noises.
 * The point is the same as the real library — a long file full of
 * contrasting transients that the slicer can cut into a kit.
 * ------------------------------------------------------------------ */

import { Biquad, TWO_PI } from './dsp';
import { Rng } from './rng';
import type { PeakData, SourceId, SourceSample } from '../types';
import { LIBRARY } from '../types';

export const SAMPLE_RATE = 48000;
const PEAK_BUCKETS = 6000;

function add(bufL: Float32Array, bufR: Float32Array, index: number, l: number, r: number): void {
  if (index < 0 || index >= bufL.length) return;
  bufL[index] += l;
  bufR[index] += r;
}

function panGains(pan: number): [number, number] {
  const angle = ((pan + 1) * Math.PI) / 4;
  return [Math.cos(angle), Math.sin(angle)];
}

interface Mode {
  freq: number;
  amp: number;
  decay: number;
  phase: number;
}

interface HitOptions {
  dur: number;
  modes: Mode[];
  click: number;
  clickDecay: number;
  noise?: number;
  noiseDecay?: number;
  noiseBand?: number;
  pan: number;
  gain: number;
  drive?: number;
  pitchSweep?: number;
}

function renderHit(
  bufL: Float32Array,
  bufR: Float32Array,
  sr: number,
  start: number,
  opt: HitOptions,
  rng: Rng,
): void {
  const offset = Math.floor(start * sr);
  const count = Math.min(Math.floor(opt.dur * sr), bufL.length - offset);
  if (count <= 0) return;
  const [pl, pr] = panGains(opt.pan);
  const clickFilter = new Biquad('highpass', 1800, sr, 0.8);
  const noiseFilter = new Biquad('bandpass', opt.noiseBand ?? 2400, sr, opt.noiseBand ? 1.1 : 0.8);
  const drive = opt.drive ?? 1;
  const modes = opt.modes.map((m) => ({ ...m, phase: rng.float(0, TWO_PI) }));
  const noiseAmt = opt.noise ?? Math.max(0, opt.click * 0.6);
  const noiseDecay = opt.noiseDecay ?? Math.max(0.004, opt.clickDecay * 2.2);
  const pitchSweep = opt.pitchSweep ?? 0;

  for (let s = 0; s < count; s++) {
    const t = s / sr;
    let sig = 0;
    for (let m = 0; m < modes.length; m++) {
      const mode = modes[m];
      const f = mode.freq * (1 + pitchSweep * Math.exp(-t / 0.02));
      sig += mode.amp * Math.sin(TWO_PI * f * t + mode.phase) * Math.exp(-t / mode.decay);
    }
    if (opt.click > 0) {
      const c = rng.noise() * opt.click * Math.exp(-t / opt.clickDecay);
      sig += clickFilter.process(c);
    }
    if (noiseAmt > 0) {
      const n = noiseFilter.process(rng.noise()) * noiseAmt * Math.exp(-t / noiseDecay);
      sig += n;
    }
    sig = Math.tanh(sig * drive) / Math.tanh(drive);
    add(bufL, bufR, offset + s, sig * pl * opt.gain, sig * pr * opt.gain);
  }
}

/* ------------------------------------------------------------------ *
 * Event recipes
 * ------------------------------------------------------------------ */

function thud(bufL: Float32Array, bufR: Float32Array, sr: number, t: number, rng: Rng, big = 1): void {
  const f = rng.float(55, 95);
  renderHit(bufL, bufR, sr, t, {
    dur: rng.float(0.35, 0.65) * big,
    modes: [
      { freq: f, amp: 0.9, decay: rng.float(0.13, 0.28) * big, phase: 0 },
      { freq: f * 1.98, amp: 0.22, decay: 0.07, phase: 0 },
      { freq: f * 3.1, amp: 0.1, decay: 0.04, phase: 0 },
    ],
    click: rng.float(0.06, 0.18),
    clickDecay: 0.006,
    noise: rng.float(0.03, 0.12),
    noiseDecay: 0.05,
    noiseBand: 420,
    pan: rng.float(-0.3, 0.3),
    gain: rng.float(0.42, 0.75) * big,
    drive: 1.4,
  }, rng);
}

function woodKnock(bufL: Float32Array, bufR: Float32Array, sr: number, t: number, rng: Rng): void {
  const base = rng.float(170, 320);
  renderHit(bufL, bufR, sr, t, {
    dur: rng.float(0.18, 0.4),
    modes: [
      { freq: base, amp: 0.7, decay: rng.float(0.035, 0.09), phase: 0 },
      { freq: base * 2.36, amp: 0.35, decay: rng.float(0.02, 0.05), phase: 0 },
      { freq: base * 4.1, amp: 0.15, decay: 0.012, phase: 0 },
    ],
    click: rng.float(0.25, 0.55),
    clickDecay: rng.float(0.0012, 0.003),
    noise: rng.float(0.1, 0.22),
    noiseDecay: 0.018,
    noiseBand: 1800,
    pan: rng.float(-0.5, 0.5),
    gain: rng.float(0.5, 0.85),
    drive: 1.25,
  }, rng);
}

function metalClank(bufL: Float32Array, bufR: Float32Array, sr: number, t: number, rng: Rng): void {
  const base = rng.float(380, 900);
  const ratios = [1, 1.41, 1.93, 2.71, 3.8, 5.2, 7.1];
  const modes: Mode[] = ratios.map((ratio, i) => ({
    freq: base * ratio * rng.float(0.985, 1.015),
    amp: rng.float(0.12, 0.5) / (1 + i * 0.32),
    decay: rng.float(0.08, 0.5) / (1 + i * 0.18),
    phase: 0,
  }));
  renderHit(bufL, bufR, sr, t, {
    dur: rng.float(0.55, 1.15),
    modes,
    click: rng.float(0.3, 0.6),
    clickDecay: 0.0025,
    noise: rng.float(0.06, 0.18),
    noiseDecay: 0.04,
    noiseBand: 6200,
    pan: rng.float(-0.55, 0.55),
    gain: rng.float(0.55, 0.95),
    drive: 1.15,
  }, rng);
}

function glassPing(bufL: Float32Array, bufR: Float32Array, sr: number, t: number, rng: Rng): void {
  const base = rng.float(1500, 3200);
  renderHit(bufL, bufR, sr, t, {
    dur: rng.float(0.3, 0.7),
    modes: [
      { freq: base, amp: 0.45, decay: rng.float(0.12, 0.3), phase: 0 },
      { freq: base * 2.04, amp: 0.28, decay: 0.09, phase: 0 },
      { freq: base * 3.13, amp: 0.16, decay: 0.05, phase: 0 },
      { freq: base * 5.4, amp: 0.08, decay: 0.03, phase: 0 },
    ],
    click: 0.15,
    clickDecay: 0.001,
    pan: rng.float(-0.7, 0.7),
    gain: rng.float(0.3, 0.55),
  }, rng);
}

function paperCrinkle(bufL: Float32Array, bufR: Float32Array, sr: number, t: number, rng: Rng): void {
  const offset = Math.floor(t * sr);
  const dur = rng.float(0.15, 0.33);
  const count = Math.floor(dur * sr);
  const [pl, pr] = panGains(rng.float(-0.6, 0.6));
  const filter = new Biquad('highpass', 1300, sr, 0.7);
  const amp = rng.float(0.16, 0.3);
  let grain = 0;
  let grainLeft = 0;
  let grainGain = 0;
  for (let s = 0; s < count; s++) {
    const tSec = s / sr;
    if (grainLeft <= 0) {
      grainLeft = Math.floor(rng.float(0.002, 0.012) * sr);
      grainGain = rng.float(0.1, 1);
      grain = rng.noise();
    }
    grainLeft--;
    const env = Math.sin((Math.PI * (s / count)) ** 0.6) * Math.exp(-tSec / (dur * 0.7));
    const sig = filter.process(rng.noise() * 0.35 + grain * grainGain) * amp * env;
    add(bufL, bufR, offset + s, sig * pl, sig * pr);
  }
}

function squeak(bufL: Float32Array, bufR: Float32Array, sr: number, t: number, rng: Rng): void {
  const offset = Math.floor(t * sr);
  const dur = rng.float(0.18, 0.32);
  const count = Math.floor(dur * sr);
  const [pl, pr] = panGains(rng.float(-0.55, 0.55));
  const startF = rng.float(700, 1300);
  const endF = startF * rng.float(1.4, 2.4);
  const amp = rng.float(0.1, 0.2);
  let phase = 0;
  for (let s = 0; s < count; s++) {
    const tt = s / count;
    const f = startF + (endF - startF) * tt + Math.sin(tt * TWO_PI * 17) * 18;
    phase += (TWO_PI * f) / sr;
    const env = Math.sin(Math.PI * Math.min(1, tt * 1.2)) * Math.pow(1 - tt, 1.4);
    const sig = (Math.sin(phase) * 0.8 + Math.sin(phase * 2.01) * 0.2) * amp * env;
    add(bufL, bufR, offset + s, sig * pl, sig * pr);
  }
}

function potHit(bufL: Float32Array, bufR: Float32Array, sr: number, t: number, rng: Rng): void {
  const base = rng.float(240, 520);
  const partials = [1, 1.52, 2.08, 2.91, 4.3];
  renderHit(bufL, bufR, sr, t, {
    dur: rng.float(0.35, 0.8),
    modes: partials.map((r, i) => ({
      freq: base * r * rng.float(0.99, 1.01),
      amp: rng.float(0.16, 0.5) / (1 + i * 0.35),
      decay: rng.float(0.12, 0.32) / (1 + i * 0.15),
      phase: 0,
    })),
    click: rng.float(0.18, 0.35),
    clickDecay: 0.002,
    noise: rng.float(0.1, 0.2),
    noiseDecay: 0.03,
    noiseBand: 3200,
    pan: rng.float(-0.45, 0.45),
    gain: rng.float(0.4, 0.8),
    drive: 1.05,
  }, rng);
}

function cutlery(bufL: Float32Array, bufR: Float32Array, sr: number, t: number, rng: Rng): void {
  const base = rng.float(900, 1800);
  renderHit(bufL, bufR, sr, t, {
    dur: rng.float(0.25, 0.55),
    modes: [1, 1.47, 2.3, 3.14, 4.41].map((r, i) => ({
      freq: base * r * rng.float(0.98, 1.02),
      amp: 0.32 / (1 + i * 0.5),
      decay: rng.float(0.06, 0.24) / (1 + i * 0.25),
      phase: 0,
    })),
    click: rng.float(0.1, 0.28),
    clickDecay: 0.0015,
    noise: 0.05,
    noiseDecay: 0.02,
    noiseBand: 7500,
    pan: rng.float(-0.8, 0.8),
    gain: rng.float(0.28, 0.55),
  }, rng);
}

function drawer(bufL: Float32Array, bufR: Float32Array, sr: number, t: number, rng: Rng): void {
  const offset = Math.floor(t * sr);
  const dur = rng.float(0.3, 0.55);
  const count = Math.floor(dur * sr);
  const [pl, pr] = panGains(rng.float(-0.4, 0.4));
  const filter = new Biquad('bandpass', rng.float(300, 600), sr, 0.9);
  let phase = 0;
  let thump = 0;
  for (let s = 0; s < count; s++) {
    const tt = s / count;
    const env = Math.exp(-tt * 5.5);
    const rumble = filter.process(rng.noise());
    phase += (TWO_PI * (78 - tt * 22)) / sr;
    thump = Math.sin(phase) * 0.45;
    const sig = (rumble * 0.5 + thump) * env * 0.55;
    add(bufL, bufR, offset + s, sig * pl, sig * pr);
  }
}

function bottle(bufL: Float32Array, bufR: Float32Array, sr: number, t: number, rng: Rng): void {
  const base = rng.float(310, 470);
  renderHit(bufL, bufR, sr, t, {
    dur: rng.float(0.35, 0.7),
    modes: [
      { freq: base, amp: 0.55, decay: rng.float(0.18, 0.4), phase: 0 },
      { freq: base * 2.9, amp: 0.2, decay: 0.08, phase: 0 },
      { freq: base * 4.7, amp: 0.1, decay: 0.04, phase: 0 },
    ],
    click: rng.float(0.3, 0.6),
    clickDecay: 0.002,
    noise: 0.08,
    noiseDecay: 0.02,
    noiseBand: 2200,
    pan: rng.float(-0.4, 0.4),
    gain: rng.float(0.32, 0.6),
  }, rng);
}

function chestThump(bufL: Float32Array, bufR: Float32Array, sr: number, t: number, rng: Rng): void {
  const f = rng.float(62, 82);
  renderHit(bufL, bufR, sr, t, {
    dur: rng.float(0.35, 0.7),
    modes: [
      { freq: f, amp: 0.9, decay: rng.float(0.12, 0.26), phase: 0 },
      { freq: f * 2.03, amp: 0.2, decay: 0.06, phase: 0 },
      { freq: 160, amp: 0.12, decay: 0.03, phase: 0 },
    ],
    click: rng.float(0.04, 0.12),
    clickDecay: 0.008,
    noise: rng.float(0.08, 0.2),
    noiseDecay: 0.06,
    noiseBand: 300,
    pan: rng.float(-0.15, 0.15),
    gain: rng.float(0.45, 0.8),
    drive: 1.35,
  }, rng);
}

function mouthPop(bufL: Float32Array, bufR: Float32Array, sr: number, t: number, rng: Rng): void {
  const base = rng.float(230, 420);
  renderHit(bufL, bufR, sr, t, {
    dur: rng.float(0.12, 0.24),
    modes: [
      { freq: base, amp: 0.6, decay: 0.022, phase: 0 },
      { freq: base * 1.6, amp: 0.3, decay: 0.015, phase: 0 },
    ],
    click: rng.float(0.4, 0.75),
    clickDecay: 0.001,
    noise: rng.float(0.18, 0.35),
    noiseDecay: 0.012,
    noiseBand: 1700,
    pan: rng.float(-0.3, 0.3),
    gain: rng.float(0.5, 0.85),
    drive: 1.6,
  }, rng);
}

function breath(bufL: Float32Array, bufR: Float32Array, sr: number, t: number, rng: Rng): void {
  const offset = Math.floor(t * sr);
  const dur = rng.float(0.35, 0.7);
  const count = Math.floor(dur * sr);
  const [pl, pr] = panGains(rng.float(-0.6, 0.6));
  const band = new Biquad('bandpass', rng.float(900, 1800), sr, 0.8);
  const air = new Biquad('highpass', 2600, sr, 0.7);
  for (let s = 0; s < count; s++) {
    const tt = s / count;
    const env = Math.pow(Math.sin(Math.PI * tt), 1.3) * (0.75 + 0.25 * Math.sin(tt * 28));
    const sig = (band.process(rng.noise()) * 0.5 + air.process(rng.noise()) * 0.22) * env * 0.32;
    add(bufL, bufR, offset + s, sig * pl, sig * pr);
  }
}

function vocalTone(bufL: Float32Array, bufR: Float32Array, sr: number, t: number, rng: Rng): void {
  const offset = Math.floor(t * sr);
  const dur = rng.float(0.25, 0.45);
  const count = Math.floor(dur * sr);
  const [pl, pr] = panGains(rng.float(-0.35, 0.35));
  const f0 = rng.float(96, 150);
  const formant1 = new Biquad('bandpass', rng.float(550, 900), sr, 2.2);
  const formant2 = new Biquad('bandpass', rng.float(1000, 1600), sr, 2.4);
  const fricative = new Biquad('highpass', 2200, sr, 0.7);
  let phase = 0;
  for (let s = 0; s < count; s++) {
    const tt = s / count;
    phase += (TWO_PI * f0 * (1 + (tt - 0.5) * 0.06)) / sr;
    const glottal = Math.sin(phase) + 0.45 * Math.sin(phase * 2) + 0.2 * Math.sin(phase * 3);
    const env = Math.exp(-tt * 2.6) * Math.min(1, tt * 14);
    const voiced = formant1.process(glottal) * 0.5 + formant2.process(glottal) * 0.28;
    const noise = fricative.process(rng.noise()) * 0.16;
    const sig = (voiced + noise) * env * 0.6;
    add(bufL, bufR, offset + s, sig * pl, sig * pr);
  }
}

function scrape(bufL: Float32Array, bufR: Float32Array, sr: number, t: number, rng: Rng): void {
  const offset = Math.floor(t * sr);
  const dur = rng.float(0.45, 0.85);
  const count = Math.floor(dur * sr);
  const [pl, pr] = panGains(rng.float(-0.75, 0.75));
  const bp = new Biquad('bandpass', rng.float(1100, 2400), sr, 0.7);
  const hp = new Biquad('highpass', 900, sr, 0.7);
  let grainLeft = 0;
  let grainGain = 0;
  for (let s = 0; s < count; s++) {
    const tt = s / count;
    if (grainLeft <= 0) {
      grainLeft = Math.floor(rng.float(0.004, 0.02) * sr);
      grainGain = rng.float(0.35, 1);
    }
    grainLeft--;
    const env = Math.sin(Math.PI * Math.min(1, tt * 1.6)) * Math.pow(1 - tt, 0.7);
    const sig = bp.process(rng.noise() * 0.7 + hp.process(rng.noise()) * 0.35) * grainGain * env * 0.16;
    add(bufL, bufR, offset + s, sig * pl, sig * pr);
  }
}

function chainRattle(bufL: Float32Array, bufR: Float32Array, sr: number, t: number, rng: Rng): void {
  for (let i = 0; i < rng.int(3, 6); i++) {
    const offsetT = t + i * rng.float(0.025, 0.06);
    renderHit(bufL, bufR, sr, offsetT, {
      dur: rng.float(0.1, 0.22),
      modes: [
        { freq: rng.float(900, 1900), amp: 0.35, decay: rng.float(0.03, 0.09), phase: 0 },
        { freq: rng.float(2100, 3600), amp: 0.18, decay: 0.035, phase: 0 },
      ],
      click: rng.float(0.1, 0.25),
      clickDecay: 0.0015,
      noise: 0.06,
      noiseDecay: 0.02,
      noiseBand: 5000,
      pan: rng.float(-0.8, 0.8),
      gain: rng.float(0.16, 0.36),
    }, rng);
  }
}

function circuitBlip(bufL: Float32Array, bufR: Float32Array, sr: number, t: number, rng: Rng): void {
  const offset = Math.floor(t * sr);
  const dur = rng.float(0.06, 0.22);
  const count = Math.floor(dur * sr);
  const [pl, pr] = panGains(rng.float(-0.7, 0.7));
  const f = rng.float(420, 2200);
  const target = f * rng.float(0.4, 2.4);
  const square = rng.chance(0.45);
  const amp = rng.float(0.2, 0.42);
  let phase = 0;
  for (let s = 0; s < count; s++) {
    const tt = s / count;
    phase += (TWO_PI * (f + (target - f) * tt)) / sr;
    const env = Math.min(1, tt * 90) * Math.exp(-tt * 3.2);
    const wave = square ? Math.tanh(Math.sin(phase) * 4) * 0.7 : Math.sin(phase);
    add(bufL, bufR, offset + s, wave * env * amp * pl, wave * env * amp * pr);
  }
}

function relayClick(bufL: Float32Array, bufR: Float32Array, sr: number, t: number, rng: Rng): void {
  renderHit(bufL, bufR, sr, t, {
    dur: rng.float(0.05, 0.13),
    modes: [
      { freq: rng.float(500, 1200), amp: 0.3, decay: 0.008, phase: 0 },
      { freq: rng.float(1800, 3400), amp: 0.14, decay: 0.004, phase: 0 },
    ],
    click: rng.float(0.4, 0.8),
    clickDecay: 0.0008,
    noise: rng.float(0.15, 0.3),
    noiseDecay: 0.006,
    noiseBand: 4200,
    pan: rng.float(-0.7, 0.7),
    gain: rng.float(0.35, 0.65),
    drive: 2,
  }, rng);
}

function zap(bufL: Float32Array, bufR: Float32Array, sr: number, t: number, rng: Rng): void {
  const offset = Math.floor(t * sr);
  const dur = rng.float(0.12, 0.3);
  const count = Math.floor(dur * sr);
  const [pl, pr] = panGains(rng.float(-0.6, 0.6));
  const hp = new Biquad('highpass', 1800, sr, 0.9);
  const amp = rng.float(0.2, 0.38);
  for (let s = 0; s < count; s++) {
    const tt = s / count;
    const crackle = rng.chance(0.35) ? rng.noise() : 0;
    const env = Math.pow(1 - tt, 2) * Math.exp(-tt * 1.8);
    const sig = hp.process(rng.noise() + crackle * 0.7) * amp * env;
    add(bufL, bufR, offset + s, sig * pl, sig * pr);
  }
}

function vinylPop(bufL: Float32Array, bufR: Float32Array, sr: number, t: number, rng: Rng): void {
  renderHit(bufL, bufR, sr, t, {
    dur: rng.float(0.04, 0.12),
    modes: [
      { freq: rng.float(160, 420), amp: 0.4, decay: rng.float(0.008, 0.025), phase: 0 },
      { freq: rng.float(900, 2100), amp: 0.15, decay: 0.006, phase: 0 },
    ],
    click: rng.float(0.3, 0.65),
    clickDecay: 0.0008,
    noise: rng.float(0.2, 0.45),
    noiseDecay: 0.008,
    noiseBand: 3000,
    pan: rng.float(-0.5, 0.5),
    gain: rng.float(0.3, 0.6),
  }, rng);
}

/* ------------------------------------------------------------------ *
 * Room, bed and normalisation
 * ------------------------------------------------------------------ */

function applyRoom(L: Float32Array, R: Float32Array, sr: number, wet: number): void {
  if (wet <= 0) return;
  const taps = [0.031, 0.043, 0.067, 0.089];
  const buffers = taps.map((d) => ({
    buf: new Float32Array(Math.floor(d * sr)),
    idx: 0,
    damp: 0,
  }));
  const nearL = new Float32Array(L.length);
  const nearR = new Float32Array(R.length);
  for (let i = 0; i < L.length; i++) {
    const mono = (L[i] + R[i]) * 0.5;
    let accL = 0;
    let accR = 0;
    for (let k = 0; k < buffers.length; k++) {
      const b = buffers[k];
      const out = b.buf[b.idx];
      b.damp = b.damp * 0.62 + out * 0.38;
      b.buf[b.idx] = mono + b.damp * 0.24;
      b.idx = (b.idx + 1) % b.buf.length;
      if (k < 3) accL += out;
      else accR += out;
      if (k >= 2) accR += out * 0.6;
    }
    nearL[i] = accL;
    nearR[i] = accR;
  }
  for (let i = 0; i < L.length; i++) {
    L[i] += nearL[i] * wet * 0.28;
    R[i] += nearR[i] * wet * 0.28;
  }
}

function addVinylBed(L: Float32Array, R: Float32Array, sr: number, rng: Rng): void {
  const lpL = new Biquad('lowpass', 5200, sr, 0.6);
  const lpR = new Biquad('lowpass', 5200, sr, 0.6);
  const hpL = new Biquad('highpass', 180, sr, 0.7);
  const hpR = new Biquad('highpass', 180, sr, 0.7);
  const len = L.length;
  const fade = Math.min(sr * 0.4, len * 0.1);
  for (let i = 0; i < len; i++) {
    const env = Math.min(1, i / fade) * Math.min(1, (len - i) / fade);
    const hissL = lpL.process(rng.noise()) * 0.014 + hpL.process(rng.noise()) * 0.005;
    const hissR = lpR.process(rng.noise()) * 0.014 + hpR.process(rng.noise()) * 0.005;
    const wow = Math.sin((TWO_PI * i) / (sr * 0.77)) * 0.0015;
    L[i] += (hissL + wow) * env;
    R[i] += (hissR - wow) * env;
  }
}

function normalize(L: Float32Array, R: Float32Array, target = 0.88): void {
  let peak = 0;
  for (let i = 0; i < L.length; i++) {
    const a = Math.abs(L[i]);
    const b = Math.abs(R[i]);
    if (a > peak) peak = a;
    if (b > peak) peak = b;
  }
  if (peak < 1e-8) return;
  const gain = target / peak;
  for (let i = 0; i < L.length; i++) {
    L[i] = Math.tanh(L[i] * gain * 1.02) * 0.98;
    R[i] = Math.tanh(R[i] * gain * 1.02) * 0.98;
  }
}

function fadeEdges(L: Float32Array, R: Float32Array, sr: number, inSec = 0.002, outSec = 0.05): void {
  const inN = Math.floor(inSec * sr);
  const outN = Math.floor(outSec * sr);
  for (let i = 0; i < inN && i < L.length; i++) {
    const g = i / inN;
    L[i] *= g;
    R[i] *= g;
  }
  for (let i = 0; i < outN && i < L.length; i++) {
    const g = i / outN;
    L[L.length - 1 - i] *= g;
    R[R.length - 1 - i] *= g;
  }
}

/* ------------------------------------------------------------------ *
 * Source recipes
 * ------------------------------------------------------------------ */

type EventKind =
  | 'wood'
  | 'metal'
  | 'thud'
  | 'glass'
  | 'paper'
  | 'squeak'
  | 'pot'
  | 'cutlery'
  | 'drawer'
  | 'bottle'
  | 'thump'
  | 'pop'
  | 'breath'
  | 'vocal'
  | 'scrape'
  | 'chain'
  | 'blip'
  | 'relay'
  | 'zap'
  | 'vinylPop';

const RECIPES: Record<SourceId, { seed: number; step: number; wet: number; events: EventKind[] }> = {
  foley: {
    seed: 0x51a7c0de,
    step: 0.64,
    wet: 0.6,
    events: ['wood', 'metal', 'thud', 'squeak', 'metal', 'paper', 'glass', 'wood', 'metal', 'thud', 'squeak', 'glass', 'wood', 'metal', 'paper', 'squeak', 'thud', 'metal', 'wood', 'glass', 'metal', 'thud', 'squeak'],
  },
  kitchen: {
    seed: 0x33b19a7e,
    step: 0.58,
    wet: 0.45,
    events: ['pot', 'cutlery', 'glass', 'drawer', 'bottle', 'pot', 'cutlery', 'glass', 'cutlery', 'bottle', 'pot', 'drawer', 'glass', 'cutlery', 'bottle', 'pot', 'cutlery', 'glass', 'drawer', 'bottle', 'pot', 'cutlery'],
  },
  body: {
    seed: 0x7e55c0a1,
    step: 0.55,
    wet: 0.25,
    events: ['thump', 'pop', 'breath', 'vocal', 'pop', 'thump', 'breath', 'pop', 'vocal', 'thump', 'breath', 'pop', 'vocal', 'breath', 'thump', 'pop', 'breath', 'vocal', 'thump', 'pop', 'breath'],
  },
  scrap: {
    seed: 0x0badf00d,
    step: 0.63,
    wet: 0.7,
    events: ['metal', 'scrape', 'chain', 'metal', 'thud', 'scrape', 'metal', 'chain', 'scrape', 'metal', 'thud', 'scrape', 'chain', 'metal', 'scrape', 'metal', 'chain', 'thud', 'scrape', 'metal', 'chain', 'scrape'],
  },
  circuit: {
    seed: 0xc1c01977,
    step: 0.47,
    wet: 0.35,
    events: ['blip', 'relay', 'zap', 'blip', 'relay', 'blip', 'zap', 'relay', 'blip', 'zap', 'relay', 'blip', 'zap', 'relay', 'blip', 'zap', 'relay', 'blip', 'zap', 'relay', 'blip', 'zap'],
  },
  vinyl: {
    seed: 0xdeadbeef,
    step: 0.6,
    wet: 0.3,
    events: ['vinylPop', 'wood', 'vinylPop', 'glass', 'vinylPop', 'thud', 'vinylPop', 'vinylPop', 'glass', 'vinylPop', 'metal', 'vinylPop', 'vinylPop', 'wood', 'vinylPop', 'glass', 'vinylPop', 'metal', 'vinylPop', 'thud', 'vinylPop', 'vinylPop'],
  },
};

function renderEvent(
  kind: EventKind,
  L: Float32Array,
  R: Float32Array,
  sr: number,
  t: number,
  rng: Rng,
): void {
  switch (kind) {
    case 'wood': woodKnock(L, R, sr, t, rng); break;
    case 'metal': metalClank(L, R, sr, t, rng); break;
    case 'thud': thud(L, R, sr, t, rng); break;
    case 'glass': glassPing(L, R, sr, t, rng); break;
    case 'paper': paperCrinkle(L, R, sr, t, rng); break;
    case 'squeak': squeak(L, R, sr, t, rng); break;
    case 'pot': potHit(L, R, sr, t, rng); break;
    case 'cutlery': cutlery(L, R, sr, t, rng); break;
    case 'drawer': drawer(L, R, sr, t, rng); break;
    case 'bottle': bottle(L, R, sr, t, rng); break;
    case 'thump': chestThump(L, R, sr, t, rng); break;
    case 'pop': mouthPop(L, R, sr, t, rng); break;
    case 'breath': breath(L, R, sr, t, rng); break;
    case 'vocal': vocalTone(L, R, sr, t, rng); break;
    case 'scrape': scrape(L, R, sr, t, rng); break;
    case 'chain': chainRattle(L, R, sr, t, rng); break;
    case 'blip': circuitBlip(L, R, sr, t, rng); break;
    case 'relay': relayClick(L, R, sr, t, rng); break;
    case 'zap': zap(L, R, sr, t, rng); break;
    case 'vinylPop': vinylPop(L, R, sr, t, rng); break;
  }
}

export function buildPeaks(L: Float32Array, R: Float32Array, count = PEAK_BUCKETS): PeakData {
  const buckets = Math.max(1, Math.min(count, L.length));
  const minMax = new Float32Array(buckets * 2);
  const per = L.length / buckets;
  for (let b = 0; b < buckets; b++) {
    const start = Math.floor(b * per);
    const end = Math.min(L.length, Math.floor((b + 1) * per));
    let lo = 0;
    let hi = 0;
    for (let i = start; i < end; i++) {
      const v = (L[i] + R[i]) * 0.5;
      if (v < lo) lo = v;
      if (v > hi) hi = v;
    }
    minMax[b * 2] = lo;
    minMax[b * 2 + 1] = hi;
  }
  return { minMax, count: buckets };
}

const cache = new Map<SourceId, SourceSample>();

export function generateLibrarySample(id: SourceId): SourceSample {
  const cached = cache.get(id);
  if (cached) return cached;

  const info = LIBRARY.find((s) => s.id === id) ?? LIBRARY[0];
  const sr = SAMPLE_RATE;
  const duration = info.duration;
  const total = Math.floor(duration * sr);
  const L = new Float32Array(total);
  const R = new Float32Array(total);
  const recipe = RECIPES[id];
  const rng = new Rng(recipe.seed);

  const usable = duration - 1.1;
  const eventCount = Math.floor(usable / recipe.step);
  for (let i = 0; i < eventCount; i++) {
    const base = 0.22 + i * recipe.step;
    const jitter = rng.float(-0.045, 0.045);
    const t = Math.max(0.05, base + jitter);
    const kind = recipe.events[i % recipe.events.length];
    renderEvent(kind, L, R, sr, t, rng);
    if (rng.chance(0.08)) {
      renderEvent(kind, L, R, sr, t + rng.float(0.18, 0.28), rng);
    }
  }

  if (id === 'vinyl') addVinylBed(L, R, sr, rng);
  applyRoom(L, R, sr, recipe.wet);
  normalize(L, R);
  fadeEdges(L, R, sr, 0.003, 0.08);

  const sample: SourceSample = {
    id,
    name: info.name,
    blurb: info.blurb,
    duration,
    sampleRate: sr,
    left: L,
    right: R,
    peaks: buildPeaks(L, R),
    isUser: false,
  };
  cache.set(id, sample);
  return sample;
}

/** Build a SourceSample around already-decoded audio (dropped file or recording). */
export function makeUserSample(
  name: string,
  left: Float32Array,
  right: Float32Array,
  sampleRate: number,
): SourceSample {
  const L = left.slice();
  const R = right.slice();
  let peak = 0;
  for (let i = 0; i < L.length; i++) {
    peak = Math.max(peak, Math.abs(L[i]), Math.abs(R[i]));
  }
  if (peak > 0.0001 && peak < 0.999) {
    const gain = 0.94 / peak;
    for (let i = 0; i < L.length; i++) {
      L[i] *= gain;
      R[i] *= gain;
    }
  }
  return {
    id: `user-${Date.now()}`,
    name: name.replace(/\.[^.]+$/, '').toUpperCase() || 'MY SAMPLE',
    blurb: 'Your own audio, trimmed and sliced into a kit.',
    duration: L.length / sampleRate,
    sampleRate,
    left: L,
    right: R,
    peaks: buildPeaks(L, R),
    isUser: true,
  };
}
