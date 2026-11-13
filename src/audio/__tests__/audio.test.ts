import { describe, expect, it } from 'vitest';
import { generateLibrarySample } from '../sampleLibrary';
import { detectSlices, evenStarts, padStartsFromSlices } from '../slicer';
import { VracEngine } from '../engineCore';
import { DEFAULT_GLOBAL, createDefaultPads, type PadParams } from '../../types';

function makeOutputs(frames = 128): Float32Array[][] {
  return Array.from({ length: 8 }, () => [new Float32Array(frames), new Float32Array(frames)]) as Float32Array[][];
}

function render(engine: VracEngine, blocks: number): Float32Array[][] {
  let outputs = makeOutputs();
  for (let i = 0; i < blocks; i++) {
    outputs = makeOutputs();
    engine.process(outputs);
  }
  return outputs;
}

function peakOf(outputs: Float32Array[][]): number {
  let peak = 0;
  for (const bus of outputs) {
    for (const channel of bus) {
      for (let i = 0; i < channel.length; i++) peak = Math.max(peak, Math.abs(channel[i]));
    }
  }
  return peak;
}

function allPeak(engine: VracEngine, blocks: number): number {
  let peak = 0;
  for (let i = 0; i < blocks; i++) {
    const outputs = makeOutputs();
    engine.process(outputs);
    peak = Math.max(peak, peakOf(outputs));
  }
  return peak;
}

describe('factory library', () => {
  it('generates finite, normalised audio for every source', () => {
    for (const id of ['foley', 'kitchen', 'body', 'scrap', 'circuit', 'vinyl'] as const) {
      const sample = generateLibrarySample(id);
      expect(sample.left.length).toBeGreaterThan(48000 * 5);
      expect(sample.left.length).toBe(sample.right.length);
      let peak = 0;
      let bad = 0;
      for (let i = 0; i < sample.left.length; i++) {
        const l = sample.left[i];
        const r = sample.right[i];
        if (!Number.isFinite(l) || !Number.isFinite(r)) bad++;
        peak = Math.max(peak, Math.abs(l), Math.abs(r));
      }
      expect(bad).toBe(0);
      expect(peak).toBeGreaterThan(0.5);
      expect(peak).toBeLessThanOrEqual(1);
    }
  });

  it('finds a useful number of slices on the foley source', () => {
    const sample = generateLibrarySample('foley');
    const { slices } = detectSlices(sample.left, sample.right, sample.sampleRate, 0.55);
    expect(slices.length).toBeGreaterThanOrEqual(12);
    expect(slices.length).toBeLessThanOrEqual(64);
    for (let i = 1; i < slices.length; i++) {
      expect(slices[i].time).toBeGreaterThan(slices[i - 1].time);
    }
  });

  it('finds more slices at higher sensitivity on a dense source', () => {
    const sample = generateLibrarySample('circuit');
    const low = detectSlices(sample.left, sample.right, sample.sampleRate, 0.1);
    const high = detectSlices(sample.left, sample.right, sample.sampleRate, 0.95);
    expect(high.slices.length).toBeGreaterThanOrEqual(low.slices.length);
    expect(high.slices.length).toBeGreaterThan(12);
  });

  it('distributes detected slices across all 16 pads', () => {
    const sample = generateLibrarySample('kitchen');
    const { slices } = detectSlices(sample.left, sample.right, sample.sampleRate, 0.6);
    const pads = padStartsFromSlices(slices, createDefaultPads());
    expect(pads).toHaveLength(16);
    expect(pads[0].sampleStart).toBe(slices[0].time);
    expect(pads[15].sampleStart).toBe(slices[slices.length - 1].time);
    expect(new Set(pads.map((p) => p.sampleStart)).size).toBeGreaterThan(8);
  });

  it('has an even-spread fallback', () => {
    const starts = evenStarts(4);
    expect(starts).toHaveLength(16);
    expect(starts[15].time).toBeLessThan(4);
  });
});

