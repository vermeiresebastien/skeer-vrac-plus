import { makeUserSample } from '../audio/sampleLibrary';
import { MAX_FILE_BYTES, MAX_FILE_SECONDS, MIN_FILE_SECONDS } from './importLimits';
import type { SourceSample } from '../types';

let decodeContext: AudioContext | null = null;

function getDecodeContext(): AudioContext {
  if (!decodeContext) {
    const Ctor = window.AudioContext ?? (window as unknown as { webkitAudioContext: typeof AudioContext }).webkitAudioContext;
    decodeContext = new Ctor();
  }
  if (decodeContext.state === 'suspended') void decodeContext.resume();
  return decodeContext;
}

export async function decodeAudioBlob(blob: Blob, name: string): Promise<SourceSample> {
  // Three refusals, all of them stated by the ceiling's own number, so a message cannot describe
  // a limit that the code no longer has. See lib/importLimits.ts for why these are the values.
  const megabytes = Math.round(MAX_FILE_BYTES / (1024 * 1024));
  if (blob.size > MAX_FILE_BYTES) {
    throw new Error(`That file is larger than the ${megabytes} MB browser limit. Try a shorter excerpt.`);
  }
  const ctx = getDecodeContext();
  const data = await blob.arrayBuffer();
  const buffer = await ctx.decodeAudioData(data.slice(0));
  if (buffer.duration < MIN_FILE_SECONDS) throw new Error('That file is too short to slice.');
  if (buffer.duration > MAX_FILE_SECONDS) {
    throw new Error(`That file is longer than the ${MAX_FILE_SECONDS / 60} minute browser limit.`);
  }

  const left = new Float32Array(buffer.length);
  const right = new Float32Array(buffer.length);
  const l = buffer.getChannelData(0);
  left.set(l);
  if (buffer.numberOfChannels > 1) {
    right.set(buffer.getChannelData(1));
  } else {
    right.set(l);
  }
  return makeUserSample(name || 'MY SAMPLE', left, right, buffer.sampleRate);
}

export async function decodeAudioFile(file: File): Promise<SourceSample> {
  if (!file.type.startsWith('audio/') && !/\.(wav|aif|aiff|mp3|flac|ogg|m4a|webm)$/i.test(file.name)) {
    throw new Error('Drop an audio file (WAV, AIFF, MP3, FLAC, OGG, M4A).');
  }
  return decodeAudioBlob(file, file.name);
}

export const ACCEPTED_AUDIO = 'audio/*,.wav,.aif,.aiff,.mp3,.flac,.ogg,.m4a';
