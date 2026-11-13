"use strict";
(() => {
  // src/types.ts
  var PAD_NOTES = [
    "C1",
    "C#1",
    "D1",
    "D#1",
    "E1",
    "F1",
    "F#1",
    "G1",
    "G#1",
    "A1",
    "A#1",
    "B1",
    "C2",
    "C#2",
    "D2",
    "D#2"
  ];
  var PAD_MIDI_NOTES = PAD_NOTES.map((_, i) => 36 + i);
  var PAD_COUNT = 16;
  var FAMILY_LENGTH = {
    kick: 0.42,
    snare: 0.24,
    hat: 0.13,
    clap: 0.3
  };
  var KIT_LAYOUT = [
    { family: "kick", model: 0, drumify: 0.72, choke: 1 },
    { family: "kick", model: 1, drumify: 0.58, choke: 1 },
    { family: "snare", model: 0, drumify: 0.68, choke: 0 },
    { family: "clap", model: 0, drumify: 0.62, choke: 0 },
    { family: "snare", model: 1, drumify: 0.76, choke: 0 },
    { family: "snare", model: 2, drumify: 0.48, choke: 0 },
    { family: "hat", model: 0, drumify: 0.8, choke: 2 },
    { family: "hat", model: 0, drumify: 0.64, choke: 2 },
    { family: "hat", model: 1, drumify: 0.85, choke: 2 },
    { family: "hat", model: 2, drumify: 0.55, choke: 2 },
    { family: "clap", model: 3, drumify: 0.7, choke: 0 },
    { family: "clap", model: 2, drumify: 0.52, choke: 0 },
    { family: "hat", model: 3, drumify: 0.6, choke: 2 },
    { family: "hat", model: 0, drumify: 0.9, choke: 2 },
    { family: "snare", model: 3, drumify: 0.66, choke: 0 },
    { family: "clap", model: 0, drumify: 0.74, choke: 0 }
  ];
  function makePad(id) {
    const layout = KIT_LAYOUT[id];
    const family = layout.family;
    return {
      id,
      note: PAD_NOTES[id],
      family,
      model: layout.model,
      sampleStart: 0,
      sliceIndex: -1,
      drumify: layout.drumify,
      originalGain: -4.4,
      drumifiedGain: -4.5,
      pitch: 0,
      length: FAMILY_LENGTH[family],
      volume: 0,
      pan: 0,
      choke: layout.choke,
      velocityAmount: 0.65,
      warp: 0.22,
      impact: 0.35,
      instability: 0.08,
      toneShift: 0,
      super: 0.18,
      eqLow: 0,
      eqMid: 0,
      eqHigh: 0,
      bus: 1,
      mute: false,
      solo: false
    };
  }
  function createDefaultPads() {
    return Array.from({ length: PAD_COUNT }, (_, i) => makePad(i));
  }
  var clamp = (v, lo, hi) => v < lo ? lo : v > hi ? hi : v;
  var lerp = (a, b, t) => a + (b - a) * t;
  var dbToGain = (db) => db <= -90 ? 0 : Math.pow(10, db / 20);
  var smoothstep = (edge0, edge1, x) => {
    const t = clamp((x - edge0) / (edge1 - edge0 || 1e-7), 0, 1);
    return t * t * (3 - 2 * t);
  };
  function panGains(pan) {
    const p = clamp(pan, -1, 1);
    const angle = (p + 1) * Math.PI / 4;
    return [Math.cos(angle), Math.sin(angle)];
  }

  // src/audio/dsp.ts
  var TWO_PI = Math.PI * 2;
  var Biquad = class {
    b0 = 1;
    b1 = 0;
    b2 = 0;
    a1 = 0;
    a2 = 0;
    x1 = 0;
    x2 = 0;
    y1 = 0;
    y2 = 0;
    constructor(kind, freq, sampleRate2, q = 0.707, gainDb = 0) {
      this.configure(kind, freq, sampleRate2, q, gainDb);
    }
    configure(kind, freq, sampleRate2, q = 0.707, gainDb = 0) {
      const nyquist = sampleRate2 * 0.5;
      const f = Math.max(10, Math.min(nyquist * 0.95, freq));
      const w0 = TWO_PI * f / sampleRate2;
      const cos = Math.cos(w0);
      const sin = Math.sin(w0);
      const alpha = sin / (2 * Math.max(0.05, q));
      const A = Math.pow(10, gainDb / 40);
      let b0 = 1;
      let b1 = 0;
      let b2 = 0;
      let a0 = 1;
      let a1 = 0;
      let a2 = 0;
      switch (kind) {
        case "lowpass":
          b0 = (1 - cos) / 2;
          b1 = 1 - cos;
          b2 = (1 - cos) / 2;
          a0 = 1 + alpha;
          a1 = -2 * cos;
          a2 = 1 - alpha;
          break;
        case "highpass":
          b0 = (1 + cos) / 2;
          b1 = -(1 + cos);
          b2 = (1 + cos) / 2;
          a0 = 1 + alpha;
          a1 = -2 * cos;
          a2 = 1 - alpha;
          break;
        case "bandpass":
          b0 = alpha;
          b1 = 0;
          b2 = -alpha;
          a0 = 1 + alpha;
          a1 = -2 * cos;
          a2 = 1 - alpha;
          break;
        case "peaking":
          b0 = 1 + alpha * A;
          b1 = -2 * cos;
          b2 = 1 - alpha * A;
          a0 = 1 + alpha / A;
          a1 = -2 * cos;
          a2 = 1 - alpha / A;
          break;
        case "lowshelf": {
          const beta = 2 * Math.sqrt(A) * alpha;
          b0 = A * (A + 1 - (A - 1) * cos + beta);
          b1 = 2 * A * (A - 1 - (A + 1) * cos);
          b2 = A * (A + 1 - (A - 1) * cos - beta);
          a0 = A + 1 + (A - 1) * cos + beta;
          a1 = -2 * (A - 1 + (A + 1) * cos);
          a2 = A + 1 + (A - 1) * cos - beta;
          break;
        }
        case "highshelf": {
          const beta = 2 * Math.sqrt(A) * alpha;
          b0 = A * (A + 1 + (A - 1) * cos + beta);
          b1 = -2 * A * (A - 1 + (A + 1) * cos);
          b2 = A * (A + 1 + (A - 1) * cos - beta);
          a0 = A + 1 - (A - 1) * cos + beta;
          a1 = 2 * (A - 1 - (A + 1) * cos);
          a2 = A + 1 - (A - 1) * cos - beta;
          break;
        }
      }
      this.b0 = b0 / a0;
      this.b1 = b1 / a0;
      this.b2 = b2 / a0;
      this.a1 = a1 / a0;
      this.a2 = a2 / a0;
    }
    process(x) {
      const y = this.b0 * x + this.b1 * this.x1 + this.b2 * this.x2 - this.a1 * this.y1 - this.a2 * this.y2;
      this.x2 = this.x1;
      this.x1 = x;
      this.y2 = this.y1;
      this.y1 = y;
      return y;
    }
    reset() {
      this.x1 = 0;
      this.x2 = 0;
      this.y1 = 0;
      this.y2 = 0;
    }
  };

  // src/audio/rng.ts
  var Rng = class {
    state;
    constructor(seed) {
      this.state = seed >>> 0 || 2654435769;
    }
    /** mulberry32 — fast, deterministic, good enough for noise and humanising. */
    next() {
      let t = this.state += 1831565813;
      t = Math.imul(t ^ t >>> 15, t | 1);
      t ^= t + Math.imul(t ^ t >>> 7, t | 61);
      return ((t ^ t >>> 14) >>> 0) / 4294967296;
    }
    float(min = 0, max = 1) {
      return min + this.next() * (max - min);
    }
    int(min, max) {
      return Math.floor(this.float(min, max + 1));
    }
    pick(items) {
      return items[Math.min(items.length - 1, Math.floor(this.next() * items.length))];
    }
    chance(probability) {
      return this.next() < probability;
    }
    /** Bipolar noise, -1..1. */
    noise() {
      return this.next() * 2 - 1;
    }
  };

  // src/audio/engineCore.ts
  var MAX_VOICES = 32;
  var BUS_COUNT = 8;
  var HALF_PI = Math.PI * 0.5;
  var KICK_MODELS = [
    { fStart: 158, fEnd: 47, pitchTau: 0.042, bodyTauMul: 0.5, click: 0.1, drive: 1.4, sub: 0.14 },
    { fStart: 248, fEnd: 53, pitchTau: 0.016, bodyTauMul: 0.3, click: 0.5, drive: 2.1, sub: 0.06 },
    { fStart: 93, fEnd: 41, pitchTau: 0.12, bodyTauMul: 0.86, click: 0.05, drive: 1.05, sub: 0.24 },
    { fStart: 184, fEnd: 55, pitchTau: 0.028, bodyTauMul: 0.42, click: 0.34, drive: 3.4, sub: 0.12 }
  ];
  var SNARE_MODELS = [
    { tone1: 183, tone2: 327, toneMix: 0.45, noiseMix: 1, noiseTauMul: 0.34, bodyTauMul: 0.25, center: 2400, q: 0.85, snap: 0.65, softAttack: 0 },
    { tone1: 228, tone2: 386, toneMix: 0.62, noiseMix: 0.86, noiseTauMul: 0.2, bodyTauMul: 0.16, center: 3300, q: 0.95, snap: 0.8, softAttack: 0 },
    { tone1: 156, tone2: 271, toneMix: 0.18, noiseMix: 1, noiseTauMul: 0.55, bodyTauMul: 0.4, center: 1500, q: 0.5, snap: 0.18, softAttack: 0.018 },
    { tone1: 427, tone2: 691, toneMix: 0.9, noiseMix: 0.25, noiseTauMul: 0.09, bodyTauMul: 0.09, center: 4600, q: 1.6, snap: 0.55, softAttack: 0 }
  ];
  var HAT_MODELS = [
    { decayMul: 0.52, metal: 0.62, noise: 0.5, metalFreq: 6200, hp: 7e3 },
    { decayMul: 2.7, metal: 0.55, noise: 0.4, metalFreq: 4800, hp: 4800 },
    { decayMul: 1.15, metal: 0.95, noise: 0.18, metalFreq: 8200, hp: 6200 },
    { decayMul: 1.7, metal: 0.16, noise: 1, metalFreq: 3800, hp: 3800 }
  ];
  var CLAP_MODELS = [
    { bursts: 4, spacing: 0.0115, tailMul: 0.62, center: 1350, q: 1.1, wide: 0.6 },
    { bursts: 3, spacing: 8e-3, tailMul: 0.4, center: 1850, q: 1.3, wide: 0.4 },
    { bursts: 5, spacing: 0.017, tailMul: 1.2, center: 920, q: 0.85, wide: 1 },
    { bursts: 2, spacing: 55e-4, tailMul: 0.3, center: 2800, q: 1.5, wide: 0.25 }
  ];
  var FAMILY_GAIN = {
    kick: 0.92,
    snare: 0.58,
    hat: 0.4,
    clap: 0.62
  };
  var MODEL_TRIM = {
    kick: [1, 1, 1, 1],
    snare: [1, 1.02, 1.06, 0.9],
    hat: [1.22, 1.16, 1.5, 0.6],
    clap: [1.04, 1, 1.12, 0.92]
  };
  var METAL_TABLE_SIZE = 4096;
  var METAL_TABLE = (() => {
    const table = new Float32Array(METAL_TABLE_SIZE);
    const ratios = [1, 1.4471, 1.617, 1.9265, 2.5028, 2.6637];
    const phases = [0, 1.7, 3.1, 4.4, 5.2, 2.2];
    for (let i = 0; i < METAL_TABLE_SIZE; i++) {
      const p = i / METAL_TABLE_SIZE * TWO_PI;
      let sum = 0;
      for (let k = 0; k < ratios.length; k++) {
        sum += Math.sin(p * ratios[k] + phases[k]);
      }
      table[i] = sum / ratios.length;
    }
    return table;
  })();
  function readMetal(phase) {
    const p = phase - Math.floor(phase / METAL_TABLE_SIZE) * METAL_TABLE_SIZE;
    const i0 = p | 0;
    const i1 = (i0 + 1) % METAL_TABLE_SIZE;
    const f = p - i0;
    return METAL_TABLE[i0] * (1 - f) + METAL_TABLE[i1] * f;
  }
  var Voice = class {
    active = false;
    muted = false;
    padIndex = 0;
    bus = 0;
    family = "kick";
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
    sample = null;
    sampleRate = 48e3;
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
    toneL = new Biquad("peaking", 900, 48e3, 1.1, 0);
    toneR = new Biquad("peaking", 900, 48e3, 1.1, 0);
    colorL = new Biquad("bandpass", 2e3, 48e3, 1);
    colorR = new Biquad("bandpass", 2e3, 48e3, 1);
    clickL = new Biquad("highpass", 1800, 48e3, 0.7);
    clickR = new Biquad("highpass", 1800, 48e3, 0.7);
    eqLowL = new Biquad("lowshelf", 120, 48e3, 0.7, 0);
    eqLowR = new Biquad("lowshelf", 120, 48e3, 0.7, 0);
    eqMidL = new Biquad("peaking", 1100, 48e3, 0.8, 0);
    eqMidR = new Biquad("peaking", 1100, 48e3, 0.8, 0);
    eqHighL = new Biquad("highshelf", 6800, 48e3, 0.7, 0);
    eqHighR = new Biquad("highshelf", 6800, 48e3, 0.7, 0);
    // scratch values produced by the model renderer each sample
    mL = 0;
    mR = 0;
    /** Fast xorshift noise in -1..1. */
    noise() {
      let x = this.seed;
      x ^= x << 13;
      x ^= x >>> 17;
      x ^= x << 5;
      this.seed = x >>> 0;
      return this.seed / 2147483648 - 1;
    }
    resetFilters(sr) {
      this.toneL.reset();
      this.toneR.reset();
      this.colorL.reset();
      this.colorR.reset();
      this.clickL.reset();
      this.clickR.reset();
      this.eqLowL.reset();
      this.eqLowR.reset();
      this.eqMidL.reset();
      this.eqMidR.reset();
      this.eqHighL.reset();
      this.eqHighR.reset();
      this.toneL.configure("peaking", 900, sr, 1.1, 0);
      this.toneR.configure("peaking", 900, sr, 1.1, 0);
      this.colorL.configure("bandpass", 2200, sr, 1);
      this.colorR.configure("bandpass", 2200, sr, 1);
      this.clickL.configure("highpass", 1800, sr, 0.7);
      this.clickR.configure("highpass", 1800, sr, 0.7);
      this.eqLowL.configure("lowshelf", 120, sr, 0.7, 0);
      this.eqLowR.configure("lowshelf", 120, sr, 0.7, 0);
      this.eqMidL.configure("peaking", 1100, sr, 0.8, 0);
      this.eqMidR.configure("peaking", 1100, sr, 0.8, 0);
      this.eqHighL.configure("highshelf", 6800, sr, 0.7, 0);
      this.eqHighR.configure("highshelf", 6800, sr, 0.7, 0);
    }
    reset(pad, global, velocity, seed, sample, sampleRate2) {
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
      this.sampleRate = sampleRate2;
      this.seed = seed >>> 0 || 19088743;
      const inst = clamp(pad.instability, 0, 1);
      const v = clamp(velocity, 0, 1);
      this.velocity = v;
      const r = () => this.noise();
      const velAmt = clamp(pad.velocityAmount, 0, 1);
      this.velGain = lerp(1, Math.pow(v, 1.35), velAmt);
      const velocityLength = 1 + (v - 0.5) * 0.38 * velAmt;
      this.len = clamp(pad.length * global.length * velocityLength, 0.012, 4);
      const cents = pad.pitch + global.transpose + global.fineTune / 100;
      const pitchJitter = r() * inst * 0.55;
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
        const start = clamp(pad.sampleStart + r() * 3e-3 * inst, 0, Math.max(0, sample.left.length / sample.sampleRate - 2e-3));
        this.srcPos = start * sample.sampleRate;
        this.srcInc = this.pitchRatio * (sample.sampleRate / sampleRate2);
      }
      const pan = clamp(pad.pan + r() * 0.16 * inst, -1, 1);
      const [pl, pr] = panGains(pan);
      this.panL = pl;
      this.panR = pr;
      this.gain = dbToGain(pad.volume + r() * 1.4 * inst);
      this.maxLife = clamp(
        this.len * (pad.family === "kick" ? 5.4 : pad.family === "hat" ? 5.2 : pad.family === "clap" ? 3.6 : 3.6) + 0.06,
        0.08,
        6
      );
      this.pitchOffset = pitchJitter;
      this.tone1Phase = 0;
      this.tone2Phase = 0;
      this.kickPhase = 0;
      this.metalPhase = 0;
      this.warpLowL = 0;
      this.warpLowR = 0;
      this.warpHighL = 0;
      this.warpHighR = 0;
      this.resetFilters(sampleRate2);
      const toneCenter = clamp(880 * Math.pow(2, this.toneShift / 12), 90, 13e3);
      const toneGain = this.toneShift === 0 ? 0 : 4.5 * Math.sign(this.toneShift) * Math.min(1, Math.abs(this.toneShift) / 4);
      this.toneL.configure("peaking", toneCenter, sampleRate2, 1.15, toneGain);
      this.toneR.configure("peaking", toneCenter, sampleRate2, 1.15, toneGain);
      this.eqLowL.configure("lowshelf", 120, sampleRate2, 0.7, pad.eqLow);
      this.eqLowR.configure("lowshelf", 120, sampleRate2, 0.7, pad.eqLow);
      this.eqMidL.configure("peaking", 1200, sampleRate2, 0.8, pad.eqMid);
      this.eqMidR.configure("peaking", 1200, sampleRate2, 0.8, pad.eqMid);
      this.eqHighL.configure("highshelf", 7e3, sampleRate2, 0.7, pad.eqHigh);
      this.eqHighR.configure("highshelf", 7e3, sampleRate2, 0.7, pad.eqHigh);
      switch (this.family) {
        case "kick": {
          this.clickL.configure("highpass", 1500, sampleRate2, 0.7);
          this.clickR.configure("highpass", 1500, sampleRate2, 0.7);
          break;
        }
        case "snare": {
          const m = SNARE_MODELS[this.model];
          this.colorL.configure("bandpass", m.center, sampleRate2, m.q);
          this.colorR.configure("bandpass", m.center, sampleRate2, m.q);
          this.clickL.configure("highpass", 2500, sampleRate2, 0.7);
          this.clickR.configure("highpass", 2500, sampleRate2, 0.7);
          break;
        }
        case "hat": {
          const m = HAT_MODELS[this.model];
          this.colorL.configure("highpass", m.hp, sampleRate2, 0.7);
          this.colorR.configure("highpass", m.hp, sampleRate2, 0.7);
          break;
        }
        case "clap": {
          const m = CLAP_MODELS[this.model];
          this.colorL.configure("bandpass", m.center, sampleRate2, m.q);
          this.colorR.configure("bandpass", m.center, sampleRate2, m.q);
          break;
        }
      }
    }
    choke(fast = false) {
      if (!this.active) return;
      this.choking = true;
      this.killRate = 1 / (0.012 * this.sampleRate);
      if (fast) this.killRate = 1 / (4e-3 * this.sampleRate);
    }
    renderModel() {
      const t = this.t;
      const sr = this.sampleRate;
      const len = this.len;
      const ratio = this.pitchRatio;
      const nL = this.noise();
      const nR = this.noise();
      let mL = 0;
      let mR = 0;
      switch (this.family) {
        case "kick": {
          const m = KICK_MODELS[this.model];
          const f = (m.fEnd + (m.fStart - m.fEnd) * Math.exp(-t / m.pitchTau)) * ratio;
          this.kickPhase += TWO_PI * f / sr;
          if (this.kickPhase > TWO_PI * 64) this.kickPhase %= TWO_PI * 4;
          const tau = Math.max(8e-3, len * m.bodyTauMul);
          const env = Math.exp(-t / tau);
          let body = Math.sin(this.kickPhase) * env;
          body += Math.sin(this.kickPhase * 2 + 0.3) * env * 0.09;
          body += Math.sin(this.kickPhase * 3.02) * env * 0.04;
          const sub = Math.sin(this.kickPhase * 0.5) * Math.exp(-t / (tau * 1.6)) * m.sub;
          const clickAmt = m.click + this.impact * 0.55;
          const click = this.clickL.process(nL) * Math.exp(-t / 28e-4) * clickAmt;
          const click2 = this.clickR.process(nR) * Math.exp(-t / 28e-4) * clickAmt;
          mL = Math.tanh((body + sub + click) * m.drive) / Math.tanh(m.drive);
          mR = Math.tanh((body + sub + click2) * m.drive) / Math.tanh(m.drive);
          break;
        }
        case "snare": {
          const m = SNARE_MODELS[this.model];
          const noiseTau = Math.max(8e-3, len * m.noiseTauMul);
          const bodyTau = Math.max(6e-3, len * m.bodyTauMul);
          const noiseEnv = Math.exp(-t / noiseTau);
          const bodyEnv = Math.exp(-t / bodyTau);
          const attack = m.softAttack > 0 ? smoothstep(0, m.softAttack, t) : 1;
          const noiseL = this.colorL.process(nL) * 2.6 * m.noiseMix * noiseEnv * attack;
          const noiseR = this.colorR.process(nR) * 2.6 * m.noiseMix * noiseEnv * attack;
          this.tone1Phase += TWO_PI * m.tone1 * ratio / sr;
          this.tone2Phase += TWO_PI * m.tone2 * ratio / sr;
          const body = (Math.sin(this.tone1Phase) * 0.7 + Math.sin(this.tone2Phase) * 0.42) * bodyEnv * m.toneMix * attack;
          const snapAmt = m.snap + this.impact * 0.45;
          const snap = Math.exp(-t / 45e-4) * snapAmt;
          const snapL = this.clickL.process(nL) * snap;
          const snapR = this.clickR.process(nR) * snap;
          const width = this.superAmount * 0.45;
          const center = body * 0.6 + snapL * 0.26;
          mL = noiseL + center + (noiseR - noiseL) * width * 0.5 + snapL * 0.35;
          mR = noiseR + center + (noiseL - noiseR) * width * 0.5 + snapR * 0.35;
          break;
        }
        case "hat": {
          const m = HAT_MODELS[this.model];
          const tau = Math.max(6e-3, len * 0.45 * m.decayMul);
          const env = Math.exp(-t / tau) * Math.min(1, t / 7e-4 + 0.15);
          this.metalPhase += m.metalFreq * ratio / sr * METAL_TABLE_SIZE;
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
        case "clap": {
          const m = CLAP_MODELS[this.model];
          const spacing = m.spacing * (0.9 + clamp(this.impact, 0, 1) * 0.15);
          let burst = 0;
          for (let b = 0; b < m.bursts; b++) {
            const bt = t - b * spacing;
            if (bt >= 0) burst += Math.exp(-bt / 45e-4);
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
    renderTo(outL, outR, frame) {
      if (!this.active) return;
      const t = this.t;
      const sr = this.sampleRate;
      let srcL = 0;
      let srcR = 0;
      if (this.hasSample && this.sample && this.srcMix > 1e-5) {
        const sample = this.sample;
        const pos = this.srcPos;
        const i0 = pos | 0;
        if (i0 >= 0 && i0 + 1 < sample.left.length) {
          const f = pos - i0;
          const l = sample.left[i0] * (1 - f) + sample.left[i0 + 1] * f;
          const r = sample.right[i0] * (1 - f) + sample.right[i0 + 1] * f;
          const attack = Math.min(1, t / 12e-4);
          const decay = Math.exp(-t / Math.max(8e-3, this.len * 0.62));
          const release = 1 - smoothstep(this.len * 0.95, this.len * 1.35, t);
          const env = attack * decay * release;
          srcL = l * env * this.srcMix;
          srcR = r * env * this.srcMix;
          this.srcPos += this.srcInc;
        }
      }
      let mL = 0;
      let mR = 0;
      if (this.modelMix > 1e-5) {
        this.renderModel();
        mL = this.mL * this.modelMix;
        mR = this.mR * this.modelMix;
      }
      let xL = srcL + mL;
      let xR = srcR + mR;
      if (this.impact > 1e-3) {
        const boost = 1 + this.impact * 1.8 * Math.exp(-t / 45e-4);
        xL *= boost;
        xR *= boost;
      }
      if (this.warp > 1e-3) {
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
      xL = this.eqLowL.process(this.toneL.process(xL));
      xR = this.eqLowR.process(this.toneR.process(xR));
      xL = this.eqMidL.process(xL);
      xR = this.eqMidR.process(xR);
      xL = this.eqHighL.process(xL);
      xR = this.eqHighR.process(xR);
      if (this.superAmount > 1e-3) {
        const drive = 1 + this.superAmount * 3.2;
        const norm = Math.tanh(drive);
        xL = Math.tanh(xL * drive) / norm;
        xR = Math.tanh(xR * drive) / norm;
        const mid = (xL + xR) * 0.5;
        const side = (xL - xR) * 0.5 * (1 + this.superAmount * 0.9);
        xL = mid + side;
        xR = mid - side;
      }
      if (Math.abs(this.width - 1) > 1e-3) {
        const mid = (xL + xR) * 0.5;
        const side = (xL - xR) * 0.5 * this.width;
        xL = mid + side;
        xR = mid - side;
      }
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
    warpLowCoef = 1 - Math.exp(-TWO_PI * 200 / 48e3);
    warpHighCoef = 1 - Math.exp(-TWO_PI * 4200 / 48e3);
    setSampleRate(sr) {
      this.sampleRate = sr;
      this.warpLowCoef = 1 - Math.exp(-TWO_PI * 200 / sr);
      this.warpHighCoef = 1 - Math.exp(-TWO_PI * 4200 / sr);
    }
  };
  var VracEngine = class {
    sampleRate;
    pads = createDefaultPads();
    global;
    sample = null;
    voices = [];
    pending = [];
    rng;
    audible = new Array(PAD_COUNT).fill(true);
    busMeters = new Float32Array(BUS_COUNT);
    meterDue = false;
    blocks = 0;
    anySolo = false;
    constructor(sampleRate2, seed = 1592594996) {
      this.sampleRate = sampleRate2;
      this.rng = new Rng(seed);
      this.global = {
        fineTune: 0,
        drumify: 100,
        volume: 0,
        transpose: 0,
        length: 1,
        tone: 0,
        filter: 0,
        width: 0.65
      };
      for (let i = 0; i < MAX_VOICES; i++) {
        const voice = new Voice();
        voice.setSampleRate(sampleRate2);
        this.voices.push(voice);
      }
      this.refreshAudible();
    }
    setSample(left, right, sampleRate2) {
      const length = Math.min(left.length, right.length);
      if (length < 2) {
        this.sample = null;
        return;
      }
      this.sample = {
        left: left.length === length ? left : left.subarray(0, length),
        right: right.length === length ? right : right.subarray(0, length),
        sampleRate: sampleRate2 || this.sampleRate
      };
    }
    setParams(params) {
      if (params.pads.length >= PAD_COUNT) this.pads = params.pads;
      this.global = params.global;
      this.refreshAudible();
      for (const voice of this.voices) {
        if (voice.active) voice.muted = !this.audible[voice.padIndex];
      }
    }
    refreshAudible() {
      this.anySolo = this.pads.some((p) => p.solo);
      for (let i = 0; i < PAD_COUNT; i++) {
        const pad = this.pads[i];
        this.audible[i] = !pad.mute && (!this.anySolo || pad.solo);
      }
    }
    trigger(padIndex, velocity = 0.85, delayFrames = 0) {
      const index = padIndex | 0;
      if (index < 0 || index >= PAD_COUNT) return;
      if (!this.audible[index]) return;
      if (delayFrames > 0) {
        this.pending.push({ pad: index, velocity, delayFrames });
        return;
      }
      this.startVoice(index, velocity);
    }
    allOff(fast = true) {
      this.pending.length = 0;
      for (const voice of this.voices) {
        if (voice.active) voice.choke(fast);
      }
    }
    /** Play a pad without touching the kit's stored state (A/B previews). */
    audition(pad, velocity = 0.85) {
      this.startVoice(clamp(pad.id | 0, 0, PAD_COUNT - 1), velocity, pad);
    }
    startVoice(index, velocity, overridePad) {
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
        slot = this.voices.reduce((oldest, v) => v.t > oldest.t ? v : oldest, this.voices[0]);
      }
      const seed = Math.floor(this.rng.next() * 4294967295) + 1;
      slot.reset(pad, this.global, velocity, seed, this.sample, this.sampleRate);
      slot.muted = overridePad ? false : !this.audible[index];
    }
    process(outputs) {
      const out0 = outputs[0];
      if (!out0 || !out0[0]) return 0;
      const frames = out0[0].length;
      for (let b = 0; b < outputs.length; b++) {
        const bus = outputs[b];
        if (!bus) continue;
        bus[0]?.fill(0);
        bus[1]?.fill(0);
      }
      const scheduled = [];
      if (this.pending.length > 0) {
        const still = [];
        for (const trigger of this.pending) {
          if (trigger.delayFrames >= frames) {
            trigger.delayFrames -= frames;
            still.push(trigger);
          } else {
            scheduled.push({
              offset: Math.max(0, trigger.delayFrames | 0),
              pad: trigger.pad,
              velocity: trigger.velocity
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
    renderSegment(outputs, start, end) {
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
    get activeVoiceCount() {
      let n = 0;
      for (const voice of this.voices) if (voice.active) n++;
      return n;
    }
    getMeter() {
      return {
        buses: Array.from(this.busMeters, (v) => Math.min(1, v)),
        voices: this.activeVoiceCount
      };
    }
    consumeMeterDue() {
      if (!this.meterDue) return false;
      this.meterDue = false;
      return true;
    }
  };

  // src/audio/worklet.ts
  var VracProcessor = class extends AudioWorkletProcessor {
    engine;
    constructor() {
      super();
      this.engine = new VracEngine(sampleRate);
      this.port.onmessage = (event) => {
        const msg = event.data;
        switch (msg.type) {
          case "sample":
            this.engine.setSample(msg.left, msg.right, msg.sampleRate);
            break;
          case "params":
            this.engine.setParams({ pads: msg.pads, global: msg.global });
            break;
          case "trigger":
            this.engine.trigger(msg.pad, msg.velocity, msg.delayFrames ?? 0);
            break;
          case "audition":
            this.engine.audition(msg.pad, msg.velocity);
            break;
          case "allOff":
            this.engine.allOff(msg.fast ?? true);
            break;
        }
      };
    }
    process(_inputs, outputs, _parameters) {
      this.engine.process(outputs);
      if (this.engine.consumeMeterDue()) {
        const meter = {
          type: "meter",
          buses: this.engine.getMeter().buses,
          voices: this.engine.activeVoiceCount
        };
        this.port.postMessage(meter);
      }
      return true;
    }
  };
  registerProcessor("vrac-engine", VracProcessor);
})();
