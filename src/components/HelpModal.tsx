import { useState } from 'react';
import { Modal } from './Modal';
import { Icon } from './Icons';

export function HelpModal({ open, onClose }: { open: boolean; onClose: () => void }) {
  const [tab, setTab] = useState<'load' | 'shape' | 'mix' | 'keys'>('load');

  return (
    <Modal open={open} onClose={onClose} eyebrow="USER GUIDE · V1.0" title="How to use SliceForge" wide>
      <div className="help-tabs" role="tablist">
        {[
          ['load', '01 · LOAD'],
          ['shape', '02 · SHAPE'],
          ['mix', '03 · MIX'],
          ['keys', 'SHORTCUTS'],
        ].map(([id, label]) => (
          <button
            type="button"
            key={id}
            role="tab"
            aria-selected={tab === id}
            className={tab === id ? 'is-active' : ''}
            onClick={() => setTab(id as typeof tab)}
          >
            {label}
          </button>
        ))}
      </div>

      {tab === 'load' && (
        <div className="guide-grid">
          <article className="guide-lead">
            <span className="guide-number">01</span>
            <h3>Any sound becomes drums.</h3>
            <p>
              Drop a recording, load one of the six factory sources, or record straight from an
              interface. SliceForge finds the transients and gives every pad its own start point — one
              file, up to sixteen different sources.
            </p>
          </article>
          <div className="guide-list">
            <div>
              <Icon name="upload" />
              <strong>LOAD A SOUND</strong>
              <p>Drop WAV, AIFF, MP3, FLAC, OGG or M4A anywhere on the source panel, or use LOAD.</p>
            </div>
            <div>
              <Icon name="wave" />
              <strong>AUTO SLICING</strong>
              <p>The AUTO SLICE amount decides how many hits are found. Drag the orange marker to move a pad's start; use the arrows to step through slices.</p>
            </div>
            <div>
              <Icon name="record" />
              <strong>RECORD IN</strong>
              <p>Open REC to capture an input for up to two minutes. Nothing leaves your machine.</p>
            </div>
            <div>
              <Icon name="folder" />
              <strong>FACTORY LIBRARY</strong>
              <p>Foley Trash, Kitchen Perc, Body Music, Scrap Metal, Circuit Noise and Vinyl Dust. Six sound worlds, each a full slicing playground.</p>
            </div>
          </div>
        </div>
      )}

      {tab === 'shape' && (
        <div className="guide-grid">
          <article className="guide-lead">
            <span className="guide-number">02</span>
            <h3>Design the hit, not just the sample.</h3>
            <p>
              Each pad morphs between the original audio and a drum model. The model bank holds
              four families — kick, snare, hat and clap — with four models each. Blend, then open
              the hood with Warp, Impact, Instability, Tone Shift, Super and the three-band EQ.
            </p>
          </article>
          <div className="guide-list">
            <div>
              <Icon name="sliders" />
              <strong>DRUMIFY</strong>
              <p>Left for the original sample, right for the selected model. ORIGINAL and DRUMIFIED set each layer's level independently.</p>
            </div>
            <div>
              <Icon name="wave" />
              <strong>SCULPT</strong>
              <p>Warp reshapes three bands over the decay, Impact sharpens the transient, Instability varies every hit, Tone Shift moves the spectrum, Super adds drive and width.</p>
            </div>
            <div>
              <Icon name="midi" />
              <strong>MODEL BANK</strong>
              <p>Click any of the sixteen cells to swap the model and hear it instantly. The pad's family colour updates everywhere.</p>
            </div>
            <div>
              <Icon name="headphones" />
              <strong>VELOCITY</strong>
              <p>Velocity shapes volume, length and tone. Play softly by clicking low on a pad, hold Shift for an accent, Alt for a ghost.</p>
            </div>
          </div>
        </div>
      )}

      {tab === 'mix' && (
        <div className="guide-grid">
          <article className="guide-lead">
            <span className="guide-number">03</span>
            <h3>Built for performance and mixing.</h3>
            <p>
              Mute and solo pads, group hats with choke groups, route each pad to one of eight
              stereo outputs, then print one-shots or the entire kit. The master filter and tone
              controls the whole instrument, and SOFT CLIP plus OUT shape what leaves it.
            </p>
          </article>
          <div className="guide-list">
            <div>
              <Icon name="stop" />
              <strong>CHOKE GROUPS</strong>
              <p>Assign pads to groups 1–4. A new hit in the group cuts the previous one — perfect for hats.</p>
            </div>
            <div>
              <Icon name="wave" />
              <strong>OUTPUT STAGE</strong>
              <p>SOFT CLIP decides how early the peaks bend — 0 % is bypassed and nothing under the knee is ever attenuated. OUT trims what comes after it, and a final ceiling keeps the app inside full scale however high you push it.</p>
            </div>
            <div>
              <Icon name="download" />
              <strong>EXPORT</strong>
              <p>Export the selected pad or the full kit: 24-bit / 48 kHz WAV one-shots plus a ZIPPED kit with a manifest and README.</p>
            </div>
            <div>
              <Icon name="midi" />
              <strong>MIDI</strong>
              <p>Pads answer notes C1–D#2 (MIDI 36–51) with velocity. Right-click any knob to MIDI learn a CC.</p>
            </div>
            <div>
              <Icon name="save" />
              <strong>PRESETS</strong>
              <p>Six factory kits ship with SliceForge. Save your own kits locally; export and import presets as JSON.</p>
            </div>
          </div>
        </div>
      )}

      {tab === 'keys' && (
        <div className="keys-guide">
          <div className="key-table">
            <span className="key-table-head">PLAY</span>
            {[
              ['1 2 3 4 · Q W E R · A S D F · Z X C V', 'Trigger the sixteen pads'],
              ['Hold SHIFT while clicking', 'Accent — full velocity'],
              ['Hold ALT while clicking', 'Ghost note — soft velocity'],
              ['SPACE', 'Stop all voices'],
              ['[  /  ]', 'Previous / next slice on the selected pad'],
              ['T', 'Random slice on the selected pad'],
            ].map(([keys, action]) => (
              <div className="key-row" key={keys}>
                <kbd>{keys}</kbd>
                <span>{action}</span>
              </div>
            ))}
          </div>
          <div className="key-table">
            <span className="key-table-head">EDIT</span>
            {[
              ['CTRL / CMD + Z', 'Undo'],
              ['CTRL / CMD + SHIFT + Z', 'Redo'],
              ['Double-click a knob', 'Reset to default'],
              ['Right-click a knob', 'MIDI learn'],
              ['Mouse wheel over a knob', 'Fine adjustment'],
              ['ESC', 'Close dialog'],
            ].map(([keys, action]) => (
              <div className="key-row" key={keys}>
                <kbd>{keys}</kbd>
                <span>{action}</span>
              </div>
            ))}
          </div>
          <div className="guide-note">
            <strong>Browser support:</strong> Chrome or Edge recommended. Audio needs a user gesture
            to start; click any pad to wake the engine. All decoding, synthesis and rendering happen
            locally in your browser.
          </div>
        </div>
      )}

      <div className="modal-tip-row">
        <Icon name="help" size={14} />
        <span>Tip: right-click any knob to learn a MIDI CC. Double-click to reset it.</span>
      </div>
    </Modal>
  );
}
