import type { PadParams } from '../types';
import { Toggle } from './Controls';
import { Icon } from './Icons';
import { OutputMeters } from './OutputMeters';
import { PadGrid } from './PadGrid';

export interface PadsPanelProps {
  pads: PadParams[];
  selectedPad: number;
  activePads: Record<number, number>;
  autoSelect: boolean;
  onAutoSelect: (value: boolean) => void;
  onTrigger: (index: number, velocity: number) => void;
  onSelect: (index: number) => void;
  onToggleMute: (index: number) => void;
  onToggleSolo: (index: number) => void;
  onPanic: () => void;
}

export function PadsPanel({
  pads,
  selectedPad,
  activePads,
  autoSelect,
  onAutoSelect,
  onTrigger,
  onSelect,
  onToggleMute,
  onToggleSolo,
  onPanic,
}: PadsPanelProps) {
  return (
    <section className="panel pads-panel">
      <header className="section-head pads-head">
        <span className="section-kicker">
          <i className="kicker-dot" /> PADS
        </span>
        <span className="pads-head-center">
          <Toggle checked={autoSelect} onChange={onAutoSelect} label="AUTO SELECT" title="Follow the pad you play" />
        </span>
        <span className="pads-head-actions">
          <span className="pads-count">16 PADS · 4 FAMILIES</span>
          <button type="button" className="chip-button chip-button--silent" onClick={onPanic} title="Stop all voices">
            <Icon name="stop" size={12} /> STOP
          </button>
        </span>
      </header>

      <div className="pads-stage">
        <PadGrid
          pads={pads}
          selectedPad={selectedPad}
          activePads={activePads}
          onTrigger={onTrigger}
          onSelect={onSelect}
          onToggleMute={onToggleMute}
          onToggleSolo={onToggleSolo}
        />
      </div>

      <OutputMeters />

      <div className="pads-foot">
        <div className="play-hint">
          <span className="keyboard-keys" aria-hidden="true">
            <kbd>1</kbd><kbd>2</kbd><kbd>3</kbd><kbd>4</kbd>
            <span className="keyboard-row-sep" />
            <kbd>Q</kbd><kbd>W</kbd><kbd>E</kbd><kbd>R</kbd>
            <span className="keyboard-row-sep" />
            <kbd>A</kbd><kbd>S</kbd><kbd>D</kbd><kbd>F</kbd>
            <span className="keyboard-row-sep" />
            <kbd>Z</kbd><kbd>X</kbd><kbd>C</kbd><kbd>V</kbd>
          </span>
          <span className="play-hint-text">
            <Icon name="keyboard" size={13} /> play with your keyboard — shift = accent, alt = ghost
          </span>
        </div>
        <div className="pad-legend">
          <span><i className="legend-dot legend-dot--kick" /> KICK</span>
          <span><i className="legend-dot legend-dot--snare" /> SNARE</span>
          <span><i className="legend-dot legend-dot--hat" /> HAT</span>
          <span><i className="legend-dot legend-dot--clap" /> CLAP</span>
        </div>
      </div>
    </section>
  );
}
