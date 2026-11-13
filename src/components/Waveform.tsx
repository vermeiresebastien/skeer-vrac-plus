import { useCallback, useEffect, useRef, useState, type PointerEvent as ReactPointerEvent } from 'react';
import type { SourceSample } from '../types';
import type { Slice } from '../audio/slicer';

export interface WaveformProps {
  sample: SourceSample;
  slices: Slice[];
  start: number;
  onStartChange?: (time: number) => void;
  onGestureStart?: () => void;
  viewStart?: number;
  viewEnd?: number;
  focusSeconds?: number;
  compact?: boolean;
  accent?: string;
  className?: string;
  showHandle?: boolean;
  ariaLabel?: string;
}

const CSS_FALLBACK = {
  wave: '#7c838c',
  waveDim: '#aeb4bb',
  accent: '#ff7a1a',
  line: 'rgba(20,22,25,.14)',
  region: 'rgba(255,122,26,.08)',
};

export function Waveform({
  sample,
  slices,
  start,
  onStartChange,
  onGestureStart,
  viewStart,
  viewEnd,
  focusSeconds,
  compact = false,
  accent,
  className = '',
  showHandle = true,
  ariaLabel = 'Sample start position',
}: WaveformProps) {
  const containerRef = useRef<HTMLDivElement | null>(null);
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const draggingRef = useRef(false);
  const gestureRef = useRef(false);
  const [size, setSize] = useState({ width: 0, height: 0 });
  const [dragTime, setDragTime] = useState<number | null>(null);

  const focusStart = focusSeconds ? Math.max(0, start - Math.min(0.22, focusSeconds * 0.12)) : 0;
  const focusEnd = focusSeconds ? Math.min(sample.duration, start + focusSeconds) : sample.duration;
  const vStart = viewStart ?? focusStart;
  const vEnd = viewEnd ?? focusEnd;
  const safeEnd = Math.max(vStart + 0.01, vEnd);
  const markerColor = accent ?? CSS_FALLBACK.accent;

  useEffect(() => {
    const container = containerRef.current;
    if (!container) return;
    const observer = new ResizeObserver((entries) => {
      const entry = entries[0];
      if (!entry) return;
      const { width, height } = entry.contentRect;
      setSize({ width: Math.max(1, Math.round(width)), height: Math.max(1, Math.round(height)) });
    });
    observer.observe(container);
    setSize({ width: Math.max(1, container.clientWidth), height: Math.max(1, container.clientHeight) });
    return () => observer.disconnect();
  }, []);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas || size.width <= 0 || size.height <= 0) return;
    const dpr = Math.min(2.5, window.devicePixelRatio || 1);
    canvas.width = Math.round(size.width * dpr);
    canvas.height = Math.round(size.height * dpr);
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    ctx.clearRect(0, 0, size.width, size.height);

    const computed = containerRef.current ? getComputedStyle(containerRef.current) : null;
    const waveColor = computed?.getPropertyValue('--waveform-wave').trim() || CSS_FALLBACK.wave;
    const dimColor = computed?.getPropertyValue('--waveform-dim').trim() || CSS_FALLBACK.waveDim;
    const regionColor = computed?.getPropertyValue('--waveform-region').trim() || CSS_FALLBACK.region;

    const w = size.width;
    const h = size.height;
    const mid = h / 2;
    const amp = Math.max(2, h * (compact ? 0.42 : 0.4));
    const span = safeEnd - vStart;
    const bucketCount = sample.peaks.count;
    const startX = ((start - vStart) / span) * w;
    const nextSlice = slices
      .filter((slice) => slice.time > start + 0.002)
      .reduce((closest, slice) => (slice.time < closest ? slice.time : closest), safeEnd);
    const endX = clamp(((Math.min(nextSlice, safeEnd) - vStart) / span) * w, 0, w);

    // Selected region tint.
    if (endX > startX) {
      ctx.fillStyle = regionColor;
      ctx.fillRect(startX, 0, endX - startX, h);
    }

    const drawWave = (clipStart: number, clipEnd: number, color: string) => {
      if (clipEnd <= clipStart) return;
      ctx.save();
      ctx.beginPath();
      ctx.rect(clipStart, 0, clipEnd - clipStart, h);
      ctx.clip();
      ctx.fillStyle = color;
      const xFrom = Math.max(0, Math.floor(clipStart));
      const xTo = Math.min(w, Math.ceil(clipEnd));
      for (let x = xFrom; x < xTo; x++) {
        const t0 = vStart + (x / w) * span;
        const t1 = vStart + ((x + 1) / w) * span;
        const b0 = Math.max(0, Math.min(bucketCount - 1, Math.floor((t0 / sample.duration) * bucketCount)));
        const b1 = Math.max(b0 + 1, Math.min(bucketCount, Math.ceil((t1 / sample.duration) * bucketCount)));
        let lo = 0;
        let hi = 0;
        for (let b = b0; b < b1; b++) {
          const min = sample.peaks.minMax[b * 2];
          const max = sample.peaks.minMax[b * 2 + 1];
          if (min < lo) lo = min;
          if (max > hi) hi = max;
        }
        const top = mid - hi * amp;
        const bottom = mid - lo * amp;
        ctx.fillRect(x, top, 1, Math.max(1, bottom - top));
      }
      ctx.restore();
    };

    // The part before the chosen start is a little dimmer: it's still visible,
    // but the ear can see which half of the sample the pad is using.
    drawWave(0, w, dimColor);
    drawWave(Math.max(0, startX), Math.min(w, endX), waveColor);

    // Slice ticks along the bottom.
    if (!compact) {
      ctx.save();
      for (const slice of slices) {
        if (slice.time < vStart || slice.time > safeEnd) continue;
        const x = ((slice.time - vStart) / span) * w;
        ctx.globalAlpha = 0.22 + slice.strength * 0.35;
        ctx.fillStyle = markerColor;
        ctx.fillRect(x, h - 7, 1, 7);
      }
      ctx.restore();
    }

    // Baseline.
    ctx.fillStyle = computed?.getPropertyValue('--waveform-line').trim() || CSS_FALLBACK.line;
    ctx.fillRect(0, mid, w, 1);

    // Selected start marker.
    if (startX >= -4 && startX <= w + 4) {
      const x = clamp(startX, 0.75, w - 0.75);
      ctx.save();
      ctx.fillStyle = markerColor;
      ctx.shadowColor = markerColor;
      ctx.shadowBlur = 7;
      ctx.fillRect(x - 0.75, 0, 1.5, h);
      ctx.restore();
      if (showHandle) {
        ctx.fillStyle = markerColor;
        ctx.beginPath();
        ctx.moveTo(x - 5, 1);
        ctx.lineTo(x + 5, 1);
        ctx.lineTo(x, 8);
        ctx.closePath();
        ctx.fill();
        ctx.beginPath();
        ctx.arc(x, 12, 2.2, 0, Math.PI * 2);
        ctx.fill();
      }
    }

    if (dragTime !== null && !compact) {
      const x = clamp(((dragTime - vStart) / span) * w, 0, w);
      ctx.fillStyle = 'rgba(255,255,255,.55)';
      ctx.fillRect(x, 0, 1, h);
    }
  }, [sample, slices, start, size, compact, markerColor, vStart, safeEnd, dragTime, showHandle]);

  const timeFromEvent = useCallback(
    (event: ReactPointerEvent<HTMLDivElement>): number => {
      const rect = event.currentTarget.getBoundingClientRect();
      const fraction = Math.max(0, Math.min(1, (event.clientX - rect.left) / rect.width));
      return Math.max(0, Math.min(sample.duration - 0.001, vStart + fraction * spanOf(vStart, safeEnd)));
    },
    [sample.duration, vStart, safeEnd],
  );

  const onPointerDown = (event: ReactPointerEvent<HTMLDivElement>) => {
    if (!onStartChange || event.button === 2) return;
    event.preventDefault();
    event.currentTarget.setPointerCapture(event.pointerId);
    draggingRef.current = true;
    if (!gestureRef.current) {
      gestureRef.current = true;
      onGestureStart?.();
    }
    const t = timeFromEvent(event);
    setDragTime(t);
    onStartChange(t);
  };

  const onPointerMove = (event: ReactPointerEvent<HTMLDivElement>) => {
    if (!draggingRef.current || !onStartChange) return;
    const t = timeFromEvent(event);
    setDragTime(t);
    onStartChange(t);
  };

  const endDrag = (event: ReactPointerEvent<HTMLDivElement>) => {
    if (!draggingRef.current) return;
    draggingRef.current = false;
    gestureRef.current = false;
    setDragTime(null);
    try {
      event.currentTarget.releasePointerCapture(event.pointerId);
    } catch {
      /* pointer capture may already be released */
    }
  };

  return (
    <div
      ref={containerRef}
      className={`waveform ${compact ? 'waveform--compact' : ''} ${className}`}
      role="group"
      aria-label={ariaLabel}
    >
      <canvas ref={canvasRef} className="waveform-canvas" />
      <div
        className="waveform-surface"
        style={{ touchAction: 'none', cursor: onStartChange ? 'ew-resize' : 'default' }}
        onPointerDown={onPointerDown}
        onPointerMove={onPointerMove}
        onPointerUp={endDrag}
        onPointerCancel={endDrag}
      />
      {dragTime !== null && (
        <span className="waveform-time-readout">{formatDragTime(dragTime)}</span>
      )}
    </div>
  );
}

function clamp(value: number, min: number, max: number): number {
  return value < min ? min : value > max ? max : value;
}

function spanOf(start: number, end: number): number {
  return Math.max(0.01, end - start);
}

function formatDragTime(time: number): string {
  const minutes = Math.floor(time / 60);
  const seconds = time - minutes * 60;
  return `${String(minutes).padStart(2, '0')}:${seconds.toFixed(3).padStart(6, '0')}`;
}
