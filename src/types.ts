/* ------------------------------------------------------------------ *
 * SliceForge — shared types, factory constants and small pure helpers.
 * This module is imported by the UI, the audio engine and the tests.
 * It must stay free of DOM and Node dependencies.
 * ------------------------------------------------------------------ */

export type Family = 'kick' | 'snare' | 'hat' | 'clap';

export const FAMILIES: Family[] = ['kick', 'snare', 'hat', 'clap'];

export interface ModelDef {
  name: string;
  code: string;
  blurb: string;
}

export const MODEL_BANK: Record<Family, ModelDef[]> = {
  kick: [
    { name: 'ROUND', code: 'RND', blurb: 'Long, round body with a soft beater.' },
    { name: 'PUNCH', code: 'PCH', blurb: 'Fast pitch drop, tight and aggressive.' },
    { name: '808', code: '808', blurb: 'Deep sub tail that carries the note.' },
    { name: 'BROKEN', code: 'BRK', blurb: 'Driven, torn speaker texture.' },
  ],
  snare: [
    { name: 'CRACK', code: 'CRK', blurb: 'Wide noise crack with a woody tone.' },
    { name: 'TIGHT', code: 'TGT', blurb: 'Short, dry and close to the mic.' },
    { name: 'BRUSH', code: 'BRS', blurb: 'Loose brush sweep, soft transients.' },
    { name: 'RIM', code: 'RIM', blurb: 'Hard rim click, almost a woodblock.' },
  ],
  hat: [
    { name: 'CLOSED', code: 'CLS', blurb: 'Clipped, bright closed hat.' },
    { name: 'OPEN', code: 'OPN', blurb: 'Open choke-ready hat with air.' },
    { name: 'METAL', code: 'MTL', blurb: 'Six-square metallic machine hat.' },
    { name: 'AIR', code: 'AIR', blurb: 'Wide, breathy top end.' },
  ],
  clap: [
    { name: 'CLASSIC', code: 'CLP', blurb: 'Four-burst hand clap with tail.' },
    { name: 'TIGHT', code: 'TGT', blurb: 'Snappy, close and dry.' },
    { name: 'BIG', code: 'BIG', blurb: 'Room-sized crowd clap.' },
    { name: 'SNAP', code: 'SNP', blurb: 'Two-burst finger snap.' },
  ],
};

export const FAMILY_META: Record<
  Family,
  { label: string; plural: string; color: string; colorDim: string }
> = {
  kick: { label: 'KICK', plural: 'KICKS', color: '#ff4f7b', colorDim: '#b93055' },
  snare: { label: 'SNARE', plural: 'SNARES', color: '#ffd23f', colorDim: '#b89218' },
  hat: { label: 'HAT', plural: 'HATS', color: '#3fdac6', colorDim: '#1e8d83' },
  clap: { label: 'CLAP', plural: 'CLAPS', color: '#9ee45f', colorDim: '#619b2e' },
};

/** The sixteen pads, chromatic from C1 (bottom-left) to D#2 (top-right). */
export const PAD_NOTES = [
  'C1', 'C#1', 'D1', 'D#1',
  'E1', 'F1', 'F#1', 'G1',
  'G#1', 'A1', 'A#1', 'B1',
  'C2', 'C#2', 'D2', 'D#2',
] as const;

/** Conventional MIDI note numbers for the 16 pads (C1 = 36). */
export const PAD_MIDI_NOTES = PAD_NOTES.map((_, i) => 36 + i);

/** Keys that trigger the pads, laid out like the grid, top row first. */
export const PAD_KEYS = [
  ['1', '2', '3', '4'],
  ['Q', 'W', 'E', 'R'],
  ['A', 'S', 'D', 'F'],
  ['Z', 'X', 'C', 'V'],
] as const;

export interface GlobalParams {
  /** cents, -50..50 */
  fineTune: number;
  /** macro amount, 0..200 % — 100 % is neutral */
  drumify: number;
  /** dB, -24..6 */
  volume: number;
  /** semitones, -24..24 */
  transpose: number;
  /** 0.25..2.0, scales every pad length */
  length: number;
  /** semitones, -12..12 — spectral tilt */
  tone: number;
  /** bipolar, -1..1 — negative closes a low-pass, positive opens a high-pass */
  filter: number;
  /** 0..1 — stereo width of the kit */
  width: number;
  /** 0..1 — output soft clipper: 0 bypasses, 1 bends everything above 0.35 */
  clip: number;
  /** dB, -24..12 — output trim applied after the clipper */
  outGain: number;
}

