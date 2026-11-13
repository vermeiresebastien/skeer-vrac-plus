import type { AudioStatus } from '../audio/audioEngine';
import { Icon } from './Icons';

export function Footer({ audioStatus, sampleRate }: { audioStatus: AudioStatus; sampleRate: number }) {
  return (
    <footer className="app-footer">
      <div className="footer-tip">
        <span className={`footer-led footer-led--${audioStatus}`} />
        <span>
          {audioStatus === 'ready'
            ? 'Ready. Drop any sound on the source panel, auto-slice it, then shape each pad.'
            : 'Click a pad to start audio. Drop any sound anywhere to build a kit.'}
        </span>
      </div>
      <div className="footer-meta">
        <span className="footer-chip">
          <Icon name="sliders" size={12} /> 4 MODEL FAMILIES
        </span>
        <span className="footer-chip">8 STEREO OUTS</span>
        <span className="footer-chip">{audioStatus === 'ready' ? `${(sampleRate / 1000).toFixed(1)} kHz` : '48 kHz'} · 24-BIT EXPORT</span>
        <span className="footer-chip">LOCAL · NO UPLOAD</span>
      </div>
      <div className="footer-brand" aria-label="VRAC+ web edition">
        <span className="footer-wordmark">
          VRAC<span>+</span>
        </span>
        <span className="footer-sub">WEB EDITION · 1.0</span>
      </div>
    </footer>
  );
}
