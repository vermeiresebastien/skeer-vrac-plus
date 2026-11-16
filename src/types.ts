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

/** One row per pad, in the same order as kPadDefaults in native/Source/Params.cpp. The two
 *  editions ship the same factory kit, so a change to any column here belongs there too —
 *  including `length` and `pan`, which are per-pad values, not per-family ones: pads 7 and 8 are
 *  the same hat model at different lengths with opposite pan, and a family-keyed table cannot
 *  express that. */
const KIT_LAYOUT: Array<{
  family: Family;
  model: number;
  drumify: number;
  length: number;
  choke: number;
  pan: number;
}> = [
  { family: 'kick', model: 0, drumify: 0.72, length: 0.42, choke: 1, pan: 0 },
  { family: 'kick', model: 1, drumify: 0.58, length: 0.26, choke: 1, pan: 0 },
  { family: 'snare', model: 0, drumify: 0.68, length: 0.24, choke: 0, pan: 0 },
  { family: 'clap', model: 0, drumify: 0.62, length: 0.3, choke: 0, pan: 0 },
  { family: 'snare', model: 1, drumify: 0.76, length: 0.22, choke: 0, pan: 0 },
  { family: 'snare', model: 2, drumify: 0.48, length: 0.34, choke: 0, pan: 0 },
  { family: 'hat', model: 0, drumify: 0.8, length: 0.09, choke: 2, pan: -0.12 },
  { family: 'hat', model: 0, drumify: 0.64, length: 0.08, choke: 2, pan: 0.12 },
  { family: 'hat', model: 1, drumify: 0.85, length: 0.3, choke: 2, pan: 0 },
  { family: 'hat', model: 2, drumify: 0.55, length: 0.14, choke: 2, pan: -0.2 },
  { family: 'clap', model: 3, drumify: 0.7, length: 0.15, choke: 0, pan: 0 },
  { family: 'clap', model: 2, drumify: 0.52, length: 0.38, choke: 0, pan: 0 },
  { family: 'hat', model: 3, drumify: 0.6, length: 0.2, choke: 2, pan: 0.2 },
  { family: 'hat', model: 0, drumify: 0.9, length: 0.06, choke: 2, pan: 0 },
  { family: 'snare', model: 3, drumify: 0.66, length: 0.12, choke: 0, pan: 0 },
  { family: 'clap', model: 0, drumify: 0.74, length: 0.28, choke: 0, pan: 0 },
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
    length: layout.length,
    volume: 0,
    pan: layout.pan,
    choke: layout.choke,
    velocityAmount: 0.65,
    warp: 0.2,
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

/** Every comparison fails against NaN, so the two-sided test below would hand a corrupt saved
 *  value straight back out again — and this function is the only numeric guard the state loaders
 *  have. The infinities are still bounded the way juce::jlimit bounds them, so only a value that
 *  cannot be ordered at all falls back to the low end. */
export const clamp = (v: number, lo: number, hi: number): number =>
  v > hi ? hi : v < lo ? lo : Number.isFinite(v) ? v : lo;

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

/* ------------------------------------------------------------------ *
 * State sanitising
 * ------------------------------------------------------------------ */

/** The bounds native declares in createParameterLayout (native/Source/Params.cpp), repeated here so
 *  the browser edition refuses the same out-of-range values the VST3 does. */
type NumberRange = readonly [number, number];

const PAD_NUMBER_RANGES: Record<string, NumberRange> = {
  drumify: [0, 1],
  originalGain: [-24, 12],
  drumifiedGain: [-24, 12],
  pitch: [-24, 24],
  length: [0.02, 2],
  volume: [-60, 6],
  pan: [-1, 1],
  choke: [0, 4],
  velocityAmount: [0, 1],
  warp: [0, 1],
  impact: [0, 1],
  instability: [0, 1],
  toneShift: [-12, 12],
  super: [0, 1],
  eqLow: [-12, 12],
  eqMid: [-12, 12],
  eqHigh: [-12, 12],
  bus: [1, 8],
};

const GLOBAL_NUMBER_RANGES: Record<keyof GlobalParams, NumberRange> = {
  fineTune: [-50, 50],
  drumify: [0, 200],
  volume: [-24, 6],
  transpose: [-24, 24],
  length: [0.25, 2],
  tone: [-12, 12],
  filter: [-1, 1],
  width: [0, 1],
  clip: [0, 1],
  outGain: [-24, 12],
} satisfies Record<keyof GlobalParams, [number, number]>;

/** A value read out of localStorage, a dropped JSON file or an AudioParam is data, not a number:
 *  it can be a string, NaN, Infinity or absent. Anything that is not a finite number falls back to
 *  the factory value, then the whole thing is bounded — the same rejection native performs while
 *  decoding a host session, so a corrupt file cannot hand the engine a 1e30 gain. */
function boundNumber(raw: unknown, fallback: number, range: NumberRange): number {
  const value = typeof raw === 'number' ? raw : Number(raw);
  const start = Number.isFinite(value) ? value : fallback;
  return clamp(start, range[0], range[1]);
}

function boundInt(raw: unknown, fallback: number, range: NumberRange): number {
  return Math.round(boundNumber(raw, fallback, range));
}

/** The same binding sanitizePad performs, for the kit-level numbers that are read straight out of
 *  storage and have no range of their own to look up: a string or a NaN in localStorage reaches
 *  here rather than into the engine. */
export function sanitizeNumber(raw: unknown, fallback: number, lo: number, hi: number): number {
  return boundNumber(raw, fallback, [lo, hi]);
}

/** One pad from an untrusted object, with makePad(index) supplying whatever it does not carry.
 *  maxStart is the length of the source sample the kit points at, because a start time beyond the
 *  end of that file is the one value that cannot be bounded by a parameter range. */
export function sanitizePad(raw: unknown, index: number, maxStart = Number.POSITIVE_INFINITY): PadParams {
  const base = makePad(index);
  // Array.isArray is not paranoia: an array is an object, and its built-in length property reads
  // as a hit length of 0, which then clamps to the shortest pad the engine allows.
  if (typeof raw !== 'object' || raw === null || Array.isArray(raw)) return base;
  const source = raw as Record<string, unknown>;
  const familyOk = isFamily(source.family);
  const family = familyOk ? (source.family as Family) : base.family;
  const modelCount = MODEL_BANK[family].length;
  return {
    ...base,
    id: index,
    note: PAD_NOTES[index],
    family,
    // A model index is only meaningful inside the bank it was written against, so a pad whose
    // family was rejected does not get to keep a model number from the family it lost.
    model: familyOk
      ? clamp(Number.isInteger(source.model) ? (source.model as number) : base.model, 0, modelCount - 1)
      : base.model,
    sampleStart: boundNumber(source.sampleStart, base.sampleStart, [0, Math.max(0, maxStart)]),
    sliceIndex: Number.isInteger(source.sliceIndex) ? (source.sliceIndex as number) : base.sliceIndex,
    drumify: boundNumber(source.drumify, base.drumify, PAD_NUMBER_RANGES.drumify),
    originalGain: boundNumber(source.originalGain, base.originalGain, PAD_NUMBER_RANGES.originalGain),
    drumifiedGain: boundNumber(source.drumifiedGain, base.drumifiedGain, PAD_NUMBER_RANGES.drumifiedGain),
    pitch: boundNumber(source.pitch, base.pitch, PAD_NUMBER_RANGES.pitch),
    length: boundNumber(source.length, base.length, PAD_NUMBER_RANGES.length),
    volume: boundNumber(source.volume, base.volume, PAD_NUMBER_RANGES.volume),
    pan: boundNumber(source.pan, base.pan, PAD_NUMBER_RANGES.pan),
    choke: boundInt(source.choke, base.choke, PAD_NUMBER_RANGES.choke),
    velocityAmount: boundNumber(source.velocityAmount, base.velocityAmount, PAD_NUMBER_RANGES.velocityAmount),
    warp: boundNumber(source.warp, base.warp, PAD_NUMBER_RANGES.warp),
    impact: boundNumber(source.impact, base.impact, PAD_NUMBER_RANGES.impact),
    instability: boundNumber(source.instability, base.instability, PAD_NUMBER_RANGES.instability),
    toneShift: boundNumber(source.toneShift, base.toneShift, PAD_NUMBER_RANGES.toneShift),
    super: boundNumber(source.super, base.super, PAD_NUMBER_RANGES.super),
    eqLow: boundNumber(source.eqLow, base.eqLow, PAD_NUMBER_RANGES.eqLow),
    eqMid: boundNumber(source.eqMid, base.eqMid, PAD_NUMBER_RANGES.eqMid),
    eqHigh: boundNumber(source.eqHigh, base.eqHigh, PAD_NUMBER_RANGES.eqHigh),
    bus: boundInt(source.bus, base.bus, PAD_NUMBER_RANGES.bus),
    mute: source.mute === true,
    solo: source.solo === true,
  };
}

export function sanitizeGlobal(raw: unknown, fallback: GlobalParams): GlobalParams {
  const base = { ...DEFAULT_GLOBAL, ...fallback };
  if (typeof raw !== 'object' || raw === null || Array.isArray(raw)) return base;
  const source = raw as Record<string, unknown>;
  const out = { ...base };
  (Object.keys(GLOBAL_NUMBER_RANGES) as Array<keyof GlobalParams>).forEach((key) => {
    out[key] = boundNumber(source[key], base[key], GLOBAL_NUMBER_RANGES[key]);
  });
  return out;
}
