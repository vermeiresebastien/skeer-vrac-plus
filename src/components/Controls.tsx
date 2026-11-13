import {
  useCallback,
  useEffect,
  useRef,
  useState,
  type FocusEvent,
  type KeyboardEvent,
  type PointerEvent as ReactPointerEvent,
} from 'react';
import { clamp } from '../types';
import { Icon } from './Icons';

/* ------------------------------------------------------------------ *
 * Knob
 * ------------------------------------------------------------------ */

type KnobSize = 'xs' | 'sm' | 'md' | 'lg';

export interface KnobProps {
  label: string;
  value: number;
  min: number;
  max: number;
  defaultValue?: number;
  step?: number;
  size?: KnobSize;
  format: (value: number) => string;
  onChange: (value: number) => void;
  onBegin?: () => void;
  onLearn?: () => void;
  onEnd?: () => void;
  accent?: string;
  bipolar?: boolean;
  disabled?: boolean;
  title?: string;
  className?: string;
}

const START_ANGLE = -135;
const SWEEP = 270;

function polar(cx: number, cy: number, radius: number, angle: number): [number, number] {
  const rad = (angle * Math.PI) / 180;
  return [cx + radius * Math.sin(rad), cy - radius * Math.cos(rad)];
}

function arcPath(from: number, to: number, radius = 26): string {
  const [sx, sy] = polar(32, 32, radius, from);
  const [ex, ey] = polar(32, 32, radius, to);
  const large = Math.abs(to - from) > 180 ? 1 : 0;
  const sweep = to >= from ? 1 : 0;
  return `M ${sx.toFixed(2)} ${sy.toFixed(2)} A ${radius} ${radius} 0 ${large} ${sweep} ${ex.toFixed(2)} ${ey.toFixed(2)}`;
}

function snap(value: number, step: number | undefined, min: number, max: number): number {
  const bounded = clamp(value, min, max);
  if (!step || step <= 0) return bounded;
  const snapped = Math.round((bounded - min) / step) * step + min;
  return clamp(Number(snapped.toFixed(6)), min, max);
}

