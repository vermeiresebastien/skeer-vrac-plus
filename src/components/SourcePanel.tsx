import type { GlobalParams, PadParams, SourceSample } from '../types';
import { formatCents, formatDb, formatSemis, formatTime, formatPct } from '../types';
import type { Slice } from '../audio/slicer';
import { Waveform } from './Waveform';
import { Knob, Slider } from './Controls';
import { Icon } from './Icons';

export interface SourcePanelProps {
  sample: SourceSample;
  slices: Slice[];
  selectedPad: PadParams;
  global: GlobalParams;
  sensitivity: number;
  onSampleStart: (time: number) => void;
  onGestureStart: () => void;
  onSensitivity: (value: number) => void;
  onAddSlice: () => void;
  onDeleteSlice: () => void;
  onGlobal: (patch: Partial<GlobalParams>) => void;
  onOpenLibrary: () => void;
  onBrowse: () => void;
  onRecord: () => void;
  dragActive: boolean;
  onPrevSlice: () => void;
  onNextSlice: () => void;
  onRandomSlice: () => void;
  onMidiLearn: (target: string) => void;
}

function filterLabel(value: number): string {
  if (Math.abs(value) < 0.015) return 'OPEN';
  if (value < 0) {
    const cutoff = 20000 * Math.pow(120 / 20000, -value);
    return `LP ${cutoff >= 1000 ? `${(cutoff / 1000).toFixed(1)}k` : Math.round(cutoff)}`;
  }
  const cutoff = 20 * Math.pow(8200 / 20, value);
  return `HP ${cutoff >= 1000 ? `${(cutoff / 1000).toFixed(1)}k` : Math.round(cutoff)}`;
}

