import type { PadParams } from '../types';
import { FAMILY_META, MODEL_BANK } from '../types';

const NOTE_TO_KEY: Record<number, string> = (() => {
  const map: Record<number, string> = {};
  const rows = [
    ['1', '2', '3', '4'],
    ['Q', 'W', 'E', 'R'],
    ['A', 'S', 'D', 'F'],
    ['Z', 'X', 'C', 'V'],
  ];
  rows.forEach((row, rowIndex) => {
    row.forEach((key, colIndex) => {
      map[(3 - rowIndex) * 4 + colIndex] = key;
    });
  });
  return map;
})();

export interface PadGridProps {
  pads: PadParams[];
  selectedPad: number;
  activePads: Record<number, number>;
  onTrigger: (index: number, velocity: number) => void;
  onSelect: (index: number) => void;
  onToggleMute: (index: number) => void;
  onToggleSolo: (index: number) => void;
}

function velocityFromPointer(event: React.PointerEvent<HTMLButtonElement>): number {
  if (event.shiftKey) return 1;
  if (event.altKey) return 0.3;
  const rect = event.currentTarget.getBoundingClientRect();
  const relative = (rect.bottom - event.clientY) / Math.max(1, rect.height);
  return Math.max(0.32, Math.min(1, 0.35 + relative * 0.65));
}

export function PadGrid({
  pads,
  selectedPad,
  activePads,
  onTrigger,
  onSelect,
  onToggleMute,
  onToggleSolo,
}: PadGridProps) {
  const rows = [3, 2, 1, 0];
  const now = Date.now();

  return (
    <div className="pad-grid" role="grid" aria-label="Drum pads">
      {rows.map((row) => (
        <div className="pad-row" role="row" key={row}>
          {[0, 1, 2, 3].map((col) => {
            const index = row * 4 + col;
            const pad = pads[index];
            const meta = FAMILY_META[pad.family];
            const active = (activePads[index] ?? 0) > now - 160;
            const selected = index === selectedPad;
            return (
              <div
                className={`pad-cell ${selected ? 'is-selected' : ''} ${pad.mute ? 'is-muted' : ''} ${pad.solo ? 'is-solo' : ''}`}
                key={index}
                role="gridcell"
                style={{ ['--pad-accent' as string]: meta.color }}
              >
                <button
                  type="button"
                  className={`pad ${active ? 'is-active' : ''}`}
                  aria-label={`${pad.note} ${meta.label} ${MODEL_BANK[pad.family][pad.model].name}`}
                  aria-pressed={selected}
                  onPointerDown={(event) => {
                    if (event.button !== 0) return;
                    event.preventDefault();
                    onSelect(index);
                    onTrigger(index, velocityFromPointer(event));
                  }}
                  onKeyDown={(event) => {
                    if (event.key === 'Enter' || event.key === ' ') {
                      event.preventDefault();
                      onSelect(index);
                      onTrigger(index, 0.85);
                    }
                  }}
                >
                  <span className="pad-corner" aria-hidden="true" />
                  <span className="pad-keyhint">{NOTE_TO_KEY[index]}</span>
                  <span className="pad-note">{pad.note}</span>
                  <span className="pad-code">
                    {MODEL_BANK[pad.family][pad.model].code}
                    <span className="pad-code-dot">{Math.round(pad.drumify * 100)}</span>
                  </span>
                  <span className="pad-flash" aria-hidden="true" />
                </button>
                <div className="pad-toggles">
                  <button
                    type="button"
                    className={`mini-toggle ${pad.mute ? 'is-on' : ''}`}
                    aria-label={`Mute ${pad.note}`}
                    aria-pressed={pad.mute}
                    title={`Mute ${pad.note}`}
                    onPointerDown={(event) => event.stopPropagation()}
                    onClick={(event) => {
                      event.stopPropagation();
                      onToggleMute(index);
                    }}
                  >
                    M
                  </button>
                  <button
                    type="button"
                    className={`mini-toggle ${pad.solo ? 'is-on' : ''}`}
                    aria-label={`Solo ${pad.note}`}
                    aria-pressed={pad.solo}
                    title={`Solo ${pad.note}`}
                    onPointerDown={(event) => event.stopPropagation()}
                    onClick={(event) => {
                      event.stopPropagation();
                      onToggleSolo(index);
                    }}
                  >
                    S
                  </button>
                </div>
              </div>
            );
          })}
        </div>
      ))}
    </div>
  );
}
