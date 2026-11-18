import { beforeEach, describe, expect, it, vi } from 'vitest';
import { formatTimer } from '../../components/RecordModal';
/* The native ceiling module, read as text. This is the Vite way to pull a file into a test: the
   project has no Node types (`types: ["vite/client"]`), and a `node:fs` import here would fail the
   only type gate, `tsc -b`. */
import nativeHeader from '../../../native/Source/Engine/ImportLimits.h?raw';
import {
  MAX_FILE_BYTES,
  MAX_FILE_SECONDS,
  MAX_TAKE_SECONDS,
  MIN_FILE_SECONDS,
  takeIsFull,
  takeLimitLabel,
} from '../importLimits';

/*  The import ceilings are a product promise, so they are gated three ways: the numbers
    themselves, the doors that enforce them, and the native edition that has to make the same
    promise from a different language. */

function nativeConstant(name: string): number {
  const found = new RegExp(`constexpr\\s+\\S+\\s+${name}\\s*=\\s*([0-9.]+)`).exec(nativeHeader);
  if (found == null) throw new Error(`${name} is not stated in Engine/ImportLimits.h`);
  return Number(found[1]);
}

/** kMaxRecordBytes is written as a power-of-two product, so read the factors rather than the text. */
function nativeRecordBytes(): number {
  const found = /kMaxRecordBytes\s*=\s*(\d+)\s*\*\s*(\d+)\s*\*\s*(\d+)/.exec(nativeHeader);
  if (found == null) throw new Error('kMaxRecordBytes is not stated as a byte product');
  return Number(found[1]) * Number(found[2]) * Number(found[3]);
}

const smallBlob = { size: 1024, arrayBuffer: async () => new ArrayBuffer(8) } as unknown as Blob;

/** Decodes every blob as `duration` seconds long, at a rate cheap enough to fake.
 * audioFiles.ts caches its decode context for the life of the module, so each call has to
 * re-import it or the first duration would answer for all of them. */
async function decodeAs(duration: number, blob: Blob, name: string, sampleRate = 1000) {
  vi.resetModules();
  class FakeAudioContext {
    state = 'running';

    async decodeAudioData() {
      const channel = new Float32Array(Math.max(1, Math.round(duration * sampleRate)));
      return {
        length: channel.length,
        duration,
        sampleRate,
        numberOfChannels: 1,
        getChannelData: () => channel,
      };
    }
  }
  (globalThis as unknown as Record<string, unknown>).window = { AudioContext: FakeAudioContext };
  const { decodeAudioBlob } = await import('../audioFiles');
  return decodeAudioBlob(blob, name);
}

beforeEach(() => {
  delete (globalThis as unknown as Record<string, unknown>).window;
});

describe('import ceilings', () => {
  it('promise five minutes of take and ten of file', () => {
    expect(MAX_TAKE_SECONDS).toBe(300);
    expect(MAX_FILE_SECONDS).toBe(600);
    expect(MIN_FILE_SECONDS).toBe(0.05);
    expect(MAX_FILE_BYTES).toBe(160 * 1024 * 1024);
  });

  it('state the take ceiling the same way the footer reads it', () => {
    expect(takeLimitLabel).toBe('Up to 5 minutes');
  });

  it('match the native edition, ceiling for ceiling', () => {
    expect(nativeConstant('kMaxTakeSeconds')).toBe(MAX_TAKE_SECONDS);
    expect(nativeConstant('kMaxFileSeconds')).toBe(MAX_FILE_SECONDS);
  });

  /*  The trap this whole change exists for: a longer promise with the old byte clamp silently
      shortens the take instead of lengthening it. Five minutes of stereo float32 at 96 kHz has
      to fit under the clamp, or the clamp - not the promise - is the ceiling. */
  it('leaves the native byte clamp wider than five minutes at 96 kHz', () => {
    const secondsAt96k = nativeRecordBytes() / (2 * 4 * 96000);
    expect(secondsAt96k).toBeGreaterThanOrEqual(MAX_TAKE_SECONDS);
  });
});

describe('takeIsFull', () => {
  it('stops at five minutes and not before', () => {
    expect(takeIsFull(120)).toBe(false); // the ceiling this replaced
    expect(takeIsFull(299.5)).toBe(false);
    expect(takeIsFull(300)).toBe(true);
    expect(takeIsFull(301)).toBe(true);
  });
});

describe('decodeAudioBlob gates', () => {
  it('refuses an over-size file before it touches the decoder', async () => {
    const huge = {
      size: MAX_FILE_BYTES + 1,
      arrayBuffer: async () => {
        throw new Error('the byte gate did not fire');
      },
    } as unknown as Blob;
    await expect(decodeAs(1, huge, 'HUGE.WAV')).rejects.toThrow(/larger than the 160 MB/);
  });

  it('refuses a file longer than the ten-minute ceiling', async () => {
    await expect(decodeAs(601, smallBlob, 'LONG.WAV')).rejects.toThrow(/longer than the 10 minute/);
  });

  it('refuses a file too short to hold an attack', async () => {
    await expect(decodeAs(0.01, smallBlob, 'BLIP.WAV')).rejects.toThrow(/too short to slice/);
  });

  it('accepts a five-minute take, which is what the record deck produces', async () => {
    const sample = await decodeAs(MAX_TAKE_SECONDS, smallBlob, 'RECORDED INPUT', 8000);
    expect(sample.duration).toBeCloseTo(MAX_TAKE_SECONDS, 3);
    expect(sample.left.length).toBe(MAX_TAKE_SECONDS * 8000);
    expect(sample.right.length).toBe(sample.left.length);
  });
});

describe('the record deck readout', () => {
  it('rolls the minutes field past a minute', () => {
    expect(formatTimer(0)).toBe('00:00.0');
    expect(formatTimer(59.94)).toBe('00:59.9');
    expect(formatTimer(119.9)).toBe('01:59.9');
    expect(formatTimer(MAX_TAKE_SECONDS)).toBe('05:00.0');
  });
});
