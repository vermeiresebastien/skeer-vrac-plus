import { describe, expect, it } from 'vitest';
import { generateLibrarySample } from '../../audio/sampleLibrary';
import { sliceGapSeconds } from '../../audio/slicer';
import { DEFAULT_GLOBAL, PAD_COUNT, createInitialKit } from '../../types';
import { FACTORY_PRESETS, buildPresetSession, buildSourceSession } from '../factoryPresets';
import { reducer } from '../useKit';

/* Step 24's two doors, which must not be merged. A factory preset ships its own per-pad
 * lengths as part of the kit's character, so re-deriving them would overwrite a shipped
 * setting. A take the user drops on the plugin ships nothing: every pad's length has to come
 * from where its slice ends, or the kit is one long recording triggered from sixteen places. */
describe('kit sessions and slice ends', () => {
  it('keeps the lengths a factory preset ships', () => {
    const preset = FACTORY_PRESETS[0];
    const { kit, sample, slices } = buildPresetSession(preset.id);
    expect(kit.pads).toHaveLength(PAD_COUNT);
    for (const [index, override] of Object.entries(preset.overrides)) {
      expect(kit.pads[Number(index)].length).toBe(override.length);
    }
    // The derived set is a different thing: if this ever matches, the preset stopped shipping a
    // design and started shipping whatever the detector found.
    const derived = kit.pads.map((pad) =>
      Math.min(2, Math.max(0.02, sliceGapSeconds(slices, pad.sliceIndex, sample.duration) / kit.global.length)),
    );
    expect(kit.pads.filter((pad, i) => pad.length === derived[i]).length).toBe(0);
  });

  it('derives every pad length from the material for a take that has no lengths', () => {
    const sample = generateLibrarySample('scrap');
    // A global of 0.5 is here to prove LENGTH is the knob's own units: the player multiplies it
    // back by the global, so this divides it out. Nothing else is compensated.
    const previous = { ...createInitialKit(), global: { ...DEFAULT_GLOBAL, length: 0.5 } };
    const { kit, slices } = buildSourceSession('scrap', 'SCRAP TAKE', sample, previous, 0.6);
    expect(slices.length).toBeGreaterThan(1);
    let interior = 0;
    kit.pads.forEach((pad) => {
      expect(pad.sliceIndex).toBeGreaterThanOrEqual(0);
      // Stated from the raw slice list rather than through `sliceGapSeconds`, so a change to the
      // helper cannot move the expectation along with the value it is supposed to be checking.
      const i = pad.sliceIndex;
      const begin = slices[i].time;
      const finish = i + 1 < slices.length ? slices[i + 1].time : sample.duration;
      const gap = Math.max(0, finish - begin);
      const raw = gap / 0.5;
      expect(pad.length).toBeCloseTo(Math.min(2, Math.max(0.02, raw)), 10);
      if (raw > 0.02 && raw < 2) {
        interior++;
        expect(pad.length * kit.global.length).toBeCloseTo(gap, 10);
      }
    });
    // Anti-vacuous: without a single unclamped pad, the bounds would be doing all the work.
    expect(interior).toBeGreaterThan(0);
    // And the pad design the player chose by ear survives the re-derive.
    expect(kit.pads.map((pad) => pad.family)).toEqual(previous.pads.map((pad) => pad.family));
    expect(kit.pads.map((pad) => pad.drumify)).toEqual(previous.pads.map((pad) => pad.drumify));
    expect(kit.pads.map((pad) => pad.volume)).toEqual(previous.pads.map((pad) => pad.volume));
  });

  it('moves the lengths when the material changes', () => {
    const previous = createInitialKit();
    const first = buildSourceSession('foley', 'A', generateLibrarySample('foley'), previous, 0.6).kit;
    const second = buildSourceSession('body', 'B', generateLibrarySample('body'), previous, 0.6).kit;
    expect(first.pads.map((pad) => pad.length)).not.toEqual(second.pads.map((pad) => pad.length));
    // Each one is its own material: no pad plays longer than the slice it was given.
    for (const kit of [first, second]) {
      expect(Math.max(...kit.pads.map((pad) => pad.length))).toBeLessThanOrEqual(2);
      expect(Math.min(...kit.pads.map((pad) => pad.length))).toBeGreaterThanOrEqual(0.02);
    }
  });

  it('re-derives the lengths when the SENSITIVITY knob reslices the take', () => {
    // The live door, not the session builder: moving the knob moves where the slices end, so the
    // lengths derived from the old boundaries have to move with them.
    const base = buildPresetSession('iron-works');
    expect(base.slices.length).toBeGreaterThan(1);
    const { present } = reducer({ present: base, past: [], future: [] }, { type: 'sensitivity', value: 0.95 });
    expect(present.slices.length).toBeGreaterThan(1);
    let checked = 0;
    present.kit.pads.forEach((pad) => {
      expect(pad.sliceIndex).toBeGreaterThanOrEqual(0);
      const i = pad.sliceIndex;
      const begin = present.slices[i].time;
      const finish = i + 1 < present.slices.length ? present.slices[i + 1].time : present.sample.duration;
      const raw = Math.max(0, finish - begin) / present.kit.global.length;
      expect(pad.length).toBeCloseTo(Math.min(2, Math.max(0.02, raw)), 10);
      if (raw > 0.02 && raw < 2) checked++;
    });
    expect(checked).toBeGreaterThan(0);
    // The rest of the shipped design survives the reslice: the knob moved the boundaries, not the
    // models or the gains.
    expect(present.kit.pads.map((pad) => pad.family)).toEqual(base.kit.pads.map((pad) => pad.family));
    expect(present.kit.pads.map((pad) => pad.drumify)).toEqual(base.kit.pads.map((pad) => pad.drumify));
  });
});
