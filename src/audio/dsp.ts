/* Small, dependency-free DSP helpers shared by the engine and the offline
 * sample/one-shot rendering. Everything here is pure and testable in Node. */

export const TWO_PI = Math.PI * 2;

export function dbToGain(db: number): number {
  return db <= -96 ? 0 : Math.pow(10, db / 20);
}

export function softClip(x: number): number {
  return Math.tanh(x);
}

/** A one-pole low-pass section. */
export class OnePole {
  private z = 0;
  private readonly a: number;

  constructor(cutoff: number, sampleRate: number) {
    this.a = Math.exp((-TWO_PI * cutoff) / sampleRate);
  }

  process(x: number): number {
    this.z = x * (1 - this.a) + this.z * this.a;
    return this.z;
  }

  reset(): void {
    this.z = 0;
  }
}

export type BiquadKind =
  | 'lowpass'
  | 'highpass'
  | 'bandpass'
  | 'peaking'
  | 'lowshelf'
  | 'highshelf';

/** RBJ cookbook biquad, direct form I. */
export class Biquad {
  private b0 = 1;
  private b1 = 0;
  private b2 = 0;
  private a1 = 0;
  private a2 = 0;
  private x1 = 0;
  private x2 = 0;
  private y1 = 0;
  private y2 = 0;

  constructor(kind: BiquadKind, freq: number, sampleRate: number, q = 0.707, gainDb = 0) {
    this.configure(kind, freq, sampleRate, q, gainDb);
  }

  configure(kind: BiquadKind, freq: number, sampleRate: number, q = 0.707, gainDb = 0): void {
    const nyquist = sampleRate * 0.5;
    const f = Math.max(10, Math.min(nyquist * 0.95, freq));
    const w0 = (TWO_PI * f) / sampleRate;
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
      case 'lowpass':
        b0 = (1 - cos) / 2;
        b1 = 1 - cos;
        b2 = (1 - cos) / 2;
        a0 = 1 + alpha;
        a1 = -2 * cos;
        a2 = 1 - alpha;
        break;
      case 'highpass':
        b0 = (1 + cos) / 2;
        b1 = -(1 + cos);
        b2 = (1 + cos) / 2;
        a0 = 1 + alpha;
        a1 = -2 * cos;
        a2 = 1 - alpha;
        break;
      case 'bandpass':
        b0 = alpha;
        b1 = 0;
        b2 = -alpha;
        a0 = 1 + alpha;
        a1 = -2 * cos;
        a2 = 1 - alpha;
        break;
      case 'peaking':
        b0 = 1 + alpha * A;
        b1 = -2 * cos;
        b2 = 1 - alpha * A;
        a0 = 1 + alpha / A;
        a1 = -2 * cos;
        a2 = 1 - alpha / A;
        break;
      case 'lowshelf': {
        const beta = 2 * Math.sqrt(A) * alpha;
        b0 = A * (A + 1 - (A - 1) * cos + beta);
        b1 = 2 * A * (A - 1 - (A + 1) * cos);
        b2 = A * (A + 1 - (A - 1) * cos - beta);
        a0 = A + 1 + (A - 1) * cos + beta;
        a1 = -2 * (A - 1 + (A + 1) * cos);
        a2 = A + 1 + (A - 1) * cos - beta;
        break;
      }
      case 'highshelf': {
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

  process(x: number): number {
    const y = this.b0 * x + this.b1 * this.x1 + this.b2 * this.x2 - this.a1 * this.y1 - this.a2 * this.y2;
    this.x2 = this.x1;
    this.x1 = x;
    this.y2 = this.y1;
    this.y1 = y;
    return y;
  }

  reset(): void {
    this.x1 = 0;
    this.x2 = 0;
    this.y1 = 0;
    this.y2 = 0;
  }
}

/** Cheap one-pole high-pass, used where a full biquad is overkill. */
export class DCBlocker {
  private x1 = 0;
  private y1 = 0;
  private readonly r: number;

  constructor(sampleRate: number, cutoff = 12) {
    this.r = 1 - (TWO_PI * cutoff) / sampleRate;
  }

  process(x: number): number {
    const y = x - this.x1 + this.r * this.y1;
    this.x1 = x;
    this.y1 = y;
    return y;
  }
}

export function equalPowerPan(pan: number): [number, number] {
  const angle = ((Math.max(-1, Math.min(1, pan)) + 1) * Math.PI) / 4;
  return [Math.cos(angle), Math.sin(angle)];
}
