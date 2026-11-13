import { Modal } from './Modal';
import { Icon } from './Icons';
import type { MidiInputInfo, MidiStatus } from '../audio/midi';
import type { MidiMapping } from '../state/storage';
import type { PadParams } from '../types';
import { formatMidiTarget } from '../lib/midiTargets';

export function MidiModal({
  open,
  onClose,
  status,
  inputs,
  mappings,
  learnTarget,
  pads,
  onEnable,
  onClearMappings,
  onRemoveMapping,
}: {
  open: boolean;
  onClose: () => void;
  status: MidiStatus;
  inputs: MidiInputInfo[];
  mappings: MidiMapping[];
  learnTarget: string | null;
  pads: PadParams[];
  onEnable: () => void;
  onClearMappings: () => void;
  onRemoveMapping: (cc: number) => void;
}) {
  return (
    <Modal
      open={open}
      onClose={onClose}
      eyebrow="CONNECTIONS"
      title="MIDI"
      footer={
        <>
          <button type="button" className="button button--ghost" onClick={onClose}>
            CLOSE
          </button>
          <button type="button" className="button button--primary" onClick={onEnable} disabled={status === 'requesting'}>
            {status === 'ready' ? 'RESCAN DEVICES' : 'ENABLE MIDI'}
          </button>
        </>
      }
    >
      <div className="midi-status-row">
        <span className={`midi-pill midi-pill--${status}`}>
          <span className="audio-dot" />
          {status === 'ready' ? 'MIDI READY' : status === 'requesting' ? 'REQUESTING…' : status === 'unsupported' ? 'NOT SUPPORTED' : status === 'error' ? 'DENIED' : 'MIDI OFF'}
        </span>
        <span className="midi-note">Pads answer notes C1–D#2 · MIDI 36–51 with velocity.</span>
      </div>

      <div className="midi-section">
        <span className="menu-eyebrow">INPUTS</span>
        {inputs.length === 0 ? (
          <div className="empty-state">
            <Icon name="midi" size={20} />
            <div>
              <strong>No MIDI inputs yet.</strong>
              <p>Connect a controller, click ENABLE MIDI and play. Chrome and Edge expose Web MIDI over localhost and HTTPS.</p>
            </div>
          </div>
        ) : (
          <div className="midi-input-list">
            {inputs.map((input) => (
              <div className="midi-input" key={input.id}>
                <span className="status-led" />
                <div>
                  <strong>{input.name}</strong>
                  <span>{input.manufacturer || 'MIDI input'} · {input.state}</span>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>

      <div className="midi-section">
        <div className="midi-section-head">
          <span className="menu-eyebrow">CC MAPPINGS</span>
          {mappings.length > 0 && (
            <button type="button" className="text-button" onClick={onClearMappings}>
              Clear all
            </button>
          )}
        </div>
        {learnTarget && (
          <div className="learn-banner">
            <span className="pulse-dot" />
            <span>
              Waiting for a CC… right-clicked target: <strong>{formatMidiTarget(learnTarget, pads)}</strong>
            </span>
          </div>
        )}
        {mappings.length === 0 ? (
          <p className="form-note">
            No mappings yet. Right-click any knob and move a controller to assign it. Mappings are
            saved in this browser.
          </p>
        ) : (
          <div className="mapping-list">
            {mappings.map((mapping) => (
              <div className="mapping-row" key={`${mapping.cc}-${mapping.target}`}>
                <span className="mapping-cc">CC {mapping.cc}</span>
                <span className="mapping-target">{formatMidiTarget(mapping.target, pads)}</span>
                <button
                  type="button"
                  className="icon-button mini"
                  aria-label={`Remove CC ${mapping.cc}`}
                  onClick={() => onRemoveMapping(mapping.cc)}
                >
                  <Icon name="close" size={12} />
                </button>
              </div>
            ))}
          </div>
        )}
      </div>

      <div className="midi-help">
        <strong>How MIDI learn works</strong>
        <p>
          Right-click a knob (or any learnable control) → move a CC on your controller. The mapping
          applies from 0–127 across the knob's full range. Right-click again to re-learn. MIDI note
          input triggers pads whether or not the browser window is focused (subject to your
          browser's background throttling).
        </p>
      </div>
    </Modal>
  );
}
