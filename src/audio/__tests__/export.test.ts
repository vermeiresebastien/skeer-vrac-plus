import { describe, expect, it } from 'vitest';
import { encodeWav } from '../wav';
import { createZip } from '../zip';
import { renderPadPcm } from '../exportKit';
import { generateLibrarySample } from '../sampleLibrary';
import { DEFAULT_GLOBAL, createDefaultPads } from '../../types';

describe('wav encoder', () => {
  it('writes a valid 24-bit stereo header', () => {
    const left = new Float32Array([0, 0.5, -0.5]);
    const right = new Float32Array([0.25, -0.25, 0]);
    const buffer = encodeWav([left, right], 48000, 24);
    const view = new DataView(buffer);
    const text = (offset: number, len: number) =>
      String.fromCharCode(...new Uint8Array(buffer, offset, len));
    expect(text(0, 4)).toBe('RIFF');
    expect(text(8, 4)).toBe('WAVE');
    expect(text(12, 4)).toBe('fmt ');
    expect(view.getUint16(20, true)).toBe(1);
    expect(view.getUint16(22, true)).toBe(2);
    expect(view.getUint32(24, true)).toBe(48000);
    expect(view.getUint16(34, true)).toBe(24);
    expect(view.getUint32(40, true)).toBe(3 * 2 * 3);
    expect(buffer.byteLength).toBe(44 + 18);
  });

  it('clamps and writes 16-bit samples', () => {
    const buffer = encodeWav([new Float32Array([2, -2, 0.5])], 44100, 16);
    const view = new DataView(buffer);
    expect(view.getInt16(44, true)).toBe(32767);
    expect(view.getInt16(46, true)).toBe(-32767);
    expect(view.getInt16(48, true)).toBeGreaterThan(16000);
  });
});

describe('zip writer', () => {
  it('builds a store-method archive with a central directory', () => {
    const zip = createZip([
      { name: 'hello.txt', data: new TextEncoder().encode('hello') },
      { name: 'folder/world.bin', data: new Uint8Array([0, 1, 2, 3]) },
    ]);
    const view = new DataView(zip.buffer, zip.byteOffset, zip.byteLength);
    expect(view.getUint32(0, true)).toBe(0x04034b50);
    // End of central directory should point at a sane offset.
    const eocd = zip.length - 22;
    expect(view.getUint32(eocd, true)).toBe(0x06054b50);
    expect(view.getUint16(eocd + 10, true)).toBe(2);
    expect(view.getUint32(eocd + 12, true)).toBeGreaterThan(0);
    const text = new TextDecoder().decode(zip);
    expect(text).toContain('hello.txt');
    expect(text).toContain('folder/world.bin');
  });
});

describe('one-shot rendering', () => {
  it('renders a finite, audible kick one-shot', () => {
    const pads = createDefaultPads();
    const pcm = renderPadPcm({
      pad: pads[0],
      pads,
      global: { ...DEFAULT_GLOBAL },
      sample: generateLibrarySample('foley'),
    });
    expect(pcm.left.length).toBeGreaterThan(48000);
    expect(pcm.left.length).toBe(pcm.right.length);
    let peak = 0;
    let bad = 0;
    for (let i = 0; i < pcm.left.length; i++) {
      const l = pcm.left[i];
      const r = pcm.right[i];
      if (!Number.isFinite(l) || !Number.isFinite(r)) bad++;
      peak = Math.max(peak, Math.abs(l), Math.abs(r));
    }
    expect(bad).toBe(0);
    expect(peak).toBeGreaterThan(0.02);
    expect(peak).toBeLessThan(2);
  });

  it('renders silence into a muted pad only when overridden', () => {
    const pads = createDefaultPads().map((pad) => ({ ...pad, mute: true }));
    const pcm = renderPadPcm({
      pad: pads[3],
      pads,
      global: { ...DEFAULT_GLOBAL },
      sample: null,
    });
    let peak = 0;
    for (let i = 0; i < pcm.left.length; i++) peak = Math.max(peak, Math.abs(pcm.left[i]));
    expect(peak).toBeGreaterThan(0.01);
  });
});
