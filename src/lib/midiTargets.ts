import type { GlobalParams, PadParams } from '../types';
import { clamp } from '../types';

export type MidiTarget =
  | { scope: 'pad'; id: number; param: string }
  | { scope: 'global'; param: string };

const PAD_RANGES: Record<string, [number, number]> = {
  drumify: [0, 1],
  pitch: [-24, 24],
  length: [0.02, 2],
  volume: [-60, 6],
  pan: [-1, 1],
  originalGain: [-24, 12],
  drumifiedGain: [-24, 12],
  velocityAmount: [0, 1],
  warp: [0, 1],
  impact: [0, 1],
  instability: [0, 1],
  toneShift: [-12, 12],
  super: [0, 1],
  eqLow: [-12, 12],
  eqMid: [-12, 12],
  eqHigh: [-12, 12],
};

const GLOBAL_RANGES: Record<string, [number, number]> = {
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
};

export function parseMidiTarget(target: string): MidiTarget | null {
  const parts = target.split('.');
  if (parts[0] === 'global' && parts.length === 2 && GLOBAL_RANGES[parts[1]]) {
    return { scope: 'global', param: parts[1] };
  }
  if (parts[0] === 'pad' && parts.length === 3) {
    const id = Number(parts[1]);
    if (Number.isInteger(id) && id >= 0 && id < 16 && PAD_RANGES[parts[2]]) {
      return { scope: 'pad', id, param: parts[2] };
    }
  }
  return null;
}

export interface MidiApplyContext {
  updatePad: (id: number, patch: Partial<PadParams>) => void;
  setGlobal: (patch: Partial<GlobalParams>) => void;
}

export function applyMidiValue(target: MidiTarget, normalized: number, ctx: MidiApplyContext): void {
  const value = clamp(normalized, 0, 1);
  if (target.scope === 'pad') {
    const range = PAD_RANGES[target.param];
    if (!range) return;
    const mapped = range[0] + (range[1] - range[0]) * value;
    ctx.updatePad(target.id, { [target.param]: mapped } as Partial<PadParams>);
  } else {
    const range = GLOBAL_RANGES[target.param];
    if (!range) return;
    const mapped = range[0] + (range[1] - range[0]) * value;
    ctx.setGlobal({ [target.param]: mapped } as Partial<GlobalParams>);
  }
}

export function formatMidiTarget(target: string, pads: PadParams[]): string {
  const parsed = parseMidiTarget(target);
  if (!parsed) return target;
  const pretty = parsed.param
    .replace(/([A-Z])/g, ' $1')
    .replace(/^./, (c) => c.toUpperCase())
    .trim();
  if (parsed.scope === 'global') return `Global · ${pretty}`;
  const note = pads[parsed.id]?.note ?? `PAD ${parsed.id + 1}`;
  return `${note} · ${pretty}`;
}
