/* Offline one-shot rendering and kit export.
 *
 * The voice engine is pure JavaScript, so a one-shot can be rendered directly
 * without opening a realtime AudioContext. Only the global master section is
 * applied through an OfflineAudioContext so the exported file matches what
 * the monitor path does. */

import type { GlobalParams, PadParams, SourceSample } from '../types';
import { PAD_COUNT, clamp } from '../types';
import { VracEngine } from './engineCore';
import { MasterChain } from './masterChain';
import { encodeWav } from './wav';
import { createZip, type ZipFile } from './zip';

const EXPORT_RATE = 48000;

export interface RenderInput {
  pad: PadParams;
  pads: PadParams[];
  global: GlobalParams;
  sample: SourceSample | null;
  sampleRate?: number;
}

export interface StereoPcm {
  left: Float32Array;
  right: Float32Array;
  sampleRate: number;
}

export function renderPadPcm(input: RenderInput): StereoPcm {
  const sampleRate = input.sampleRate ?? EXPORT_RATE;
  const length = clamp(
    Math.max(0.3, input.pad.length * clamp(input.global.length, 0.25, 2) * 6 + 0.25),
    0.3,
    10,
  );
  const totalFrames = Math.ceil(length * sampleRate);
  const engine = new VracEngine(sampleRate);
  if (input.sample) {
    engine.setSample(input.sample.left, input.sample.right, input.sample.sampleRate);
  }
  const pads = input.pads.map((pad) => ({ ...pad, mute: false, solo: false }));
  const padIndex = clamp(input.pad.id, 0, PAD_COUNT - 1);
  pads[padIndex] = { ...pads[padIndex], mute: false, solo: false };
  engine.setParams({ pads, global: input.global });
  engine.trigger(padIndex, 0.95);

  const busIndex = clamp(input.pad.bus - 1, 0, 7);
  const left = new Float32Array(totalFrames);
  const right = new Float32Array(totalFrames);
  const block = 128;
  let written = 0;
  while (written < totalFrames) {
    const outputs = Array.from({ length: 8 }, () => [
      new Float32Array(block),
      new Float32Array(block),
    ]) as Float32Array[][];
    engine.process(outputs);
    const bus = outputs[busIndex];
    const take = Math.min(block, totalFrames - written);
    left.set(bus[0].subarray(0, take), written);
    right.set(bus[1].subarray(0, take), written);
    written += take;
  }

  // Short fade-out so the end of the file is always clean.
  const fade = Math.min(Math.floor(sampleRate * 0.008), totalFrames);
  for (let i = 0; i < fade; i++) {
    const g = i / fade;
    left[totalFrames - 1 - i] *= g;
    right[totalFrames - 1 - i] *= g;
  }

  return { left, right, sampleRate };
}

/** Soft-clip fallback for environments without OfflineAudioContext. */
function fallbackMaster(input: StereoPcm, global: GlobalParams): StereoPcm {
  const gain = Math.pow(10, clamp(global.volume, -40, 6) / 20);
  const left = new Float32Array(input.left.length);
  const right = new Float32Array(input.right.length);
  for (let i = 0; i < left.length; i++) {
    left[i] = Math.tanh(input.left[i] * gain * 1.15);
    right[i] = Math.tanh(input.right[i] * gain * 1.15);
  }
  return { left, right, sampleRate: input.sampleRate };
}

export async function applyMaster(input: StereoPcm, global: GlobalParams): Promise<StereoPcm> {
  if (typeof OfflineAudioContext === 'undefined') return fallbackMaster(input, global);
  const length = input.left.length;
  if (length === 0) return input;
  const ctx = new OfflineAudioContext(2, length, input.sampleRate);
  const buffer = ctx.createBuffer(2, length, input.sampleRate);
  buffer.copyToChannel(input.left as Float32Array<ArrayBuffer>, 0);
  buffer.copyToChannel(input.right as Float32Array<ArrayBuffer>, 1);
  const source = ctx.createBufferSource();
  source.buffer = buffer;
  const chain = new MasterChain(ctx);
  chain.setGlobal(global, true);
  source.connect(chain.input);
  chain.connect(ctx.destination);
  source.start(0);
  const rendered = await ctx.startRendering();
  return {
    left: rendered.getChannelData(0).slice(),
    right: rendered.getChannelData(1).slice(),
    sampleRate: input.sampleRate,
  };
}

export async function renderPadWav(input: RenderInput): Promise<{ name: string; data: Uint8Array }> {
  const pcm = renderPadPcm(input);
  const mastered = await applyMaster(pcm, input.global);
  const wav = encodeWav([mastered.left, mastered.right], mastered.sampleRate, 24);
  const safeNote = input.pad.note.replace('#', 's');
  return {
    name: `${safeNote}_${input.pad.family.toUpperCase()}_${input.pad.bus}.wav`,
    data: new Uint8Array(wav),
  };
}

export function kitManifestJSON(
  pads: PadParams[],
  global: GlobalParams,
  source: SourceSample | null,
  kitName: string,
): Uint8Array {
  const manifest = {
    instrument: 'VRAC+',
    exportedAt: new Date().toISOString(),
    kit: kitName,
    source: source ? { name: source.name, duration: source.duration, sampleRate: source.sampleRate } : null,
    global,
    pads: pads.map((pad) => ({
      note: pad.note,
      family: pad.family,
      model: pad.model,
      bus: pad.bus,
      drumify: Number(pad.drumify.toFixed(3)),
      lengthMs: Math.round(pad.length * 1000),
      pitch: pad.pitch,
      pan: Number(pad.pan.toFixed(3)),
      volumeDb: pad.volume,
      choke: pad.choke,
      warp: pad.warp,
      impact: pad.impact,
      instability: pad.instability,
      toneShift: pad.toneShift,
      super: pad.super,
      eq: [pad.eqLow, pad.eqMid, pad.eqHigh],
    })),
  };
  return new TextEncoder().encode(JSON.stringify(manifest, null, 2));
}

export function readmeText(kitName: string): Uint8Array {
  return new TextEncoder().encode(
    `VRAC+ kit export — ${kitName}\n\n` +
      `One 24-bit / 48 kHz WAV per pad, named NOTE_FAMILY_OUTPUT.wav.\n` +
      `Every pad was rendered through the current kit settings and the global master section.\n` +
      `The output number at the end of each filename is the stereo bus the pad is routed to (1–8).\n\n` +
      `Drag the files into any DAW. For a quick start, put C1 on a kick track,\n` +
      `D1 on a snare track, F#1 on a hat track and D#1 on a clap track.\n\n` +
      `Created with VRAC+ — drumify any sound.\n`,
  );
}

export async function exportKitZip(
  pads: PadParams[],
  global: GlobalParams,
  sample: SourceSample | null,
  kitName: string,
  onProgress?: (done: number, total: number) => void,
): Promise<Blob> {
  const files: ZipFile[] = [];
  const total = PAD_COUNT + 2;
  let done = 0;
  for (const pad of pads.slice(0, PAD_COUNT)) {
    const wav = await renderPadWav({ pad, pads, global, sample });
    files.push(wav);
    done++;
    onProgress?.(done, total);
  }
  files.push({ name: 'kit.json', data: kitManifestJSON(pads, global, sample, kitName) });
  done++;
  onProgress?.(done, total);
  files.push({ name: 'README.txt', data: readmeText(kitName) });
  done++;
  onProgress?.(done, total);
  return new Blob([createZip(files) as unknown as BlobPart], { type: 'application/zip' });
}
