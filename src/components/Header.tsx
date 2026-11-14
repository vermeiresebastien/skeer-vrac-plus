import { useEffect, useRef, useState } from 'react';
import type { KitState } from '../types';
import { LIBRARY } from '../types';
import type { AudioStatus } from '../audio/audioEngine';
import type { SavedKit } from '../state/storage';
import { FACTORY_PRESETS } from '../state/factoryPresets';
import type { MidiStatus } from '../audio/midi';
import { Icon } from './Icons';

export interface HeaderProps {
  kit: KitState;
  savedKits: SavedKit[];
  audioStatus: AudioStatus;
  sampleRate: number;
  midiStatus: MidiStatus;
  theme: 'light' | 'dark';
  canUndo: boolean;
  canRedo: boolean;
  onUndo: () => void;
  onRedo: () => void;
  onRandomize: () => void;
  onLoadPreset: (id: string) => void;
  onLoadSavedKit: (kit: KitState) => void;
  onOpenSave: () => void;
  onOpenExport: () => void;
  onOpenMidi: () => void;
  onOpenHelp: () => void;
  onOpenAbout: () => void;
  onToggleTheme: () => void;
  onStartAudio: () => void;
  onStopAll: () => void;
  onExportPreset: () => void;
  onImportPreset: (file: File) => void;
}

function currentFactoryIndex(kit: KitState): number {
  return FACTORY_PRESETS.findIndex((preset) => preset.name === kit.presetName);
}

