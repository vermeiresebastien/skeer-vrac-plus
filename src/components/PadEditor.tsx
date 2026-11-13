import { useMemo } from 'react';
import type { Family, PadParams, SourceSample } from '../types';
import {
  FAMILIES,
  FAMILY_META,
  MODEL_BANK,
  formatDb,
  formatMs,
  formatPct,
  formatSemis,
  formatTime,
} from '../types';
import type { Slice } from '../audio/slicer';
import { Waveform } from './Waveform';
import { IconButton, Knob, Segmented, Slider } from './Controls';
import { Icon, type IconName } from './Icons';

export interface PadEditorProps {
  pad: PadParams;
  sample: SourceSample;
  slices: Slice[];
  canPaste: boolean;
  onPadChange: (patch: Partial<PadParams>) => void;
  onPadStart: (time: number) => void;
  onGestureStart: () => void;
  onModelSelect: (family: Family, model: number) => void;
  onChoke: (group: number) => void;
  onBus: (bus: number) => void;
  onTrigger: (velocity?: number) => void;
  onAuditionOverride: (pad: PadParams) => void;
  onMidiLearn: (target: string) => void;
  onPrevSlice: () => void;
  onNextSlice: () => void;
  onRandomSlice: () => void;
  onCopy: () => void;
  onPaste: () => void;
  onReset: () => void;
}

function panLabel(value: number): string {
  if (Math.abs(value) < 0.02) return 'C';
  return `${value < 0 ? 'L' : 'R'}${Math.round(Math.abs(value) * 100)}`;
}

function chokeOptions() {
  return [
    { value: 0, label: 'OFF', title: 'No choke group' },
    { value: 1, label: '1', title: 'Choke group 1' },
    { value: 2, label: '2', title: 'Choke group 2' },
    { value: 3, label: '3', title: 'Choke group 3' },
    { value: 4, label: '4', title: 'Choke group 4' },
  ];
}

const BUS_OPTIONS = Array.from({ length: 8 }, (_, i) => ({
  value: i + 1,
  label: String(i + 1),
  title: `Stereo output ${i + 1}`,
}));

