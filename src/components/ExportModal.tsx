import { useEffect, useState } from 'react';
import type { GlobalParams, PadParams, SourceSample } from '../types';
import { MODEL_BANK } from '../types';
import { downloadBlob, slugify } from '../state/storage';
import { exportKitZip, renderPadWav } from '../audio/exportKit';
import { Modal } from './Modal';
import { Icon } from './Icons';

type ExportMode = 'pad' | 'kit';

export function ExportModal({
  open,
  onClose,
  pad,
  pads,
  global,
  sample,
  kitName,
}: {
  open: boolean;
  onClose: () => void;
  pad: PadParams;
  pads: PadParams[];
  global: GlobalParams;
  sample: SourceSample | null;
  kitName: string;
}) {
  const [mode, setMode] = useState<ExportMode>('pad');
  const [busy, setBusy] = useState(false);
  const [done, setDone] = useState<number | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    if (open) {
      setDone(null);
      setError(null);
      setMode('pad');
    }
  }, [open]);

  const progress = done === null ? 0 : done;

  const runExport = async () => {
    if (busy) return;
    setBusy(true);
    setError(null);
    setDone(0);
    try {
      if (mode === 'pad') {
        setDone(1);
        const wav = await renderPadWav({ pad, pads, global, sample });
        downloadBlob(new Blob([wav.data as unknown as BlobPart], { type: 'audio/wav' }), `${slugify(kitName)}_${wav.name}`);
        setDone(100);
      } else {
        const blob = await exportKitZip(pads, global, sample, kitName, (completed, total) => {
          setDone(Math.round((completed / total) * 100));
        });
        downloadBlob(blob, `${slugify(kitName)}_vrac-kit.zip`);
        setDone(100);
      }
    } catch (err) {
      console.error('[SliceForge] export failed', err);
      setError(err instanceof Error ? err.message : 'Export failed');
      setDone(null);
    } finally {
      setBusy(false);
    }
  };

  return (
    <Modal
      open={open}
      onClose={onClose}
      eyebrow="PRINT & EXPORT"
      title="Take the kit with you"
      footer={
        <>
          <span className="modal-foot-note">24-bit / 48 kHz · rendered through the master chain</span>
          <button type="button" className="button button--ghost" onClick={onClose}>
            CLOSE
          </button>
          <button type="button" className="button button--primary" onClick={runExport} disabled={busy}>
            {busy ? `RENDERING ${progress}%` : mode === 'pad' ? 'EXPORT PAD WAV' : 'EXPORT FULL KIT'}
          </button>
        </>
      }
    >
      <p className="modal-lead">
        Print audio that sounds like what you're hearing. Every pad gets its full tail; the global
        filter, tone and master stage are baked in.
      </p>

      <div className="export-options">
        <button
          type="button"
          className={`export-option ${mode === 'pad' ? 'is-selected' : ''}`}
          onClick={() => setMode('pad')}
        >
          <span className="export-radio" />
          <div>
            <strong>SELECTED PAD · {pad.note}</strong>
            <p>
              One WAV of the current pad — {pad.family.toUpperCase()} /{' '}
              {MODEL_BANK[pad.family][pad.model].name}, output {pad.bus}. Great for dropping a
              single hit into a sampler.
            </p>
          </div>
          <span className="export-size">1 FILE</span>
        </button>
        <button
          type="button"
          className={`export-option ${mode === 'kit' ? 'is-selected' : ''}`}
          onClick={() => setMode('kit')}
        >
          <span className="export-radio" />
          <div>
            <strong>FULL KIT · 16 PADS</strong>
            <p>
              Zipped folder with every pad as a named WAV, plus a kit manifest and a README. Files
              are grouped by output bus in their names.
            </p>
          </div>
          <span className="export-size">ZIP · 18 FILES</span>
        </button>
      </div>

      <div className="export-summary">
        <div>
          <span className="mini-label">KIT</span>
          <span>{kitName}</span>
        </div>
        <div>
          <span className="mini-label">SOURCE</span>
          <span>{sample?.name ?? 'No sample'}</span>
        </div>
        <div>
          <span className="mini-label">FORMAT</span>
          <span>WAV · 24-BIT · 48 KHZ</span>
        </div>
      </div>

      {(busy || done !== null) && (
        <div className="export-progress">
          <div className="export-progress-head">
            <span>{busy ? 'Rendering audio…' : done === 100 ? 'Export complete' : 'Preparing…'}</span>
            <span className="mono">{Math.round(progress)}%</span>
          </div>
          <div className="progress-track">
            <div className="progress-fill" style={{ width: `${progress}%` }} />
          </div>
          <p className="form-note">
            Keep this tab focused while rendering. The one-shots are generated offline at full
            quality.
          </p>
        </div>
      )}

      {done === 100 && !busy && (
        <div className="export-success">
          <Icon name="check" size={16} />
          <span>Downloaded. Check your browser's downloads for the {mode === 'pad' ? 'WAV' : 'ZIP'}.</span>
        </div>
      )}

      {error && (
        <div className="inline-error">
          <Icon name="info" size={14} /> {error}
        </div>
      )}

      <div className="export-note">
        <Icon name="info" size={14} />
        <p>
          Tip: pad names use the note, family and stereo output (for example <code>D1_SNARE_2.wav</code>).
          Drag the files straight into your DAW's sampler or clip view.
        </p>
      </div>
    </Modal>
  );
}
