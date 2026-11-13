/* Factory kits. Each one changes the source, the slice sensitivity, the global
 * tone and the pad models so the six presets really are six kit characters,
 * not just six names. */

import type { GlobalParams, KitState, PadParams, SourceSample } from '../types';
import {
  DEFAULT_GLOBAL,
  PAD_NOTES,
  createDefaultPads,
  makePad,
} from '../types';
import { detectSlices, padStartsFromSlices, type Slice } from '../audio/slicer';
import { generateLibrarySample } from '../audio/sampleLibrary';

export interface SessionData {
  kit: KitState;
  sample: SourceSample;
  slices: Slice[];
}

export interface FactoryPreset {
  id: string;
  name: string;
  tag: string;
  blurb: string;
  sourceId: string;
  sensitivity: number;
  selectedPad: number;
  global: Partial<GlobalParams>;
  overrides: Record<number, Partial<PadParams>>;
}

export const FACTORY_PRESETS: FactoryPreset[] = [
  {
    id: 'big-cat',
    name: 'BIG CAT',
    tag: 'FOLEY BREAK',
    blurb: 'The house kit: fat foley kicks, crisp claps and tight machine hats.',
    sourceId: 'foley',
    sensitivity: 0.55,
    selectedPad: 3,
    global: { volume: -2.4, width: 0.68, drumify: 104 },
    overrides: {
      0: { family: 'kick', model: 0, length: 0.46, drumify: 0.7, impact: 0.42, warp: 0.28 },
      1: { family: 'kick', model: 1, length: 0.27, drumify: 0.62, pitch: -1, impact: 0.55 },
      2: { family: 'snare', model: 1, length: 0.2, drumify: 0.76, impact: 0.5, pan: 0.04 },
      3: { family: 'clap', model: 0, length: 0.31, drumify: 0.78, super: 0.32 },
      4: { family: 'snare', model: 0, length: 0.28, drumify: 0.7, pitch: -2 },
      5: { family: 'snare', model: 2, length: 0.36, drumify: 0.56 },
      6: { family: 'hat', model: 0, length: 0.085, drumify: 0.86, choke: 2, pan: 0.18 },
      7: { family: 'hat', model: 0, length: 0.07, drumify: 0.8, choke: 2, pan: -0.12 },
      8: { family: 'hat', model: 1, length: 0.33, drumify: 0.86, choke: 2, pan: 0.3 },
      9: { family: 'hat', model: 2, length: 0.13, drumify: 0.72, choke: 2, pan: -0.3 },
      10: { family: 'clap', model: 3, length: 0.17, drumify: 0.8 },
      11: { family: 'clap', model: 2, length: 0.42, drumify: 0.6 },
      12: { family: 'hat', model: 3, length: 0.2, drumify: 0.7, pan: 0.22 },
      13: { family: 'hat', model: 0, length: 0.055, drumify: 0.92, choke: 2 },
      14: { family: 'snare', model: 3, length: 0.12, drumify: 0.72 },
      15: { family: 'clap', model: 0, length: 0.27, drumify: 0.82 },
    },
  },
  {
    id: 'night-kitchen',
    name: 'NIGHT KITCHEN',
    tag: 'CLATTER',
    blurb: 'Tight pots and cutlery, filtered for a late-night club room.',
    sourceId: 'kitchen',
    sensitivity: 0.64,
    selectedPad: 3,
    global: { volume: -3.2, width: 0.78, length: 0.88, filter: -0.11, tone: -0.8, drumify: 116 },
    overrides: {
      0: { family: 'kick', model: 1, length: 0.3, drumify: 0.8, impact: 0.62, warp: 0.34 },
      1: { family: 'kick', model: 0, length: 0.4, drumify: 0.64, pitch: 2 },
      2: { family: 'snare', model: 1, length: 0.17, drumify: 0.82, impact: 0.6 },
      3: { family: 'clap', model: 1, length: 0.19, drumify: 0.86, super: 0.4 },
      4: { family: 'snare', model: 3, length: 0.11, drumify: 0.78, pan: -0.1 },
      5: { family: 'snare', model: 2, length: 0.28, drumify: 0.6, pan: 0.2 },
      6: { family: 'hat', model: 0, length: 0.055, drumify: 0.9, choke: 2, pan: 0.24 },
      7: { family: 'hat', model: 0, length: 0.06, drumify: 0.86, choke: 2, pan: -0.18 },
      8: { family: 'hat', model: 1, length: 0.22, drumify: 0.9, choke: 2, pan: 0.32 },
      9: { family: 'hat', model: 2, length: 0.09, drumify: 0.8, choke: 2, pan: -0.28 },
      10: { family: 'clap', model: 3, length: 0.13, drumify: 0.84 },
      11: { family: 'clap', model: 0, length: 0.28, drumify: 0.72 },
      12: { family: 'hat', model: 3, length: 0.16, drumify: 0.8, pan: 0.28 },
      13: { family: 'hat', model: 0, length: 0.05, drumify: 0.94, choke: 2 },
      14: { family: 'snare', model: 0, length: 0.24, drumify: 0.72 },
      15: { family: 'clap', model: 2, length: 0.36, drumify: 0.78 },
    },
  },
  {
    id: 'body-talk',
    name: 'BODY TALK',
    tag: 'HUMAN PERC',
    blurb: 'Chest thumps and mouth clicks, warm and loose under the fingers.',
    sourceId: 'body',
    sensitivity: 0.5,
    selectedPad: 2,
    global: { volume: -2.8, width: 0.62, length: 1.12, tone: -2.4, drumify: 96 },
    overrides: {
      0: { family: 'kick', model: 2, length: 0.52, drumify: 0.6, pitch: -1 },
      1: { family: 'kick', model: 0, length: 0.44, drumify: 0.52, pitch: -2 },
      2: { family: 'snare', model: 2, length: 0.34, drumify: 0.62, pan: 0.08 },
      3: { family: 'clap', model: 2, length: 0.44, drumify: 0.66, super: 0.2 },
      4: { family: 'snare', model: 0, length: 0.26, drumify: 0.68 },
      5: { family: 'snare', model: 2, length: 0.4, drumify: 0.5 },
      6: { family: 'hat', model: 3, length: 0.09, drumify: 0.76, choke: 2, pan: 0.2 },
      7: { family: 'hat', model: 3, length: 0.075, drumify: 0.7, choke: 2, pan: -0.2 },
      8: { family: 'hat', model: 1, length: 0.4, drumify: 0.62, choke: 2 },
      9: { family: 'hat', model: 3, length: 0.14, drumify: 0.6, choke: 2, pan: -0.3 },
      10: { family: 'clap', model: 0, length: 0.32, drumify: 0.7 },
      11: { family: 'clap', model: 2, length: 0.5, drumify: 0.56 },
      12: { family: 'hat', model: 3, length: 0.22, drumify: 0.6, pan: 0.26 },
      13: { family: 'hat', model: 0, length: 0.06, drumify: 0.8, choke: 2 },
      14: { family: 'snare', model: 3, length: 0.14, drumify: 0.64 },
      15: { family: 'clap', model: 2, length: 0.4, drumify: 0.68 },
    },
  },
  {
    id: 'iron-works',
    name: 'IRON WORKS',
    tag: 'HEAVY METAL',
    blurb: 'Scrap steel. Impact up, decay down, driven through the console.',
    sourceId: 'scrap',
    sensitivity: 0.7,
    selectedPad: 0,
    global: { volume: -4.1, width: 0.9, length: 0.8, drumify: 138, tone: 2.2, filter: 0.04 },
    overrides: {
      0: { family: 'kick', model: 3, length: 0.35, drumify: 0.92, impact: 0.85, super: 0.5, warp: 0.5 },
      1: { family: 'kick', model: 1, length: 0.24, drumify: 0.88, impact: 0.9, pitch: -3, super: 0.4 },
      2: { family: 'snare', model: 0, length: 0.26, drumify: 0.9, impact: 0.8, super: 0.65 },
      3: { family: 'clap', model: 3, length: 0.2, drumify: 0.92, super: 0.6 },
      4: { family: 'snare', model: 1, length: 0.2, drumify: 0.86, impact: 0.7 },
      5: { family: 'snare', model: 3, length: 0.18, drumify: 0.88, impact: 0.75 },
      6: { family: 'hat', model: 2, length: 0.07, drumify: 0.95, choke: 2, pan: 0.26, super: 0.3 },
      7: { family: 'hat', model: 2, length: 0.06, drumify: 0.9, choke: 2, pan: -0.22 },
      8: { family: 'hat', model: 1, length: 0.24, drumify: 0.92, choke: 2, pan: 0.34 },
      9: { family: 'hat', model: 2, length: 0.11, drumify: 0.94, choke: 2, pan: -0.34 },
      10: { family: 'clap', model: 3, length: 0.15, drumify: 0.9 },
      11: { family: 'clap', model: 2, length: 0.3, drumify: 0.84 },
      12: { family: 'hat', model: 2, length: 0.18, drumify: 0.88, pan: 0.3 },
      13: { family: 'hat', model: 0, length: 0.05, drumify: 0.95, choke: 2 },
      14: { family: 'snare', model: 3, length: 0.14, drumify: 0.9, impact: 0.8 },
      15: { family: 'clap', model: 2, length: 0.32, drumify: 0.9, super: 0.6 },
    },
  },
  {
    id: 'short-circuit',
    name: 'SHORT CIRCUIT',
    tag: 'GLITCH',
    blurb: 'Relay clicks and coil whine, unstable by design.',
    sourceId: 'circuit',
    sensitivity: 0.76,
    selectedPad: 10,
    global: { volume: -3.6, width: 0.86, length: 0.64, drumify: 122, transpose: -3, filter: 0.07, tone: 2.8 },
    overrides: {
      0: { family: 'kick', model: 1, length: 0.2, drumify: 0.8, instability: 0.28, impact: 0.6 },
      1: { family: 'kick', model: 3, length: 0.16, drumify: 0.9, pitch: 5, instability: 0.4, super: 0.5 },
      2: { family: 'snare', model: 1, length: 0.12, drumify: 0.88, instability: 0.5 },
      3: { family: 'clap', model: 3, length: 0.1, drumify: 0.9, instability: 0.6, pitch: 4 },
      4: { family: 'snare', model: 3, length: 0.08, drumify: 0.9, pitch: 7, instability: 0.5 },
      5: { family: 'snare', model: 0, length: 0.17, drumify: 0.82, instability: 0.48 },
      6: { family: 'hat', model: 0, length: 0.045, drumify: 0.95, choke: 2, instability: 0.4, pan: 0.2 },
      7: { family: 'hat', model: 2, length: 0.05, drumify: 0.95, choke: 2, instability: 0.4, pan: -0.2 },
      8: { family: 'hat', model: 1, length: 0.15, drumify: 0.94, choke: 2 },
      9: { family: 'hat', model: 3, length: 0.08, drumify: 0.86, choke: 2, pan: -0.32 },
      10: { family: 'clap', model: 1, length: 0.09, drumify: 0.94, instability: 0.55, pitch: 3 },
      11: { family: 'clap', model: 3, length: 0.13, drumify: 0.9, instability: 0.55 },
      12: { family: 'hat', model: 2, length: 0.11, drumify: 0.92, instability: 0.5, pan: 0.3 },
      13: { family: 'hat', model: 0, length: 0.04, drumify: 0.98, choke: 2, instability: 0.35 },
      14: { family: 'snare', model: 3, length: 0.09, drumify: 0.92, pitch: 9, instability: 0.6 },
      15: { family: 'clap', model: 0, length: 0.18, drumify: 0.9, instability: 0.5 },
    },
  },
  {
    id: 'dust-loop',
    name: 'DUST LOOP',
    tag: 'LO-FI',
    blurb: 'Vinyl crackle and run-out groove, mostly original and beautifully worn.',
    sourceId: 'vinyl',
    sensitivity: 0.45,
    selectedPad: 15,
    global: { volume: -2.6, width: 0.52, length: 1.35, drumify: 44, filter: -0.17, tone: -4.2, fineTune: -12 },
    overrides: {
      0: { family: 'kick', model: 2, length: 0.55, drumify: 0.34, originalGain: -1.2, drumifiedGain: -8.5, pitch: -3 },
      1: { family: 'kick', model: 0, length: 0.48, drumify: 0.28, originalGain: -1.6, drumifiedGain: -9, pitch: -2 },
      2: { family: 'snare', model: 2, length: 0.36, drumify: 0.38, originalGain: -1.4, drumifiedGain: -8.6 },
      3: { family: 'clap', model: 0, length: 0.44, drumify: 0.34, originalGain: -2, drumifiedGain: -9 },
      4: { family: 'snare', model: 0, length: 0.3, drumify: 0.4, originalGain: -1.8, drumifiedGain: -8.4 },
      5: { family: 'snare', model: 2, length: 0.4, drumify: 0.3, originalGain: -2.2, drumifiedGain: -9.2 },
      6: { family: 'hat', model: 0, length: 0.11, drumify: 0.42, originalGain: -2, drumifiedGain: -8, choke: 2, pan: 0.18 },
      7: { family: 'hat', model: 0, length: 0.1, drumify: 0.38, originalGain: -2.4, drumifiedGain: -8, choke: 2, pan: -0.18 },
      8: { family: 'hat', model: 1, length: 0.4, drumify: 0.36, originalGain: -2.2, drumifiedGain: -8.5, choke: 2 },
      9: { family: 'hat', model: 3, length: 0.18, drumify: 0.34, originalGain: -2.4, drumifiedGain: -9, choke: 2 },
      10: { family: 'clap', model: 3, length: 0.22, drumify: 0.4, originalGain: -1.8, drumifiedGain: -8.2 },
      11: { family: 'clap', model: 2, length: 0.46, drumify: 0.28, originalGain: -2.2, drumifiedGain: -9 },
      12: { family: 'hat', model: 3, length: 0.28, drumify: 0.3, originalGain: -2.4, drumifiedGain: -9 },
      13: { family: 'hat', model: 0, length: 0.09, drumify: 0.44, originalGain: -2.4, drumifiedGain: -8 },
      14: { family: 'snare', model: 3, length: 0.18, drumify: 0.36, originalGain: -2, drumifiedGain: -8.6 },
      15: { family: 'clap', model: 2, length: 0.52, drumify: 0.3, originalGain: -1.6, drumifiedGain: -9 },
    },
  },
];