export interface PadParams {
  id: number;
  note: string;
  family: Family;
  model: number;
  /** absolute start time in the source sample, seconds */
  sampleStart: number;
  /** which detected slice this start came from; -1 for a manually moved start */
  sliceIndex: number;
  /** original ↔ drumified crossfade, 0..1 */
  drumify: number;
  /** layer gains in dB */
  originalGain: number;
  drumifiedGain: number;
  /** semitones, -24..24 */
  pitch: number;
  /** seconds, 0.02..2 — per-pad hit length */
  length: number;
  /** dB, -60..6 */
  volume: number;
  /** -1..1 */
  pan: number;
  /** 0 = no choke, 1..4 = group */
  choke: number;
  /** 0..1 — how much velocity shapes level, length and tone */
  velocityAmount: number;
  /** 0..1 */
  warp: number;
  /** 0..1 */
  impact: number;
  /** 0..1 */
  instability: number;
  /** semitones, -12..12 */
  toneShift: number;
  /** 0..1 */
  super: number;
  /** dB, -12..12 */
  eqLow: number;
  eqMid: number;
  eqHigh: number;
  /** stereo output pair, 1..8 */
  bus: number;
  mute: boolean;
  solo: boolean;
}

export type SourceId = 'foley' | 'kitchen' | 'body' | 'scrap' | 'circuit' | 'vinyl';

export interface SourceInfo {
  id: SourceId;
  name: string;
  blurb: string;
  character: string;
  duration: number;
}

export const LIBRARY: SourceInfo[] = [
  {
    id: 'foley',
    name: 'FOLEY TRASH',
    blurb: 'Dustbin lids, broken wood and a squeaky hinge.',
    character: 'Cinematic · dry',
    duration: 13.2,
  },
  {
    id: 'kitchen',
    name: 'KITCHEN PERC',
    blurb: 'Pots, cutlery, glass and a stubborn drawer.',
    character: 'Tactile · bright',
    duration: 12.4,
  },
  {
    id: 'body',
    name: 'BODY MUSIC',
    blurb: 'Chest thumps, clicks, whispers and cheek pops.',
    character: 'Warm · human',
    duration: 11.8,
  },
  {
    id: 'scrap',
    name: 'SCRAP METAL',
    blurb: 'Sheet steel, springs, chains and a grinder.',
    character: 'Abrasive · wide',
    duration: 13.8,
  },
  {
    id: 'circuit',
    name: 'CIRCUIT NOISE',
    blurb: 'Relay clicks, coil whine and failing capacitors.',
    character: 'Electronic · tight',
    duration: 10.6,
  },
  {
    id: 'vinyl',
    name: 'VINYL DUST',
    blurb: 'Crackle, pops, run-out groove and a stuck note.',
    character: 'Dusty · lo-fi',
    duration: 12.1,
  },
];

export interface PeakData {
  /** buckets of min/max samples, interleaved as [min0,max0,min1,max1,…] */
  minMax: Float32Array;
  count: number;
}

export interface SourceSample {
  id: string;
  name: string;
  blurb: string;
  duration: number;
  sampleRate: number;
  left: Float32Array;
  right: Float32Array;
  peaks: PeakData;
  isUser: boolean;
}

export interface KitState {
  version: 1;
  presetName: string;
  sourceId: string;
  sourceName: string;
  sensitivity: number;
  autoSelect: boolean;
  global: GlobalParams;
  pads: PadParams[];
  selectedPad: number;
}

export const MAX_SLICES = 64;
export const PAD_COUNT = 16;

/* ------------------------------------------------------------------ *
 * Defaults
 * ------------------------------------------------------------------ */

export const DEFAULT_GLOBAL: GlobalParams = {
  fineTune: 0,
  drumify: 100,
  volume: -2.4,
  transpose: 0,
  length: 1,
  tone: 0,
  filter: 0,
  width: 0.65,
  clip: 0,
  outGain: 0,
};

const FAMILY_LENGTH: Record<Family, number> = {
  kick: 0.42,
  snare: 0.24,
  hat: 0.13,
  clap: 0.3,
};

