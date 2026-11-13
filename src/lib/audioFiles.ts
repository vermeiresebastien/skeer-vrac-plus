import { makeUserSample } from '../audio/sampleLibrary';
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
  if (blob.size > 160 * 1024 * 1024) {
    throw new Error('That file is larger than the 160 MB browser limit. Try a shorter excerpt.');
  }
  const ctx = getDecodeContext();
  const data = await blob.arrayBuffer();
  const buffer = await ctx.decodeAudioData(data.slice(0));
  if (buffer.duration < 0.05) throw new Error('That file is too short to slice.');
  if (buffer.duration > 10 * 60) throw new Error('That file is longer than the 10 minute browser limit.');

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