export function PadEditor({
  pad,
  sample,
  slices,
  canPaste,
  onPadChange,
  onPadStart,
  onGestureStart,
  onModelSelect,
  onChoke,
  onBus,
  onTrigger,
  onAuditionOverride,
  onMidiLearn,
  onPrevSlice,
  onNextSlice,
  onRandomSlice,
  onCopy,
  onPaste,
  onReset,
}: PadEditorProps) {
  const meta = FAMILY_META[pad.family];
  const model = MODEL_BANK[pad.family][pad.model];
  const focusSpan = Math.max(0.7, pad.length * 3.2);
  const viewStart = Math.max(0, pad.sampleStart - Math.min(0.3, focusSpan * 0.16));
  const viewEnd = Math.min(sample.duration, pad.sampleStart + focusSpan);
  const knobTarget = (param: string) => `pad.${pad.id}.${param}`;

  const auditionOriginal = () => {
    onAuditionOverride({ ...pad, drumify: 0 });
  };
  const auditionDrumified = () => {
    onAuditionOverride({ ...pad, drumify: 1 });
  };

  const controls: Array<{
    label: string;
    value: number;
    min: number;
    max: number;
    step: number;
    defaultValue: number;
    format: (value: number) => string;
    patch: Partial<PadParams>;
    param: string;
    bipolar?: boolean;
    title: string;
    icon: IconName;
  }> = useMemo(
    () => [
      {
        label: 'PITCH',
        value: pad.pitch,
        min: -24,
        max: 24,
        step: 1,
        defaultValue: 0,
        format: formatSemis,
        patch: { pitch: pad.pitch },
        param: 'pitch',
        bipolar: true,
        title: 'Pitch the sample and model. Right-click to MIDI learn.',
        icon: 'sliders',
      },
      {
        label: 'LENGTH',
        value: pad.length,
        min: 0.02,
        max: 2,
        step: 0.001,
        defaultValue: 0.25,
        format: formatMs,
        patch: { length: pad.length },
        param: 'length',
        title: 'Hit length. Short for ticks, long for tails.',
        icon: 'wave',
      },
      {
        label: 'VELOCITY',
        value: pad.velocityAmount,
        min: 0,
        max: 1,
        step: 0.01,
        defaultValue: 0.65,
        format: (value) => formatPct(value * 100),
        patch: { velocityAmount: pad.velocityAmount },
        param: 'velocityAmount',
        title: 'How much velocity shapes volume, length and tone.',
        icon: 'sliders',
      },
      {
        label: 'PAN',
        value: pad.pan,
        min: -1,
        max: 1,
        step: 0.01,
        defaultValue: 0,
        format: panLabel,
        patch: { pan: pad.pan },
        param: 'pan',
        bipolar: true,
        title: 'Place the pad in the stereo field.',
        icon: 'headphones',
      },
      {
        label: 'VOLUME',
        value: pad.volume,
        min: -60,
        max: 6,
        step: 0.1,
        defaultValue: 0,
        format: (value) => formatDb(value),
        patch: { volume: pad.volume },
        param: 'volume',
        title: 'Pad level. Right-click to MIDI learn.',
        icon: 'headphones',
      },
      {
        label: 'TONE',
        value: pad.toneShift,
        min: -12,
        max: 12,
        step: 0.1,
        defaultValue: 0,
        format: formatSemis,
        patch: { toneShift: pad.toneShift },
        param: 'toneShift',
        bipolar: true,
        title: 'Spectral shift for this pad.',
        icon: 'sliders',
      },
    ],
    [pad],
  );

  const sculpt: Array<{
    label: string;
    value: number;
    patch: Partial<PadParams>;
    param: string;
    title: string;
    bipolar?: boolean;
    format: (value: number) => string;
    min: number;
    max: number;
    step: number;
    defaultValue: number;
  }> = [
    {
      label: 'WARP',
      value: pad.warp,
      patch: { warp: pad.warp },
      param: 'warp',
      title: 'Three-band dynamic reshaping: lows forward, then air as the hit decays.',
      format: (value) => formatPct(value * 100),
      min: 0,
      max: 1,
      step: 0.01,
      defaultValue: 0.2,
    },
    {
      label: 'IMPACT',
      value: pad.impact,
      patch: { impact: pad.impact },
      param: 'impact',
      title: 'Transient shaping. More snap, less body.',
      format: (value) => formatPct(value * 100),
      min: 0,
      max: 1,
      step: 0.01,
      defaultValue: 0.35,
    },
    {
      label: 'INSTAB',
      value: pad.instability,
      patch: { instability: pad.instability },
      param: 'instability',
      title: 'Random variation at every hit: pitch, decay, level and position.',
      format: (value) => formatPct(value * 100),
      min: 0,
      max: 1,
      step: 0.01,
      defaultValue: 0.08,
    },
    {
      label: 'SUPER',
      value: pad.super,
      patch: { super: pad.super },
      param: 'super',
      title: 'Saturation and stereo width.',
      format: (value) => formatPct(value * 100),
      min: 0,
      max: 1,
      step: 0.01,
      defaultValue: 0.18,
    },
  ];

  return (
    <section className="panel pad-editor" style={{ ['--pad-accent' as string]: meta.color }}>
      <header className="section-head pad-editor-head">
        <div className="pad-editor-title">
          <span className="section-kicker">
            <i className="kicker-dot" /> 03 — SELECTED PAD
          </span>
          <div className="pad-title-row">
            <h2 className="pad-title">{pad.note}</h2>
            <span className="family-badge">{meta.label}</span>
            <span className="model-badge">{model.name}</span>
          </div>
        </div>
        <div className="pad-editor-actions">
          <IconButton icon="play" label={`Audition ${pad.note}`} onClick={() => onTrigger(0.9)} />
          <IconButton icon="copy" label="Copy pad settings" onClick={onCopy} />
          <IconButton icon="paste" label="Paste pad settings" onClick={onPaste} disabled={!canPaste} />
          <IconButton icon="reset" label="Reset pad" onClick={onReset} />
        </div>
      </header>

      <div className="pad-wave-card">
        <div className="pad-wave-meta">
          <button type="button" className="icon-button mini" onClick={onPrevSlice} title="Previous slice">
            <Icon name="left" size={13} />
          </button>
          <span className="mono">{formatTime(pad.sampleStart, true)}</span>
          <span className="pad-wave-hint">START · SLICE {pad.sliceIndex >= 0 ? pad.sliceIndex + 1 : 'MANUAL'}</span>
          <button type="button" className="icon-button mini" onClick={onRandomSlice} title="Random slice">
            <Icon name="shuffle" size={13} />
          </button>
          <button type="button" className="icon-button mini" onClick={onNextSlice} title="Next slice">
            <Icon name="right" size={13} />
          </button>
        </div>
        <Waveform
          sample={sample}
          slices={slices}
          start={pad.sampleStart}
          viewStart={viewStart}
          viewEnd={viewEnd}
          onStartChange={onPadStart}
          onGestureStart={onGestureStart}
          compact
          showHandle
          ariaLabel={`Start position for ${pad.note}`}
        />
      </div>

      <div className="morph-block">
        <div className="morph-side">
          <Knob
            label="ORIGINAL"
            value={pad.originalGain}
            min={-24}
            max={12}
            step={0.1}
            defaultValue={-4.4}
            size="sm"
            format={(value) => formatDb(value)}
            onChange={(value) => onPadChange({ originalGain: value })}
            onBegin={onGestureStart}
            onLearn={() => onMidiLearn(knobTarget('originalGain'))}
            title="Level of the original sample layer."
          />
          <button type="button" className="ghost-audition" onClick={auditionOriginal}>
            <Icon name="play" size={10} /> HEAR SAMPLE
          </button>
        </div>
        <div className="morph-main">
          <Knob
            label="DRUMIFY"
            value={pad.drumify}
            min={0}
            max={1}
            step={0.005}
            defaultValue={0.65}
            size="lg"
            format={(value) => formatPct(value * 100)}
            onChange={(value) => onPadChange({ drumify: value })}
            onBegin={onGestureStart}
            onLearn={() => onMidiLearn(knobTarget('drumify'))}
            title="Morph between the original sample and the selected drum model. Right-click to MIDI learn."
          />
          <span className="morph-caption">
            {pad.drumify < 0.15 ? 'SAMPLE' : pad.drumify > 0.85 ? 'MODEL' : 'MORPH'}
          </span>
        </div>
        <div className="morph-side morph-side--right">
          <Knob
            label="DRUMIFIED"
            value={pad.drumifiedGain}
            min={-24}
            max={12}
            step={0.1}
            defaultValue={-4.5}
            size="sm"
            format={(value) => formatDb(value)}
            onChange={(value) => onPadChange({ drumifiedGain: value })}
            onBegin={onGestureStart}
            onLearn={() => onMidiLearn(knobTarget('drumifiedGain'))}
            title="Level of the drumified model layer."
          />
          <button type="button" className="ghost-audition" onClick={auditionDrumified}>
            <Icon name="play" size={10} /> HEAR MODEL
          </button>
        </div>
      </div>

      <div className="pad-control-grid">
        {controls.map((control) => (
          <Knob
            key={control.label}
            label={control.label}
            value={control.value}
            min={control.min}
            max={control.max}
            step={control.step}
            defaultValue={control.defaultValue}
            size="sm"
            format={control.format}
            onChange={(value) => onPadChange({ [control.param]: value } as Partial<PadParams>)}
            onBegin={onGestureStart}
            onLearn={() => onMidiLearn(knobTarget(control.param))}
            bipolar={control.bipolar}
            title={control.title}
          />
        ))}
      </div>

      <div className="pad-utility-row">
        <div className="choke-block">
          <span className="mini-label">CHOKE</span>
          <Segmented
            options={chokeOptions()}
            value={pad.choke}
            onBegin={onGestureStart}
            onChange={onChoke}
            compact
            label="Choke group"
          />
        </div>
        <div className="bus-block">
          <span className="mini-label">OUTPUT</span>
          <Segmented
            options={BUS_OPTIONS}
            value={pad.bus}
            onBegin={onGestureStart}
            onChange={onBus}
            compact
            label="Stereo output"
          />
        </div>
      </div>

      <div className="panel-rule" />

      <header className="section-head">
        <span className="section-kicker">
          <i className="kicker-dot" /> 04 — MODEL BANK
        </span>
        <span className="section-meta">
          {meta.plural} · {model.name}
        </span>
      </header>

      <div className="model-matrix" role="grid" aria-label="Morphing model bank">
        {FAMILIES.map((family) => {
          const familyMeta = FAMILY_META[family];
          const models = MODEL_BANK[family];
          return (
            <div className={`model-row ${family === pad.family ? 'is-current-family' : ''}`} key={family} role="row">
              <span className="model-family-label" style={{ color: familyMeta.color }}>
                {familyMeta.plural}
              </span>
              <div className="model-cells" role="presentation">
                {models.map((entry, index) => {
                  const selected = family === pad.family && index === pad.model;
                  return (
                    <button
                      type="button"
                      key={entry.name}
                      role="gridcell"
                      title={`${entry.name} — ${entry.blurb}`}
                      aria-pressed={selected}
                      className={`model-cell ${selected ? 'is-selected' : ''}`}
                      style={{ ['--family-color' as string]: familyMeta.color, ['--family-color-dim' as string]: familyMeta.colorDim }}
                      onClick={() => onModelSelect(family, index)}
                    >
                      <span className="model-name">{entry.name}</span>
                      <span className="model-code">{entry.code}</span>
                      <i className="model-corner" />
                    </button>
                  );
                })}
              </div>
            </div>
          );
        })}
      </div>

      <div className="panel-rule" />

      <header className="section-head">
        <span className="section-kicker">
          <i className="kicker-dot" /> 05 — SCULPT
        </span>
        <span className="section-meta">PER PAD</span>
      </header>

      <div className="sculpt-grid">
        {sculpt.map((control) => (
          <Knob
            key={control.label}
            label={control.label}
            value={control.value}
            min={control.min}
            max={control.max}
            step={control.step}
            defaultValue={control.defaultValue}
            size="sm"
            format={control.format}
            onChange={(value) => onPadChange({ [control.param]: value } as Partial<PadParams>)}
            onBegin={onGestureStart}
            onLearn={() => onMidiLearn(knobTarget(control.param))}
            bipolar={control.bipolar}
            title={control.title}
          />
        ))}
      </div>

      <div className="eq-block">
        <div className="eq-head">
          <span className="mini-label">3-BAND EQ</span>
          <span className="mono eq-hint">LOW / MID / HIGH</span>
        </div>
        <div className="eq-sliders">
          <Slider
            value={pad.eqLow}
            min={-12}
            max={12}
            step={0.1}
            onChange={(value) => onPadChange({ eqLow: value })}
            onBegin={onGestureStart}
            format={(value) => `${value > 0 ? '+' : ''}${value.toFixed(1)}`}
            ariaLabel="Low EQ"
            accent={meta.color}
            compact
          />
          <Slider
            value={pad.eqMid}
            min={-12}
            max={12}
            step={0.1}
            onChange={(value) => onPadChange({ eqMid: value })}
            onBegin={onGestureStart}
            format={(value) => `${value > 0 ? '+' : ''}${value.toFixed(1)}`}
            ariaLabel="Mid EQ"
            accent={meta.color}
            compact
          />
          <Slider
            value={pad.eqHigh}
            min={-12}
            max={12}
            step={0.1}
            onChange={(value) => onPadChange({ eqHigh: value })}
            onBegin={onGestureStart}
            format={(value) => `${value > 0 ? '+' : ''}${value.toFixed(1)}`}
            ariaLabel="High EQ"
            accent={meta.color}
            compact
          />
        </div>
      </div>
    </section>
  );
}