export function getFactoryPreset(id: string): FactoryPreset {
  return FACTORY_PRESETS.find((preset) => preset.id === id) ?? FACTORY_PRESETS[0];
}

/** Build a complete playable session for a factory preset. */
export function buildPresetSession(id: string): SessionData {
  const preset = getFactoryPreset(id);
  const sample = generateLibrarySample(preset.sourceId as never);
  const { slices } = detectSlices(sample.left, sample.right, sample.sampleRate, preset.sensitivity);
  let pads = padStartsFromSlices(slices, createDefaultPads());
  pads = pads.map((pad, index) => ({
    ...pad,
    ...(preset.overrides[index] ?? {}),
    id: index,
    note: PAD_NOTES[index],
  }));
  const kit: KitState = {
    version: 1,
    presetName: preset.name,
    sourceId: preset.sourceId,
    sourceName: sample.name,
    sensitivity: preset.sensitivity,
    autoSelect: true,
    global: { ...DEFAULT_GLOBAL, ...preset.global },
    pads,
    selectedPad: preset.selectedPad,
  };
  return { kit, sample, slices };
}

/** Build a session from the first preset (used on boot). */
export function buildInitialSession(): SessionData {
  return buildPresetSession('big-cat');
}

/** Keep the current pad design when a different source is loaded. */
export function buildSourceSession(
  sourceId: string,
  name: string,
  sample: SourceSample,
  previous: KitState,
  sensitivity = previous.sensitivity,
): SessionData {
  const { slices } = detectSlices(sample.left, sample.right, sample.sampleRate, sensitivity);
  const pads = padStartsFromSlices(slices, previous.pads.map((pad) => ({ ...pad })));
  const kit: KitState = {
    ...previous,
    presetName: name,
    sourceId,
    sourceName: sample.name,
    sensitivity,
    pads,
    selectedPad: previous.selectedPad,
  };
  return { kit, sample, slices };
}

export function makeEmptyPad(index: number): PadParams {
  return makePad(index);
}