export function Knob({
  label,
  value,
  min,
  max,
  defaultValue,
  step,
  size = 'sm',
  format,
  onChange,
  onBegin,
  onLearn,
  onEnd,
  accent,
  bipolar = false,
  disabled = false,
  title,
  className = '',
}: KnobProps) {
  const wrapRef = useRef<HTMLDivElement | null>(null);
  const dragRef = useRef<{ startY: number; startValue: number; moved: boolean } | null>(null);
  const wheelTimer = useRef(0);
  const [dragging, setDragging] = useState(false);
  const range = max - min;
  const normalized = range === 0 ? 0 : (value - min) / range;
  const angle = START_ANGLE + normalized * SWEEP;
  const centerAngle = START_ANGLE + (bipolar ? 0.5 : 0) * SWEEP;
  const startAngle = bipolar ? Math.min(centerAngle, angle) : START_ANGLE;
  const endAngle = bipolar ? Math.max(centerAngle, angle) : angle;
  const fillColor = accent ?? 'var(--accent)';

  const reset = useCallback(() => {
    if (disabled) return;
    onBegin?.();
    onChange(snap(defaultValue ?? min + range / 2, step, min, max));
  }, [disabled, onBegin, onChange, defaultValue, min, range, step, max]);

  const commit = useCallback(
    (next: number, withBegin = false) => {
      if (disabled) return;
      if (withBegin) onBegin?.();
      onChange(snap(next, step, min, max));
    },
    [disabled, onBegin, onChange, step, min, max],
  );

  const onPointerDown = (event: ReactPointerEvent<HTMLDivElement>) => {
    if (disabled || event.button === 2) return;
    event.preventDefault();
    wrapRef.current?.focus();
    wrapRef.current?.setPointerCapture(event.pointerId);
    dragRef.current = { startY: event.clientY, startValue: value, moved: false };
    setDragging(true);
    onBegin?.();
  };

  const onPointerMove = (event: ReactPointerEvent<HTMLDivElement>) => {
    const drag = dragRef.current;
    if (!drag) return;
    const dy = drag.startY - event.clientY;
    if (Math.abs(dy) > 1) drag.moved = true;
    const speed = event.shiftKey ? 760 : event.altKey ? 1400 : 210;
    const next = drag.startValue + (dy / speed) * range;
    onChange(snap(next, step, min, max));
  };

  const endDrag = (event: ReactPointerEvent<HTMLDivElement>) => {
    if (!dragRef.current) return;
    dragRef.current = null;
    setDragging(false);
    onEnd?.();
    try {
      wrapRef.current?.releasePointerCapture(event.pointerId);
    } catch {
      /* pointer capture may already be gone */
    }
  };

  useEffect(() => {
    const el = wrapRef.current;
    if (!el || disabled) return;
    const handleWheel = (event: WheelEvent) => {
      event.preventDefault();
      const now = performance.now();
      if (now - wheelTimer.current > 380) onBegin?.();
      wheelTimer.current = now;
      const sensitivity = event.shiftKey ? 0.001 : 0.004;
      const next = value + (-event.deltaY / 100) * range * sensitivity;
      onChange(snap(next, step, min, max));
    };
    el.addEventListener('wheel', handleWheel, { passive: false });
    return () => el.removeEventListener('wheel', handleWheel);
  }, [disabled, onBegin, onChange, value, range, step, min, max]);

  const onKeyDown = (event: KeyboardEvent<HTMLDivElement>) => {
    if (disabled) return;
    const baseStep = step ?? range / 100;
    const amount = event.shiftKey ? baseStep * 10 : baseStep;
    let next: number | null = null;
    if (event.key === 'ArrowUp' || event.key === 'ArrowRight') next = value + amount;
    else if (event.key === 'ArrowDown' || event.key === 'ArrowLeft') next = value - amount;
    else if (event.key === 'PageUp') next = value + range * 0.1;
    else if (event.key === 'PageDown') next = value - range * 0.1;
    else if (event.key === 'Home') next = min;
    else if (event.key === 'End') next = max;
    if (next !== null) {
      event.preventDefault();
      commit(next, true);
    }
  };

  const onBlur = (_event: FocusEvent<HTMLDivElement>) => {
    if (dragRef.current) {
      dragRef.current = null;
      setDragging(false);
      onEnd?.();
    }
  };

  return (
    <div
      className={`knob knob--${size} ${dragging ? 'is-dragging' : ''} ${disabled ? 'is-disabled' : ''} ${className}`}
      style={{ ['--knob-accent' as string]: fillColor }}
      title={title}
    >
      <span className="knob-label">{label}</span>
      <div
        ref={wrapRef}
        className="knob-dial"
        role="slider"
        tabIndex={disabled ? -1 : 0}
        aria-label={label}
        aria-valuemin={min}
        aria-valuemax={max}
        aria-valuenow={Number(value.toFixed(3))}
        aria-valuetext={format(value)}
        aria-disabled={disabled}
        onPointerDown={onPointerDown}
        onPointerMove={onPointerMove}
        onPointerUp={endDrag}
        onPointerCancel={endDrag}
        onDoubleClick={reset}
        onKeyDown={onKeyDown}
        onBlur={onBlur}
        onContextMenu={(event) => {
          if (!onLearn) return;
          event.preventDefault();
          onLearn();
        }}
      >
        <svg className="knob-arc" viewBox="0 0 64 64" aria-hidden="true">
          {Array.from({ length: 19 }).map((_, i) => {
            const tickAngle = START_ANGLE + (i / 18) * SWEEP;
            const [x1, y1] = polar(32, 32, 30.2, tickAngle);
            const [x2, y2] = polar(32, 32, 28.4, tickAngle);
            return (
              <line
                key={i}
                x1={x1}
                y1={y1}
                x2={x2}
                y2={y2}
                className="knob-tick"
              />
            );
          })}
          <path d={arcPath(START_ANGLE, START_ANGLE + SWEEP)} className="knob-track" />
          {Math.abs(endAngle - startAngle) > 0.4 && (
            <path d={arcPath(startAngle, endAngle)} className="knob-fill" style={{ stroke: fillColor }} />
          )}
        </svg>
        <span className="knob-body">
          <span className="knob-indicator" style={{ transform: `rotate(${angle}deg)` }} />
        </span>
      </div>
      <span className="knob-value">{format(value)}</span>
    </div>
  );
}