const KIT_LAYOUT: Array<{ family: Family; model: number; drumify: number; choke: number }> = [
  { family: 'kick', model: 0, drumify: 0.72, choke: 1 },
  { family: 'kick', model: 1, drumify: 0.58, choke: 1 },
  { family: 'snare', model: 0, drumify: 0.68, choke: 0 },
  { family: 'clap', model: 0, drumify: 0.62, choke: 0 },
  { family: 'snare', model: 1, drumify: 0.76, choke: 0 },
  { family: 'snare', model: 2, drumify: 0.48, choke: 0 },
  { family: 'hat', model: 0, drumify: 0.8, choke: 2 },
  { family: 'hat', model: 0, drumify: 0.64, choke: 2 },
  { family: 'hat', model: 1, drumify: 0.85, choke: 2 },
  { family: 'hat', model: 2, drumify: 0.55, choke: 2 },
  { family: 'clap', model: 3, drumify: 0.7, choke: 0 },
  { family: 'clap', model: 2, drumify: 0.52, choke: 0 },
  { family: 'hat', model: 3, drumify: 0.6, choke: 2 },
  { family: 'hat', model: 0, drumify: 0.9, choke: 2 },
  { family: 'snare', model: 3, drumify: 0.66, choke: 0 },
  { family: 'clap', model: 0, drumify: 0.74, choke: 0 },
];

export function makePad(id: number): PadParams {
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
    solo: false,
  };
}

export function createDefaultPads(): PadParams[] {
  return Array.from({ length: PAD_COUNT }, (_, i) => makePad(i));
}

export function createInitialKit(name = 'BIG CAT'): KitState {
  return {
    version: 1,
    presetName: name,
    sourceId: 'foley',
    sourceName: 'FOLEY TRASH',
    sensitivity: 0.55,
    autoSelect: true,
    global: { ...DEFAULT_GLOBAL },
    pads: createDefaultPads(),
    selectedPad: 3,
  };
}

/* ------------------------------------------------------------------ *
 * Math / formatting helpers
 * ------------------------------------------------------------------ */

export const clamp = (v: number, lo: number, hi: number): number =>
  v < lo ? lo : v > hi ? hi : v;

export const lerp = (a: number, b: number, t: number): number => a + (b - a) * t;

export const inverseLerp = (a: number, b: number, v: number): number =>
  b === a ? 0 : clamp((v - a) / (b - a), 0, 1);

export const dbToGain = (db: number): number => (db <= -90 ? 0 : Math.pow(10, db / 20));

export const gainToDb = (gain: number): number =>
  gain <= 0.0000001 ? -120 : 20 * Math.log10(gain);

export const midiToFreq = (note: number): number => 440 * Math.pow(2, (note - 69) / 12);

export const semitoneRatio = (semitones: number): number => Math.pow(2, semitones / 12);

export const smoothstep = (edge0: number, edge1: number, x: number): number => {
  const t = clamp((x - edge0) / (edge1 - edge0 || 1e-7), 0, 1);
  return t * t * (3 - 2 * t);
};

export function panGains(pan: number): [number, number] {
  const p = clamp(pan, -1, 1);
  const angle = ((p + 1) * Math.PI) / 4;
  return [Math.cos(angle), Math.sin(angle)];
}

export function formatDb(db: number, digits = 1): string {
  if (db <= -59.95) return '-∞ dB';
  const sign = db > 0 ? '+' : '';
  return `${sign}${db.toFixed(digits)} dB`;
}

export function formatMs(seconds: number): string {
  const ms = seconds * 1000;
  if (ms < 100) return `${ms.toFixed(ms < 10 ? 1 : 0)} ms`;
  return `${Math.round(ms)} ms`;
}

export function formatPct(v: number, digits = 0): string {
  return `${v.toFixed(digits)}%`;
}

export function formatTime(seconds: number, withMs = false): string {
  const s = Math.max(0, seconds);
  const m = Math.floor(s / 60);
  const rest = s - m * 60;
  const whole = Math.floor(rest);
  const ms = Math.round((rest - whole) * 1000);
  const base = `${String(m).padStart(2, '0')}:${String(whole).padStart(2, '0')}`;
  return withMs ? `${base}.${String(ms).padStart(3, '0')}` : base;
}

export function formatCents(cents: number): string {
  const c = Math.round(cents);
  return `${c > 0 ? '+' : ''}${c} ct`;
}

export function formatSemis(semis: number): string {
  const v = Math.round(semis * 10) / 10;
  return `${v > 0 ? '+' : ''}${v.toFixed(Number.isInteger(v) ? 0 : 1)} st`;
}

export function familyColor(family: Family): string {
  return FAMILY_META[family].color;
}

export function modelLabel(pad: PadParams): string {
  return MODEL_BANK[pad.family][pad.model]?.name ?? '';
}

export function busLabel(bus: number): string {
  return `OUT ${bus}`;
}

/** Accepts a few spellings so simple preset files survive hand editing. */
export function isFamily(value: unknown): value is Family {
  return typeof value === 'string' && (FAMILIES as string[]).includes(value);
}