export function Header(props: HeaderProps) {
  const {
    kit,
    savedKits,
    audioStatus,
    sampleRate,
    midiStatus,
    theme,
    canUndo,
    canRedo,
    onUndo,
    onRedo,
    onRandomize,
    onLoadPreset,
    onLoadSavedKit,
    onOpenSave,
    onOpenExport,
    onOpenMidi,
    onOpenHelp,
    onOpenAbout,
    onToggleTheme,
    onStartAudio,
    onStopAll,
    onExportPreset,
    onImportPreset,
  } = props;

  const [menuOpen, setMenuOpen] = useState(false);
  const menuRef = useRef<HTMLDivElement | null>(null);
  const importRef = useRef<HTMLInputElement | null>(null);
  const index = currentFactoryIndex(kit);
  const tag = index >= 0 ? FACTORY_PRESETS[index].tag : 'USER KIT';

  useEffect(() => {
    if (!menuOpen) return;
    const close = (event: MouseEvent) => {
      if (menuRef.current && !menuRef.current.contains(event.target as Node)) setMenuOpen(false);
    };
    const escape = (event: globalThis.KeyboardEvent) => {
      if (event.key === 'Escape') setMenuOpen(false);
    };
    document.addEventListener('mousedown', close);
    document.addEventListener('keydown', escape);
    return () => {
      document.removeEventListener('mousedown', close);
      document.removeEventListener('keydown', escape);
    };
  }, [menuOpen]);

  const stepPreset = (direction: number) => {
    const next = index < 0 ? 0 : (index + direction + FACTORY_PRESETS.length) % FACTORY_PRESETS.length;
    onLoadPreset(FACTORY_PRESETS[next].id);
  };

  const audioLabel =
    audioStatus === 'ready'
      ? `${(sampleRate / 1000).toFixed(1)} kHz`
      : audioStatus === 'starting'
        ? 'STARTING'
        : audioStatus === 'suspended'
          ? 'RESUME'
          : audioStatus === 'error'
            ? 'AUDIO ERROR'
            : 'AUDIO OFF';

  return (
    <header className="app-header">
      <div className="brand">
        <div className="brand-mark" aria-hidden="true">
          <span />
          <span />
          <span />
          <span />
        </div>
        <div className="brand-copy">
          <span className="brand-name">SliceForge</span>
          <span className="brand-sub">SAMPLE DRUM MACHINE</span>
        </div>
      </div>

      <div className="preset-browser" ref={menuRef}>
        <button type="button" className="preset-step" onClick={() => stepPreset(-1)} aria-label="Previous factory preset">
          <Icon name="left" size={14} />
        </button>
        <button
          type="button"
          className={`preset-current ${menuOpen ? 'is-open' : ''}`}
          onClick={() => setMenuOpen((open) => !open)}
          aria-expanded={menuOpen}
          aria-haspopup="listbox"
        >
          <span className="preset-tag">{tag}</span>
          <span className="preset-name">{kit.presetName}</span>
          <Icon name="down" size={13} className="preset-caret" />
        </button>
        <button type="button" className="preset-step" onClick={() => stepPreset(1)} aria-label="Next factory preset">
          <Icon name="right" size={14} />
        </button>

        {menuOpen && (
          <div className="preset-menu" role="listbox" aria-label="Presets">
            <div className="preset-menu-section">
              <span className="menu-eyebrow">FACTORY KITS</span>
              {FACTORY_PRESETS.map((preset) => (
                <button
                  type="button"
                  key={preset.id}
                  role="option"
                  aria-selected={index >= 0 && FACTORY_PRESETS[index].id === preset.id}
                  className={`preset-option ${index >= 0 && FACTORY_PRESETS[index].id === preset.id ? 'is-current' : ''}`}
                  onClick={() => {
                    onLoadPreset(preset.id);
                    setMenuOpen(false);
                  }}
                >
                  <span className="preset-option-main">
                    <strong>{preset.name}</strong>
                    <em>{LIBRARY.find((source) => source.id === preset.sourceId)?.name ?? preset.tag}</em>
                  </span>
                  <span className="preset-option-tag">{preset.tag}</span>
                </button>
              ))}
            </div>
            {savedKits.length > 0 && (
              <div className="preset-menu-section">
                <span className="menu-eyebrow">SAVED KITS</span>
                {savedKits.map((saved) => (
                  <button
                    type="button"
                    key={saved.id}
                    role="option"
                    aria-selected={kit.presetName === saved.name}
                    className={`preset-option ${kit.presetName === saved.name ? 'is-current' : ''}`}
                    onClick={() => {
                      onLoadSavedKit(saved.kit);
                      setMenuOpen(false);
                    }}
                  >
                    <span className="preset-option-main">
                      <strong>{saved.name}</strong>
                      <em>LOCAL</em>
                    </span>
                    <span className="preset-option-tag">{saved.kit.sourceName}</span>
                  </button>
                ))}
              </div>
            )}
            <div className="preset-menu-actions">
              <button type="button" onClick={() => { setMenuOpen(false); onOpenSave(); }}>
                <Icon name="save" size={14} /> Save current kit
              </button>
              <button type="button" onClick={() => { setMenuOpen(false); onExportPreset(); }}>
                <Icon name="download" size={14} /> Export preset
              </button>
              <button type="button" onClick={() => { setMenuOpen(false); importRef.current?.click(); }}>
                <Icon name="upload" size={14} /> Import preset
              </button>
            </div>
          </div>
        )}
        <input
          ref={importRef}
          type="file"
          accept="application/json,.json"
          hidden
          onChange={(event) => {
            const file = event.currentTarget.files?.[0];
            if (file) onImportPreset(file);
            event.currentTarget.value = '';
          }}
        />
        <button type="button" className="icon-button preset-dice" onClick={onRandomize} title="Randomize kit">
          <Icon name="dice" size={17} />
        </button>
        <button type="button" className="icon-button" onClick={onOpenSave} title="Save kit">
          <Icon name="save" size={16} />
        </button>
      </div>

      <div className="header-divider" />

      <div className="history-buttons">
        <button type="button" className="icon-button" onClick={onUndo} disabled={!canUndo} title="Undo (Ctrl+Z)">
          <Icon name="undo" size={16} />
        </button>
        <button type="button" className="icon-button" onClick={onRedo} disabled={!canRedo} title="Redo (Ctrl+Shift+Z)">
          <Icon name="redo" size={16} />
        </button>
      </div>

      <div className="header-spacer" />

      <div className="header-actions">
        <button
          type="button"
          className={`audio-status audio-status--${audioStatus}`}
          onClick={() => {
            if (audioStatus === 'ready') onStopAll();
            else onStartAudio();
          }}
          title={
            audioStatus === 'ready'
              ? 'Audio engine running — click to stop all voices'
              : 'Start the audio engine'
          }
        >
          <span className="audio-dot" />
          <span>{audioLabel}</span>
        </button>
        <button
          type="button"
          className={`icon-button midi-button ${midiStatus === 'ready' ? 'is-live' : ''}`}
          onClick={onOpenMidi}
          title="MIDI settings"
        >
          <Icon name="midi" size={17} />
          <span className="midi-status-dot" />
        </button>
        <button type="button" className="icon-button" onClick={onToggleTheme} title="Toggle light/dark theme">
          <Icon name={theme === 'light' ? 'moon' : 'sun'} size={16} />
        </button>
        <button type="button" className="icon-button" onClick={onOpenExport} title="Export audio">
          <Icon name="download" size={16} />
        </button>
        <button type="button" className="icon-button" onClick={onOpenHelp} title="Help & shortcuts">
          <Icon name="help" size={17} />
        </button>
        <button type="button" className="icon-button" onClick={onOpenAbout} title="About SliceForge">
          <Icon name="info" size={17} />
        </button>
      </div>
    </header>
  );
}
