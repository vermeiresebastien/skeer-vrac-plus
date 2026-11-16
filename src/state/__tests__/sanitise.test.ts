import { describe, expect, it } from 'vitest';
import {
  DEFAULT_GLOBAL,
  PAD_COUNT,
  clamp,
  createDefaultPads,
  sanitizeGlobal,
  sanitizeNumber,
  sanitizePad,
} from '../../types';

describe('clamp', () => {
  it('treats a value that cannot be ordered as the low end', () => {
    // Every comparison fails against NaN, so the plain two-sided test used to hand NaN straight
    // back out — and clamp is the only numeric guard the state loaders have.
    expect(clamp(Number.NaN, 0, 1)).toBe(0);
    expect(clamp(Number.POSITIVE_INFINITY, 0, 1)).toBe(1);
    expect(clamp(Number.NEGATIVE_INFINITY, -0.5, 0.5)).toBe(-0.5);
    expect(clamp(0.25, 0, 1)).toBe(0.25);
  });
});

describe('sanitizePad', () => {
  it('falls back to the factory pad for anything that is not an object', () => {
    const base = createDefaultPads()[4];
    for (const junk of [null, undefined, 'kick', 42, []]) {
      expect(sanitizePad(junk, 4)).toEqual(base);
    }
  });

  it('bounds a family and a model index the engine has no bank for', () => {
    const pad = sanitizePad({ family: 'gong', model: 99 }, 0);
    expect(pad.family).toBe('kick'); // the factory family for pad 1
    expect(pad.model).toBe(0);
    // A model index that is merely too large is clamped, exactly as native's AudioParameterChoice
    // clamps an out-of-range index rather than discarding the pad's model.
    const retargeted = sanitizePad({ family: 'hat', model: 77 }, 0);
    expect(retargeted.family).toBe('hat');
    expect(retargeted.model).toBe(3);
    expect(sanitizePad({ family: 'hat', model: -9 }, 0).model).toBe(0);
    expect(sanitizePad({ family: 'hat', model: 1.5 }, 0).model).toBe(0); // a fraction is not an index
  });

  it('bounds every numeric field instead of trusting storage', () => {
    const hostile = sanitizePad(
      {
        volume: 1e30,
        pan: -1e30,
        length: Number.NaN,
        pitch: '8',
        choke: 2.6,
        bus: 99,
        drumify: -3,
        instability: 5,
      },
      3,
    );
    expect(hostile.volume).toBe(6);
    expect(hostile.pan).toBe(-1);
    expect(hostile.length).toBe(createDefaultPads()[3].length); // NaN means the factory value
    expect(hostile.pitch).toBe(8); // a numeric string still means the number it spells
    expect(hostile.choke).toBe(3);
    expect(hostile.bus).toBe(8);
    expect(hostile.drumify).toBe(0);
    expect(hostile.instability).toBe(1);
  });

  it('keeps a start time inside the sample the kit points at', () => {
    expect(sanitizePad({ sampleStart: 12.5 }, 0, 4).sampleStart).toBe(4);
    expect(sanitizePad({ sampleStart: -1 }, 0, 4).sampleStart).toBe(0);
    expect(sanitizePad({ sampleStart: 1.5 }, 0, 4).sampleStart).toBe(1.5);
  });

  it('reads mute and solo as booleans, not as truthiness', () => {
    const pad = sanitizePad({ mute: 'false', solo: 1 }, 0);
    expect(pad.mute).toBe(false);
    expect(pad.solo).toBe(false);
  });

  it('renumbers the pad and its note from the index it is loaded into', () => {
    const pad = sanitizePad({ id: 99, note: 'C0' }, 11);
    expect(pad.id).toBe(11);
    expect(pad.note).not.toBe('C0');
  });
});

describe('sanitizeGlobal', () => {
  it('bounds each global field to the range native declares', () => {
    const g = sanitizeGlobal(
      { volume: 1e9, tone: Number.NaN, filter: 4, drumify: -50, outGain: '3', width: 0.5 },
      DEFAULT_GLOBAL,
    );
    expect(g.volume).toBe(6);
    expect(g.tone).toBe(0);
    expect(g.filter).toBe(1);
    expect(g.drumify).toBe(0);
    expect(g.outGain).toBe(3);
    expect(g.width).toBe(0.5);
  });

  it('keeps the passed-in fallback when the object holds nothing usable', () => {
    const fallback = { ...DEFAULT_GLOBAL, volume: -9 };
    expect(sanitizeGlobal(undefined, fallback).volume).toBe(-9);
    expect(sanitizeGlobal({}, fallback).volume).toBe(-9);
  });

  it('bounds the kit-level numbers that have no range table of their own', () => {
    expect(sanitizeNumber('0.5', 0.55, 0, 1)).toBe(0.5);
    expect(sanitizeNumber(undefined, 0.55, 0, 1)).toBe(0.55);
    expect(sanitizeNumber(Number.NaN, 0.55, 0, 1)).toBe(0.55);
    expect(sanitizeNumber(7, 0.55, 0, 1)).toBe(1);
    expect(sanitizeNumber(PAD_COUNT + 3, 0, 0, PAD_COUNT - 1)).toBe(PAD_COUNT - 1);
  });
});