export function SourcePanel({
  sample,
  slices,
  selectedPad,
  global,
  sensitivity,
  onSampleStart,
  onGestureStart,
  onSensitivity,
  onAddSlice,
  onDeleteSlice,
  onGlobal,
  onOpenLibrary,
  onBrowse,
  onRecord,
  dragActive,
  onPrevSlice,
  onNextSlice,
  onRandomSlice,
  onMidiLearn,
}: SourcePanelProps) {
  return (
    <section className={`panel source-panel ${dragActive ? 'is-dragging-file' : ''}`}>
      <header className="section-head">
        <span className="section-kicker">
          <i className="kicker-dot" /> 01 — SOURCE
        </span>
        <span className="section-meta">{sample.name}</span>
      </header>

      <div className="source-wave-card">
        <div className="wave-card-top">
          <span className="mono wave-time">00:00.000</span>
          <span className="wave-source-tag">
            {sample.isUser ? 'USER SAMPLE' : 'SliceForge LIBRARY'} · {(sample.sampleRate / 1000).toFixed(1)} kHz
          </span>
          <span className="mono wave-time">{formatTime(sample.duration, true)}</span>
        </div>
        <Waveform
          sample={sample}
          slices={slices}
          start={selectedPad.sampleStart}
          onStartChange={onSampleStart}
          onGestureStart={onGestureStart}
          ariaLabel={`Source waveform. Sample start for ${selectedPad.note}`}
        />
        <div className="wave-card-bottom">
          <span className="wave-hint">
            <Icon name="wave" size={12} /> drag the marker to move {selectedPad.note}&apos;s start
          </span>
          <span className="wave-slice-count">{slices.length} SLICES</span>
        </div>
        {dragActive && (
          <div className="drop-overlay">
            <Icon name="upload" size={22} />
            <strong>DROP AUDIO</strong>
            <span>any sound becomes a kit</span>
          </div>
        )}
      </div>

      <div className="source-actions">
        <button type="button" className="chip-button" onClick={onOpenLibrary} title="Open the sample library">
          <Icon name="folder" size={14} />
          LIBRARY
        </button>
        <button type="button" className="chip-button" onClick={onBrowse} title="Load an audio file">
          <Icon name="upload" size={14} />
          LOAD
        </button>
        <button type="button" className="chip-button" onClick={onRecord} title="Record from an input">
          <Icon name="record" size={12} />
          REC
        </button>
        <span className="source-actions-hint">or drop a file anywhere</span>
      </div>

      <div className="slicer-block">
        <div className="slicer-head">
          <span className="mini-label">AUTO SLICE</span>
          <span className="mono mini-value">{slices.length} HITS</span>
        </div>
        <Slider
          value={sensitivity}
          min={0}
          max={1}
          step={0.001}
          onChange={onSensitivity}
          onBegin={onGestureStart}
          format={(value) => `${Math.round(value * 100)}%`}
          ariaLabel="Auto slice sensitivity"
          compact
        />
        <div className="slice-tools">
          <div className="slice-start-readout">
            <span className="mini-label">{selectedPad.note} START</span>
            <span className="mono">{formatTime(selectedPad.sampleStart, true)}</span>
          </div>
          <div className="slice-nav">
            <button type="button" className="icon-button mini" onClick={onPrevSlice} title="Previous slice">
              <Icon name="left" size={13} />
            </button>
            <button type="button" className="icon-button mini" onClick={onNextSlice} title="Next slice">
              <Icon name="right" size={13} />
            </button>
            <button type="button" className="icon-button mini" onClick={onRandomSlice} title="Random slice">
              <Icon name="shuffle" size={13} />
            </button>
            <button type="button" className="icon-button mini" onClick={onAddSlice} title="Add a slice at this start">
              <Icon name="plus" size={12} />
            </button>
            <button type="button" className="icon-button mini" onClick={onDeleteSlice} title="Delete the nearest slice">
              <Icon name="trash" size={12} />
            </button>
          </div>
        </div>
      </div>

      <div className="panel-rule" />

      <header className="section-head">
        <span className="section-kicker">
          <i className="kicker-dot" /> 02 — GLOBAL
        </span>
        <span className="section-meta">KIT</span>
      </header>

      <div className="global-knobs">
        <Knob
          label="FINE TUNE"
          value={global.fineTune}
          min={-50}
          max={50}
          step={1}
          defaultValue={0}
          size="sm"
          format={formatCents}
          onChange={(value) => onGlobal({ fineTune: value })}
          onBegin={onGestureStart}
          bipolar
          onLearn={() => onMidiLearn('global.fineTune')}
          title="Fine tune the whole kit, in cents. Right-click to MIDI learn."
        />
        <Knob
          label="DRUMIFY"
          value={global.drumify}
          min={0}
          max={200}
          step={1}
          defaultValue={100}
          size="sm"
          format={(value) => `${Math.round(value)}%`}
          onChange={(value) => onGlobal({ drumify: value })}
          onBegin={onGestureStart}
          onLearn={() => onMidiLearn('global.drumify')}
          title="Global drumify amount. 100% is neutral; lower it to hear more original sample."
        />
        <Knob
          label="VOLUME"
          value={global.volume}
          min={-24}
          max={6}
          step={0.1}
          defaultValue={-2.4}
          size="sm"
          format={(value) => formatDb(value)}
          onChange={(value) => onGlobal({ volume: value })}
          onBegin={onGestureStart}
          onLearn={() => onMidiLearn('global.volume')}
          title="Master volume. Right-click to MIDI learn."
        />
        <Knob
          label="TRANSPOSE"
          value={global.transpose}
          min={-24}
          max={24}
          step={1}
          defaultValue={0}
          size="sm"
          format={formatSemis}
          onChange={(value) => onGlobal({ transpose: value })}
          onBegin={onGestureStart}
          bipolar
          onLearn={() => onMidiLearn('global.transpose')}
          title="Transpose the whole kit in semitones."
        />
        <Knob
          label="LENGTH"
          value={global.length}
          min={0.25}
          max={2}
          step={0.01}
          defaultValue={1}
          size="sm"
          format={(value) => formatPct(value * 100)}
          onChange={(value) => onGlobal({ length: value })}
          onBegin={onGestureStart}
          onLearn={() => onMidiLearn('global.length')}
          title="Scale all pad lengths together."
        />
        <Knob
          label="TONE"
          value={global.tone}
          min={-12}
          max={12}
          step={0.1}
          defaultValue={0}
          size="sm"
          format={(value) => (Math.abs(value) < 0.05 ? '0.0' : `${value > 0 ? '+' : ''}${value.toFixed(1)}`)}
          onChange={(value) => onGlobal({ tone: value })}
          onBegin={onGestureStart}
          bipolar
          onLearn={() => onMidiLearn('global.tone')}
          title="Global spectral tilt. Positive is brighter."
        />
        <Knob
          label="FILTER"
          value={global.filter}
          min={-1}
          max={1}
          step={0.01}
          defaultValue={0}
          size="sm"
          format={filterLabel}
          onChange={(value) => onGlobal({ filter: value })}
          onBegin={onGestureStart}
          bipolar
          onLearn={() => onMidiLearn('global.filter')}
          title="DJ-style filter: turn left for low-pass, right for high-pass."
        />
        <Knob
          label="WIDTH"
          value={global.width}
          min={0}
          max={1}
          step={0.01}
          defaultValue={0.65}
          size="sm"
          format={(value) => formatPct(value * 100)}
          onChange={(value) => onGlobal({ width: value })}
          onBegin={onGestureStart}
          onLearn={() => onMidiLearn('global.width')}
          title="Global stereo width."
        />
        <Knob
          label="SOFT CLIP"
          value={global.clip}
          min={0}
          max={1}
          step={0.01}
          defaultValue={0}
          size="sm"
          format={(value) => formatPct(value * 100)}
          onChange={(value) => onGlobal({ clip: value })}
          onBegin={onGestureStart}
          onLearn={() => onMidiLearn('global.clip')}
          title="Output soft clipper: how early the peaks bend. 0 % leaves the signal untouched; anything under the knee is never attenuated."
        />
        <Knob
          label="OUT"
          value={global.outGain}
          min={-24}
          max={12}
          step={0.1}
          defaultValue={0}
          size="sm"
          format={(value) => formatDb(value)}
          onChange={(value) => onGlobal({ outGain: value })}
          onBegin={onGestureStart}
          bipolar
          onLearn={() => onMidiLearn('global.outGain')}
          title="Output trim, applied after the clipper. A final ceiling keeps the app inside full scale however high this goes."
        />
      </div>

      <div className="panel-rule" />

      <div className="source-info">
        <div>
          <span className="mini-label">DURATION</span>
          <span className="mono">{sample.duration.toFixed(1)}s</span>
        </div>
        <div>
          <span className="mini-label">RATE</span>
          <span className="mono">{(sample.sampleRate / 1000).toFixed(1)} kHz</span>
        </div>
        <div>
          <span className="mini-label">SLICES</span>
          <span className="mono">{slices.length}</span>
        </div>
        <div>
          <span className="mini-label">PADS</span>
          <span className="mono">16</span>
        </div>
      </div>

      <header className="section-head quickstart-head">
        <span className="section-kicker">
          <i className="kicker-dot" /> QUICK START
        </span>
        <span className="section-meta">3 STEPS</span>
      </header>

      <div className="quickstart">
        <div>
          <span>01</span>
          <strong>DROP</strong>
          <p>Any sound</p>
        </div>
        <div>
          <span>02</span>
          <strong>SLICE</strong>
          <p>Find the hits</p>
        </div>
        <div>
          <span>03</span>
          <strong>MORPH</strong>
          <p>Shape the kit</p>
        </div>
      </div>

      <div className="source-note">
        <span className="status-led" />
        <span>
          {sample.isUser ? 'Your audio is sliced locally — nothing is uploaded.' : 'Factory source · swap it any time.'}
        </span>
      </div>
    </section>
  );
}
