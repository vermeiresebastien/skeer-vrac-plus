/* WAV encoder (PCM, 16/24-bit). Pure and testable. */

export function encodeWav(
  channels: Float32Array[],
  sampleRate: number,
  bitDepth: 16 | 24 = 24,
): ArrayBuffer {
  const channelCount = Math.max(1, channels.length);
  const frameCount = channels[0]?.length ?? 0;
  const bytesPerSample = bitDepth / 8;
  const blockAlign = channelCount * bytesPerSample;
  const dataSize = frameCount * blockAlign;
  const buffer = new ArrayBuffer(44 + dataSize);
  const view = new DataView(buffer);

  const writeString = (offset: number, value: string) => {
    for (let i = 0; i < value.length; i++) view.setUint8(offset + i, value.charCodeAt(i));
  };

  writeString(0, 'RIFF');
  view.setUint32(4, 36 + dataSize, true);
  writeString(8, 'WAVE');
  writeString(12, 'fmt ');
  view.setUint32(16, 16, true);
  view.setUint16(20, 1, true);
  view.setUint16(22, channelCount, true);
  view.setUint32(24, sampleRate, true);
  view.setUint32(28, sampleRate * blockAlign, true);
  view.setUint16(32, blockAlign, true);
  view.setUint16(34, bitDepth, true);
  writeString(36, 'data');
  view.setUint32(40, dataSize, true);

  const scale = bitDepth === 16 ? 32767 : 8388607;
  let offset = 44;
  for (let frame = 0; frame < frameCount; frame++) {
    for (let channel = 0; channel < channelCount; channel++) {
      const sample = Math.max(-1, Math.min(1, channels[channel]?.[frame] ?? 0));
      const value = Math.round(sample * scale);
      if (bitDepth === 16) {
        view.setInt16(offset, value, true);
        offset += 2;
      } else {
        view.setUint8(offset, value & 0xff);
        view.setUint8(offset + 1, (value >> 8) & 0xff);
        view.setUint8(offset + 2, (value >> 16) & 0xff);
        offset += 3;
      }
    }
  }
  return buffer;
}

export function wavBlob(channels: Float32Array[], sampleRate: number, bitDepth: 16 | 24 = 24): Blob {
  return new Blob([encodeWav(channels, sampleRate, bitDepth)], { type: 'audio/wav' });
}
