import { Modal } from './Modal';
import { Icon } from './Icons';

export function AboutModal({ open, onClose }: { open: boolean; onClose: () => void }) {
  return (
    <Modal
      open={open}
      onClose={onClose}
      eyebrow="ABOUT"
      title="VRAC+ — drumify any sound"
      wide
      footer={
        <>
          <span className="modal-foot-note">An independent browser instrument inspired by VRAC+ by Beatsurfing.</span>
          <button type="button" className="button button--ghost" onClick={onClose}>
            CLOSE
          </button>
        </>
      }
    >
      <div className="about-hero">
        <span className="about-kicker">SOURCE-BASED · COHESIVE · ORIGINAL</span>
        <h3>
          One sound.
          <br />
          One drum kit.
        </h3>
        <p>
          Feed it any sound and get back a full drum kit. Auto-slice a sample, find every hit,
          sculpt each pad. VRAC+ turns raw audio into punchy, original beats built for the stage,
          the studio or both.
        </p>
        <div className="about-stats">
          <div><strong>16</strong><span>PADS</span></div>
          <div><strong>8</strong><span>STEREO OUTS</span></div>
          <div><strong>4</strong><span>MODEL FAMILIES</span></div>
          <div><strong>24</strong><span>BIT EXPORT</span></div>
        </div>
      </div>

      <div className="about-columns">
        <section>
          <h4>Why VRAC+</h4>
          <ul className="about-list">
            <li><strong>Kits in minutes.</strong> Skip the sample-browsing rabbit hole and get to a playable kit fast.</li>
            <li><strong>Any audio becomes drums.</strong> Raw, unprocessed sound turned into expressive, stage-ready material.</li>
            <li><strong>A full bank of morphing models.</strong> Blend across kicks, snares, hats and claps to land on the exact character you're after.</li>
            <li><strong>Real sound design, not just presets.</strong> Warp, Impact, Instability, Tone Shift, Super and a 3-band EQ on every pad.</li>
          </ul>
        </section>
        <section>
          <h4>Inside this build</h4>
          <div className="about-feature-grid">
            <div><Icon name="wave" size={15} /><span>Auto-slicer with sensitivity control and manual slice starts</span></div>
            <div><Icon name="folder" size={15} /><span>Six factory sound worlds plus drag-and-drop and input recording</span></div>
            <div><Icon name="sliders" size={15} /><span>16 morphing models across four model families</span></div>
            <div><Icon name="headphones" size={15} /><span>Per-pad shaping, velocity, choke groups and eight stereo outs</span></div>
            <div><Icon name="midi" size={15} /><span>MIDI note triggering and MIDI-learn on every knob</span></div>
            <div><Icon name="download" size={15} /><span>One-shot and full-kit WAV/ZIP export at 24-bit / 48 kHz</span></div>
          </div>
        </section>
      </div>

      <div className="about-workflow">
        {[
          ['01 · LOAD', 'Record from your input, drop any audio file, or start from the library. The slicer finds the hits automatically, with sixteen pad start positions.'],
          ['02 · SHAPE', 'Morph each pad between the original and a kick, snare, hat or clap model. Open the hood with Warp, Impact, Instability, Tone Shift, Super and EQ.'],
          ['03 · MIX', 'Mute, solo, choke groups and eight stereo outputs. Print individual one-shots or the whole kit and drop it straight into your DAW.'],
        ].map(([title, body]) => (
          <article key={title}>
            <span>{title}</span>
            <p>{body}</p>
          </article>
        ))}
      </div>

      <div className="about-library">
        <div>
          <span className="menu-eyebrow">SOUND LIBRARY</span>
          <h4>Six worlds, made for slicing.</h4>
          <p>
            Cinematic foley, kitchen clatter, body percussion, scrap metal, circuit noise and vinyl
            dust. Every factory source is generated locally from a seeded procedural recipe, so the
            library ships with the app and never needs a download. The real VRAC+ library was built
            with foley artist Olivier Thys; this browser edition pays tribute with its own synthetic
            found-sound set.
          </p>
        </div>
        <div className="about-credit">
          <span className="menu-eyebrow">CREDITS</span>
          <p>
            An independent, non-commercial browser interpretation inspired by{' '}
            <a href="https://beatsurfing.com/audio-plugins/vrac-plus/" target="_blank" rel="noreferrer">
              VRAC+ by Beatsurfing
            </a>
            . Not affiliated with or endorsed by Beatsurfing / Druw Audio. Original concept, product
            name and marketing copy belong to their authors. Built with React, TypeScript and the Web
            Audio API; all instrument audio is synthesised in this tab.
          </p>
          <p className="about-system">
            Requires a browser with Web Audio and AudioWorklet — Chrome or Edge recommended. No
            account, no upload, no samples sent anywhere.
          </p>
        </div>
      </div>
    </Modal>
  );
}