/* ------------------------------------------------------------------ *
 * Slider
 * ------------------------------------------------------------------ */

export interface SliderProps {
  label?: string;
  value: number;
  min: number;
  max: number;
  step?: number;
  format?: (value: number) => string;
  onChange: (value: number) => void;
  onBegin?: () => void;
  accent?: string;
  className?: string;
  ariaLabel?: string;
  compact?: boolean;
}

export function Slider({
  label,
  value,
  min,
  max,
  step = 0.01,
  format,
  onChange,
  onBegin,
  accent,
  className = '',
  ariaLabel,
  compact = false,
}: SliderProps) {
  const fill = ((value - min) / (max - min)) * 100;
  return (
    <div className={`slider ${compact ? 'slider--compact' : ''} ${className}`} style={{ ['--slider-accent' as string]: accent }}>
      {(label || format) && (
        <div className="slider-head">
          <span className="slider-label">{label}</span>
          {format && <span className="slider-value">{format(value)}</span>}
        </div>
      )}
      <div className="slider-track">
        <div className="slider-fill" style={{ width: `${clamp(fill, 0, 100)}%` }} />
        <input
          type="range"
          min={min}
          max={max}
          step={step}
          value={value}
          aria-label={ariaLabel ?? label}
          onPointerDown={onBegin}
          onChange={(event) => onChange(Number(event.currentTarget.value))}
        />
      </div>
    </div>
  );
}

/* ------------------------------------------------------------------ *
 * Toggle / switch
 * ------------------------------------------------------------------ */

export function Toggle({
  checked,
  onChange,
  label,
  disabled = false,
  className = '',
  title,
}: {
  checked: boolean;
  onChange: (value: boolean) => void;
  label?: string;
  disabled?: boolean;
  className?: string;
  title?: string;
}) {
  return (
    <button
      type="button"
      role="switch"
      aria-checked={checked}
      aria-label={label}
      title={title ?? label}
      disabled={disabled}
      className={`toggle ${checked ? 'is-on' : ''} ${className}`}
      onClick={() => onChange(!checked)}
    >
      <span className="toggle-track">
        <span className="toggle-thumb" />
      </span>
      {label && <span className="toggle-label">{label}</span>}
    </button>
  );
}

/* ------------------------------------------------------------------ *
 * Segmented buttons
 * ------------------------------------------------------------------ */

export interface SegmentOption<T extends string | number> {
  value: T;
  label: string;
  title?: string;
}

export function Segmented<T extends string | number>({
  options,
  value,
  onChange,
  onBegin,
  label,
  className = '',
  compact = false,
}: {
  options: SegmentOption<T>[];
  value: T;
  onChange: (value: T) => void;
  onBegin?: () => void;
  label?: string;
  className?: string;
  compact?: boolean;
}) {
  return (
    <div className={`segmented ${compact ? 'segmented--compact' : ''} ${className}`} role="group" aria-label={label}>
      {options.map((option) => (
        <button
          key={String(option.value)}
          type="button"
          title={option.title}
          aria-pressed={option.value === value}
          className={option.value === value ? 'is-active' : ''}
          onClick={() => {
            onBegin?.();
            onChange(option.value);
          }}
        >
          {option.label}
        </button>
      ))}
    </div>
  );
}

/* ------------------------------------------------------------------ *
 * Icon button
 * ------------------------------------------------------------------ */

export function IconButton({
  icon,
  label,
  onClick,
  active = false,
  disabled = false,
  size = 16,
  className = '',
  badge,
}: {
  icon: Parameters<typeof Icon>[0]['name'];
  label: string;
  onClick?: () => void;
  active?: boolean;
  disabled?: boolean;
  size?: number;
  className?: string;
  badge?: boolean;
}) {
  return (
    <button
      type="button"
      className={`icon-button ${active ? 'is-active' : ''} ${className}`}
      aria-label={label}
      title={label}
      onClick={onClick}
      disabled={disabled}
    >
      <Icon name={icon} size={size} />
      {badge && <span className="icon-button-dot" />}
    </button>
  );
}
