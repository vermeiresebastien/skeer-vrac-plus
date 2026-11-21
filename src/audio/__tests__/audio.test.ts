import { describe, expect, it } from 'vitest';
import { generateLibrarySample } from '../sampleLibrary';
import {
  assignSliceLengths,
  detectSlices,
  evenStarts,
  padLengthFromSeconds,
  padStartsFromSlices,
  sliceGapSeconds,
  type Slice,
} from '../slicer';
import { SliceForgeEngine } from '../engineCore';
import { DEFAULT_GLOBAL, createDefaultPads, type PadParams } from '../../types';

function makeOutputs(frames = 128): Float32Array[][] {
  return Array.from({ length: 8 }, () => [new Float32Array(frames), new Float32Array(frames)]) as Float32Array[][];
}

function render(engine: SliceForgeEngine, blocks: number): Float32Array[][] {
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

function allPeak(engine: SliceForgeEngine, blocks: number): number {
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

/* Step 24 — give every slice an end. A detected onset used to be only a start: the pad that
 * got it kept the shipped LENGTH, so a 90 ms hi-hat gap played 420 ms of the hit after it and a
 * 1.2 s tail played 420 ms of itself. These are the native suite's checks at
 * `native/Tests/SmokeTest.cpp` mirrored onto the browser engine, on the same four onsets. */
describe('slice ends', () => {
  const onsets = [0.4, 1.3, 1.75, 2.6];
  const fileSeconds = 3.0;
  const slicesAt = (times: number[]): Slice[] => times.map((time) => ({ time, strength: 0.8 }));
  const slices = slicesAt(onsets);

  it('measures a slice as the distance to the next onset, and the last as the file tail', () => {
    expect(sliceGapSeconds(slices, 0, fileSeconds)).toBeCloseTo(0.9, 12);
    expect(sliceGapSeconds(slices, 1, fileSeconds)).toBeCloseTo(0.45, 12);
    expect(sliceGapSeconds(slices, 2, fileSeconds)).toBeCloseTo(0.85, 12);
    expect(sliceGapSeconds(slices, 3, fileSeconds)).toBeCloseTo(0.4, 12);
    expect(sliceGapSeconds(slices, 4, fileSeconds)).toBe(0);
    expect(sliceGapSeconds(slices, -1, fileSeconds)).toBe(0);
    expect(sliceGapSeconds([], 0, fileSeconds)).toBe(0);
  });

  it('never ends a slice before it began when the onset sits past the material', () => {
    // The pad editor lets you place a slice beyond the end of the file, so the end is taken as
    // never before the start rather than trusted to be after it.
    expect(sliceGapSeconds(slicesAt([0.5, 1.0, 1.5, 2.8]), 3, 2.0)).toBe(0);
  });

  it('tiles the material: the four gaps run from the first onset to the end of the file', () => {
    // Not "sum to the duration" — that only holds when the first onset is at zero. The material
    // before the first onset belongs to no slice, so the sum is duration minus that start.
    const gaps = slices.map((_, i) => sliceGapSeconds(slices, i, fileSeconds));
    expect(gaps.reduce((a, b) => a + b, 0)).toBeCloseTo(fileSeconds - onsets[0], 12);
  });

  it('repeats a slice only after every slice has a pad of its own', () => {
    const pads = assignSliceLengths(padStartsFromSlices(slices, createDefaultPads()), slices, fileSeconds, 1);
    expect(pads).toHaveLength(16);
    // Four onsets used to wrap at `pad % 4`, which left all sixteen pads on the first quarter of
    // the file. Now the first four take the four slices in order and only then does it reuse them.
    expect(pads.slice(0, 4).map((pad) => pad.sliceIndex)).toEqual([0, 1, 2, 3]);
    expect(new Set(pads.map((pad) => pad.sampleStart)).size).toBe(4);
    expect(new Set(pads.map((pad) => pad.length)).size).toBe(4);
    // The spread runs forward through the material, so the last pad reaches the last onset.
    expect(pads[15].sliceIndex).toBe(3);
    expect(pads[15].sampleStart).toBe(onsets[3]);
    expect(pads[15].length).toBeCloseTo(0.4, 12);
  });

  it('walks forward through the material once the slices run out', () => {
    // The old `pad % n` wrap also produced four distinct starts, so distinctness alone is not the
    // defect. What the wrap did was restart at zero: a four-onset take put pads 4 through 7 back on
    // the first quarter of the file. After every slice has had its own pad, the reuse has to keep
    // moving towards the end.
    const pads = assignSliceLengths(padStartsFromSlices(slices, createDefaultPads()), slices, fileSeconds, 1);
    const indices = pads.map((pad) => pad.sliceIndex);
    // The step from the last first-round pad to the first reused pad is the legitimate restart:
    // pads 0..3 take slices 0..3, then the reuse begins again at the top of the map.
    expect(indices[onsets.length]).toBe(0);
    for (let i = onsets.length + 1; i < pads.length; i++) expect(indices[i]).toBeGreaterThanOrEqual(indices[i - 1]);
    const starts = pads.map((pad) => pad.sampleStart);
    for (let i = onsets.length + 1; i < pads.length; i++) expect(starts[i]).toBeGreaterThanOrEqual(starts[i - 1]);
    // And the reuse is spread over the whole file, not bunched on one onset: the last pad of each
    // round still lands on the last slice, and the middle of the map is not a single start.
    expect(indices[pads.length - 1]).toBe(slices.length - 1);
    expect(new Set(indices.slice(onsets.length)).size).toBeGreaterThan(1);
  });

  it('writes LENGTH as the gap divided by the global, bounded by the knob', () => {
    expect(padLengthFromSeconds(0.9, 1.0)).toBeCloseTo(0.9, 12);
    expect(padLengthFromSeconds(0.9, 2.0)).toBeCloseTo(0.45, 12);
    // The global knob bottoms out at 0.25, so a smaller argument cannot inflate the pad:
    // 0.9 s across a 0.1 global still divides by 0.25, and 3.6 lands on the ceiling.
    expect(padLengthFromSeconds(0.9, 0.1)).toBe(2);
    expect(padLengthFromSeconds(0.001, 1.0)).toBe(0.02);
    expect(padLengthFromSeconds(30, 1.0)).toBe(2);
  });

  it('leaves a hand-placed start alone', () => {
    // A pad with sliceIndex -1 was moved by hand; recomputing its length would silently undo it.
    const shipped = createDefaultPads();
    expect(assignSliceLengths(shipped, slices, fileSeconds, 1).map((pad) => pad.length)).toEqual(
      shipped.map((pad) => pad.length),
    );
  });

  it('plays the slice, not the rest of the file', () => {
    const sr = 48000;
    const frames = sr * 2;
    const left = new Float32Array(frames);
    const right = new Float32Array(frames);
    for (let i = 0; i < frames; i++) {
      const v = 0.9 * Math.sin((2 * Math.PI * 300 * i) / sr);
      left[i] = v;
      right[i] = v;
    }
    // Pad 0 gets the onset at zero and the gap to the next one: 200 ms of a 2 s recording.
    const short = slicesAt([0, 0.2, 1.0, 1.5]);
    const pads = assignSliceLengths(padStartsFromSlices(short, createDefaultPads()), short, 2, 1);
    expect(pads[0].length).toBeCloseTo(0.2, 12);
    const play = pads.map((pad) => ({
      ...pad,
      drumify: 0,
      originalGain: 0,
      velocityAmount: 0,
      instability: 0,
      impact: 0,
      warp: 0,
      super: 0,
      choke: 0,
      pitch: 0,
      pan: 0,
      volume: 0,
      toneShift: 0,
      eqLow: 0,
      eqMid: 0,
      eqHigh: 0,
      mute: false,
      solo: false,
    }));
    const engine = new SliceForgeEngine(sr);
    engine.setSample(left, right, sr);
    engine.setParams({
      pads: play,
      global: { ...DEFAULT_GLOBAL, drumify: 0, length: 1, transpose: 0, fineTune: 0 },
    });
    engine.trigger(0, 1);
    const peakBetween = (fromSeconds: number, toSeconds: number): number => {
      let peak = 0;
      const blocks = Math.ceil((toSeconds * sr) / 128);
      for (let b = 0; b < blocks; b++) {
        const outputs = makeOutputs();
        engine.process(outputs);
        if (b * 128 >= fromSeconds * sr) peak = Math.max(peak, peakOf(outputs));
      }
      return peak;
    };
    const inside = peakBetween(0, 0.3);
    const after = peakBetween(0.4, 0.9);
    // The first guards against a vacuous pass: a silent engine also satisfies the second bound.
    expect(inside).toBeGreaterThan(0.2);
    expect(after).toBeLessThan(inside * 0.01);
  });
});

describe('voice engine', () => {
  it('is silent before a trigger', () => {
    const engine = new SliceForgeEngine(48000);
    expect(allPeak(engine, 5)).toBe(0);
  });

  it('plays a model and a sample layer on the selected bus', () => {
    const engine = new SliceForgeEngine(48000);
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
    const engine = new SliceForgeEngine(48000);
    const pads = createDefaultPads();
    pads[2].drumify = 1;
    engine.setParams({ pads, global: { ...DEFAULT_GLOBAL } });
    engine.trigger(2, 1);
    expect(allPeak(engine, 60)).toBeGreaterThan(0.02);
  });

  it('respects mute and solo', () => {
    const base = createDefaultPads();
    const engine = new SliceForgeEngine(48000);
    engine.setSample(generateLibrarySample('foley').left, generateLibrarySample('foley').right, 48000);
    const muted: PadParams[] = base.map((p, i) => ({ ...p, mute: i === 0 }));
    engine.setParams({ pads: muted, global: { ...DEFAULT_GLOBAL } });
    engine.trigger(0, 1);
    expect(allPeak(engine, 40)).toBe(0);

    const soloed: PadParams[] = base.map((p, i) => ({ ...p, solo: i === 2 }));
    const engine2 = new SliceForgeEngine(48000);
    engine2.setSample(generateLibrarySample('foley').left, generateLibrarySample('foley').right, 48000);
    engine2.setParams({ pads: soloed, global: { ...DEFAULT_GLOBAL } });
    engine2.trigger(0, 1);
    expect(allPeak(engine2, 20)).toBe(0);
    engine2.trigger(2, 1);
    expect(allPeak(engine2, 30)).toBeGreaterThan(0.003);
  });

  it('chokes voices in the same group', () => {
    // The old claim here was activeVoiceCount <= 2 after two triggers, which is also what an
    // engine that ignored choke entirely would report. This measures the pad that is supposed to
    // be silenced, on a bus the second hit never touches: an unchoked hat of length 0.09 s is
    // still ~35 blocks into its tail here, while the choke ramp is 12 ms (~4.5 blocks).
    function chokedBusPeak(choke: number): number {
      const pads = createDefaultPads();
      pads[6].choke = choke;
      pads[7].choke = choke;
      pads[7].bus = 2; // keep pad 8 off bus 1 so the measurement sees only pad 7's target
      const engine = new SliceForgeEngine(48000);
      engine.setParams({ pads, global: { ...DEFAULT_GLOBAL } });
      engine.trigger(6, 1);
      render(engine, 4);
      engine.trigger(7, 1);
      // Eight blocks first, which is past the 12 ms choke ramp and still short of the hat's own
      // 0.09 s tail: measuring from here is what separates a killed voice from a ringing one.
      render(engine, 8);
      let peak = 0;
      for (let i = 0; i < 8; i++) {
        const outputs = render(engine, 1);
        for (const channel of outputs[0]) {
          for (let s = 0; s < channel.length; s++) peak = Math.max(peak, Math.abs(channel[s]));
        }
      }
      return peak;
    }

    const grouped = chokedBusPeak(2);
    const open = chokedBusPeak(0);
    expect(open).toBeGreaterThan(0.001); // the control really is still ringing
    expect(grouped).toBeLessThan(open * 0.2);
  });

  it('allOff silences the engine', () => {
    const engine = new SliceForgeEngine(48000);
    engine.setParams({ pads: createDefaultPads(), global: { ...DEFAULT_GLOBAL } });
    engine.trigger(0, 1);
    render(engine, 2);
    engine.allOff(true);
    render(engine, 8);
    expect(allPeak(engine, 10)).toBe(0);
  });

  it('scales level with velocity', () => {
    function hitPeak(velocity: number): number {
      const engine = new SliceForgeEngine(48000);
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
        const engine = new SliceForgeEngine(48000);
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