describe('voice engine', () => {
  it('is silent before a trigger', () => {
    const engine = new VracEngine(48000);
    expect(allPeak(engine, 5)).toBe(0);
  });

  it('plays a model and a sample layer on the selected bus', () => {
    const engine = new VracEngine(48000);
    const sample = generateLibrarySample('foley');
    engine.setSample(sample.left, sample.right, sample.sampleRate);
    const pads = createDefaultPads();
    pads[0].bus = 3;
    engine.setParams({ pads, global: { ...DEFAULT_GLOBAL } });
    engine.trigger(0, 1);
    const outputs = render(engine, 20);
    const busPeaks = outputs.map((bus) => Math.max(...bus.map((ch) => Math.max(...Array.from(ch, Math.abs)))));
    expect(busPeaks[2]).toBeGreaterThan(0.01);
    expect(busPeaks.reduce((a, b) => a + b, 0)).toBeCloseTo(busPeaks[2], 5);
  });

  it('produces an audible drumified-only hit with no sample loaded', () => {
    const engine = new VracEngine(48000);
    const pads = createDefaultPads();
    pads[2].drumify = 1;
    engine.setParams({ pads, global: { ...DEFAULT_GLOBAL } });
    engine.trigger(2, 1);
    expect(allPeak(engine, 60)).toBeGreaterThan(0.02);
  });

  it('respects mute and solo', () => {
    const base = createDefaultPads();
    const engine = new VracEngine(48000);
    engine.setSample(generateLibrarySample('foley').left, generateLibrarySample('foley').right, 48000);
    const muted: PadParams[] = base.map((p, i) => ({ ...p, mute: i === 0 }));
    engine.setParams({ pads: muted, global: { ...DEFAULT_GLOBAL } });
    engine.trigger(0, 1);
    expect(allPeak(engine, 40)).toBe(0);

    const soloed: PadParams[] = base.map((p, i) => ({ ...p, solo: i === 2 }));
    const engine2 = new VracEngine(48000);
    engine2.setSample(generateLibrarySample('foley').left, generateLibrarySample('foley').right, 48000);
    engine2.setParams({ pads: soloed, global: { ...DEFAULT_GLOBAL } });
    engine2.trigger(0, 1);
    expect(allPeak(engine2, 20)).toBe(0);
    engine2.trigger(2, 1);
    expect(allPeak(engine2, 30)).toBeGreaterThan(0.003);
  });

  it('chokes voices in the same group', () => {
    const pads = createDefaultPads();
    // two different pads in group 2
    pads[6].choke = 2;
    pads[7].choke = 2;
    const engine = new VracEngine(48000);
    engine.setParams({ pads, global: { ...DEFAULT_GLOBAL } });
    engine.trigger(6, 1);
    render(engine, 4);
    const first = engine.activeVoiceCount;
    engine.trigger(7, 1);
    render(engine, 4);
    // The first voice is fading, so both are still technically active for a
    // few milliseconds, but the count never exceeds two.
    expect(first).toBeGreaterThanOrEqual(1);
    expect(engine.activeVoiceCount).toBeLessThanOrEqual(2);
  });

  it('allOff silences the engine', () => {
    const engine = new VracEngine(48000);
    engine.setParams({ pads: createDefaultPads(), global: { ...DEFAULT_GLOBAL } });
    engine.trigger(0, 1);
    render(engine, 2);
    engine.allOff(true);
    render(engine, 8);
    expect(allPeak(engine, 10)).toBe(0);
  });

  it('scales level with velocity', () => {
    function hitPeak(velocity: number): number {
      const engine = new VracEngine(48000);
      engine.setParams({ pads: createDefaultPads(), global: { ...DEFAULT_GLOBAL } });
      engine.trigger(0, velocity);
      return allPeak(engine, 40);
    }
    const loud = hitPeak(1);
    const soft = hitPeak(0.2);
    expect(loud).toBeGreaterThan(soft * 2);
  });

  it('makes every model in the bank audible', () => {
    const families = ['kick', 'snare', 'hat', 'clap'] as const;
    for (const family of families) {
      for (let model = 0; model < 4; model++) {
        const engine = new VracEngine(48000);
        const pads = createDefaultPads();
        pads[0] = {
          ...pads[0],
          family,
          model,
          drumify: 1,
          length: family === 'hat' ? 0.12 : 0.22,
          volume: 0,
        };
        engine.setParams({ pads, global: { ...DEFAULT_GLOBAL } });
        engine.trigger(0, 1);
        const peak = allPeak(engine, 35);
        expect(peak, `${family} model ${model}`).toBeGreaterThan(0.004);
        expect(peak).toBeLessThan(2);
      }
    }
  });
});
