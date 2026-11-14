import { useEffect, useState } from 'react';
import type { PeakData, SourceInfo } from '../types';
import { LIBRARY } from '../types';
import { generateLibrarySample } from '../audio/sampleLibrary';
import { Modal } from './Modal';
import { Icon } from './Icons';

export function LibraryModal({
  open,
  onClose,
  currentSourceId,
  onSelect,
  onBrowse,
  onRecord,
}: {
  open: boolean;
  onClose: () => void;
  currentSourceId: string;
  onSelect: (id: string) => void;
  onBrowse: () => void;
  onRecord: () => void;
}) {
  const [peaks, setPeaks] = useState<Record<string, PeakData>>({});
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    if (!open) return;
    let cancelled = false;
    const pending = [...LIBRARY];
    setLoading(true);
    const run = () => {
      if (cancelled) return;
      const info = pending.shift();
      if (!info) {
        setLoading(false);
        return;
      }
      try {
        const sample = generateLibrarySample(info.id);
        setPeaks((prev) => ({ ...prev, [info.id]: sample.peaks }));
      } catch (error) {
        console.error('[SliceForge] preview generation failed', error);
      }
      window.setTimeout(run, 10);
    };
    window.setTimeout(run, 30);
    return () => {
      cancelled = true;
    };
  }, [open]);

  return (
    <Modal
      open={open}
      onClose={onClose}
      eyebrow="SAMPLE LIBRARY"
      title="Find a source"
      wide
      footer={
        <>
          <span className="modal-foot-note">
            {loading ? 'Generating previews…' : 'Drop any audio file anywhere to slice your own sound.'}
          </span>
          <button type="button" className="button button--ghost" onClick={onBrowse}>
            <Icon name="upload" size={14} /> LOAD FILE
          </button>
          <button type="button" className="button button--ghost" onClick={onRecord}>
            <Icon name="record" size={12} /> RECORD INPUT
          </button>
        </>
      }
    >
      <p className="modal-lead">
        Six found-sound worlds, recorded as recipes and generated in your browser. Load one as the
        source for all sixteen pads; the slicer does the rest.
      </p>
      <div className="library-grid">
        {LIBRARY.map((source: SourceInfo, index) => {
          const preview = peaks[source.id];
          const current = currentSourceId === source.id;
          return (
            <button
              type="button"
              key={source.id}
              className={`library-card ${current ? 'is-current' : ''}`}
              onClick={() => {
                onSelect(source.id);
                onClose();
              }}
            >
              <div className="library-card-top">
                <span>SOURCE {String(index + 1).padStart(2, '0')}</span>
                <span className="mono">{source.duration.toFixed(1)}s</span>
              </div>
              <div className="library-thumb">
                {preview ? <SourceThumb peaks={preview} /> : <span className="thumb-skeleton" />}
              </div>
              <div className="library-card-main">
                <h3>{source.name}</h3>
                <p>{source.blurb}</p>
              </div>
              <div className="library-card-foot">
                <span>{source.character}</span>
                <span className="library-load">{current ? 'LOADED' : 'LOAD →'}</span>
              </div>
            </button>
          );
        })}
      </div>
    </Modal>
  );
}

function SourceThumb({ peaks }: { peaks: PeakData }) {
  const bars = 120;
  const per = Math.max(1, Math.floor(peaks.count / bars));
  const paths: string[] = [];
  for (let i = 0; i < bars; i++) {
    let lo = 0;
    let hi = 0;
    for (let b = i * per; b < Math.min(peaks.count, (i + 1) * per); b++) {
      if (peaks.minMax[b * 2] < lo) lo = peaks.minMax[b * 2];
      if (peaks.minMax[b * 2 + 1] > hi) hi = peaks.minMax[b * 2 + 1];
    }
    const x = i * (180 / bars);
    paths.push(`M${x.toFixed(1)} ${(20 - hi * 18).toFixed(1)}V${(20 - lo * 18).toFixed(1)}`);
  }
  return (
    <svg viewBox="0 0 180 40" preserveAspectRatio="none" aria-hidden="true">
      <path d={paths.join('')} />
    </svg>
  );
}
