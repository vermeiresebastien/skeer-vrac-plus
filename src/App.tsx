import { useCallback, useEffect, useLayoutEffect, useRef, useState } from 'react';
import type { PadParams, SourceSample } from './types';
import { LIBRARY, PAD_COUNT } from './types';
import { audioEngine, type AudioStatus } from './audio/audioEngine';
import { midiHub, type MidiInputInfo, type MidiStatus } from './audio/midi';
import { decodeAudioFile } from './lib/audioFiles';
import { applyMidiValue, formatMidiTarget, parseMidiTarget } from './lib/midiTargets';
import { useKit } from './state/useKit';
import { loadUserSample as restoreUserSample, persistUserSample } from './state/sampleStore';
import { FACTORY_PRESETS } from './state/factoryPresets';
import {
  downloadBlob,
  getThemePreference,
  loadMidiMappings,
  loadSavedKits,
  saveUserKit,
  setThemePreference,
  slugify,
  writeMidiMappings,
  type MidiMapping,
  type SavedKit,
} from './state/storage';
import { Header } from './components/Header';
import { SourcePanel } from './components/SourcePanel';
import { PadsPanel } from './components/PadsPanel';
import { PadEditor } from './components/PadEditor';
import { Footer } from './components/Footer';
import { HelpModal } from './components/HelpModal';
import { AboutModal } from './components/AboutModal';
import { LibraryModal } from './components/LibraryModal';
import { RecordModal } from './components/RecordModal';
import { ExportModal } from './components/ExportModal';
import { SavePresetModal } from './components/SavePresetModal';
import { MidiModal } from './components/MidiModal';
import { Toasts, type Toast, type ToastKind } from './components/Toasts';
import { Icon } from './components/Icons';

type ModalKind = 'help' | 'about' | 'library' | 'record' | 'export' | 'save' | 'midi' | null;

const KEY_TO_PAD: Record<string, number> = {
  Digit1: 12, Digit2: 13, Digit3: 14, Digit4: 15,
  KeyQ: 8, KeyW: 9, KeyE: 10, KeyR: 11,
  KeyA: 4, KeyS: 5, KeyD: 6, KeyF: 7,
  KeyZ: 0, KeyX: 1, KeyC: 2, KeyV: 3,
};

function isEditable(target: EventTarget | null): boolean {
  if (!(target instanceof HTMLElement)) return false;
  const tag = target.tagName.toLowerCase();
  return tag === 'input' || tag === 'textarea' || tag === 'select' || target.isContentEditable;
}

export default function App() {
  const { kit, sample, slices, selectedPad, actions } = useKit();
  const [theme, setTheme] = useState<'light' | 'dark'>(() => getThemePreference());
  const [modal, setModal] = useState<ModalKind>(null);
  const [savedKits, setSavedKits] = useState<SavedKit[]>(() => loadSavedKits());
  const [audioStatus, setAudioStatus] = useState<AudioStatus>(audioEngine.status);
  const [activePads, setActivePads] = useState<Record<number, number>>({});
  const [copiedPad, setCopiedPad] = useState<PadParams | null>(null);
  const [toasts, setToasts] = useState<Toast[]>([]);
  const [midiStatus, setMidiStatus] = useState<MidiStatus>(midiHub.status);
  const [midiInputs, setMidiInputs] = useState<MidiInputInfo[]>([]);
  const [mappings, setMappings] = useState<MidiMapping[]>(() => loadMidiMappings());
  const [learnTarget, setLearnTarget] = useState<string | null>(null);
  const [dragActive, setDragActive] = useState(false);

  const toastId = useRef(0);
  const flashTimers = useRef(new Map<number, number>());
  const dragDepth = useRef(0);
  const fileInputRef = useRef<HTMLInputElement | null>(null);
  const kitRef = useRef(kit);
  const selectedPadRef = useRef(selectedPad);
  const slicesRef = useRef(slices);
  const actionsRef = useRef(actions);
  const triggerRef = useRef<(index: number, velocity?: number, source?: 'ui' | 'key' | 'midi') => void>(() => undefined);
  const mappingsRef = useRef(mappings);
  const learnRef = useRef<string | null>(null);
  const midiStatusRef = useRef<MidiStatus>(midiStatus);

  kitRef.current = kit;
  selectedPadRef.current = selectedPad;
  slicesRef.current = slices;
  actionsRef.current = actions;
  mappingsRef.current = mappings;
  learnRef.current = learnTarget;
  midiStatusRef.current = midiStatus;

  const notify = useCallback((message: string, kind: ToastKind = 'info') => {
    const id = ++toastId.current;
    setToasts((prev) => [...prev.slice(-2), { id, message, kind }]);
    window.setTimeout(() => {
      setToasts((prev) => prev.filter((toast) => toast.id !== id));
    }, 4200);
  }, []);

  const dismissToast = useCallback((id: number) => {
    setToasts((prev) => prev.filter((toast) => toast.id !== id));
  }, []);

  const flashPad = useCallback((index: number) => {
    setActivePads((prev) => ({ ...prev, [index]: Date.now() }));
    const existing = flashTimers.current.get(index);
    if (existing) window.clearTimeout(existing);
    const timer = window.setTimeout(() => {
      setActivePads((prev) => {
        const next = { ...prev };
        delete next[index];
        return next;
      });
      flashTimers.current.delete(index);
    }, 190);
    flashTimers.current.set(index, timer);
  }, []);

  useEffect(() => {
    const timers = flashTimers.current;
    return () => {
      timers.forEach((timer) => window.clearTimeout(timer));
    };
  }, []);

  const startAudio = useCallback(() => {
    void audioEngine.start();
  }, []);

  // Wake the audio context on the first interaction so the first pad hit is
  // as immediate as the browser allows.
  useEffect(() => {
    const warm = () => {
      void audioEngine.start();
    };
    window.addEventListener('pointerdown', warm, { once: true });
    window.addEventListener('keydown', warm, { once: true });
    return () => {
      window.removeEventListener('pointerdown', warm);
      window.removeEventListener('keydown', warm);
    };
  }, []);

  const triggerPad = useCallback(
    (index: number, velocity = 0.85, source: 'ui' | 'key' | 'midi' = 'ui') => {
      if (index < 0 || index >= PAD_COUNT) return;
      audioEngine.trigger(index, velocity);
      flashPad(index);
      if (source !== 'ui' && kitRef.current.autoSelect) {
        actionsRef.current.selectPad(index);
      }
    },
    [flashPad],
  );
  triggerRef.current = triggerPad;

  /* ---- audio engine wiring -------------------------------------- */
  useEffect(() => {
    const unsubscribe = audioEngine.onStatus((status, detail) => {
      setAudioStatus(status);
      if (status === 'error') notify(detail ?? 'Audio could not start. Try Chrome or Edge.', 'error');
    });
    return unsubscribe;
  }, [notify]);

  useLayoutEffect(() => {
    audioEngine.setSample(sample);
  }, [sample]);

  useLayoutEffect(() => {
    audioEngine.setParams(kit.pads, kit.global);
  }, [kit.pads, kit.global]);

  useEffect(() => {
    document.documentElement.dataset.theme = theme;
    setThemePreference(theme);
  }, [theme]);

  // Small debug surface used by the browser smoke tests; harmless in production.
  useEffect(() => {
    (window as unknown as { __SLICEFORGE__?: unknown }).__SLICEFORGE__ = {
      audioEngine,
      trigger: (index: number, velocity?: number) => triggerRef.current(index, velocity ?? 0.9, 'ui'),
      getKit: () => kitRef.current,
      getSelectedPad: () => selectedPadRef.current,
      getSlices: () => slicesRef.current,
      actions,
    };
  }, [actions]);

  /* ---- MIDI ------------------------------------------------------ */
  useEffect(() => {
    midiHub.onNote = (pad, velocity) => triggerRef.current(pad, velocity, 'midi');
    midiHub.onState = (status, inputs, message) => {
      setMidiStatus(status);
      setMidiInputs(inputs);
      if (message) notify(message, 'success');
    };
    midiHub.onCC = (cc, value) => {
      const mapping = mappingsRef.current.find((entry) => entry.cc === cc);
      if (!mapping) return;
      const target = parseMidiTarget(mapping.target);
      if (!target) return;
      applyMidiValue(target, value, {
        updatePad: actionsRef.current.updatePad,
        setGlobal: actionsRef.current.setGlobal,
      });
    };
    midiHub.onLearn = (cc) => {
      const target = learnRef.current;
      if (!target) return;
      setMappings((prev) => {
        const next = [...prev.filter((entry) => entry.cc !== cc && entry.target !== target), { cc, target }];
        writeMidiMappings(next);
        return next;
      });
      setLearnTarget(null);
      midiHub.clearLearn();
      notify(`MIDI learned: CC ${cc} → ${formatMidiTarget(target, kitRef.current.pads)}`, 'success');
    };
    return () => {
      midiHub.onNote = null;
      midiHub.onState = null;
      midiHub.onCC = null;
      midiHub.onLearn = null;
    };
  }, [notify]);

  const enableMidi = useCallback(async () => {
    const ok = await midiHub.enable();
    if (ok) notify('MIDI enabled. Play notes C1–D#2 or right-click a knob to learn a CC.', 'success');
  }, [notify]);

  const requestMidiLearn = useCallback(
    (target: string) => {
      midiHub.armLearn(target);
      setLearnTarget(target);
      notify(`MIDI learn armed: ${formatMidiTarget(target, kit.pads)} — move a CC`, 'learn');
      if (midiStatusRef.current !== 'ready') void enableMidi();
    },
    [enableMidi, kit.pads, notify],
  );

  const cancelMidiLearn = useCallback(() => {
    midiHub.clearLearn();
    setLearnTarget(null);
  }, []);

  const removeMapping = useCallback((cc: number) => {
    setMappings((prev) => {
      const next = prev.filter((entry) => entry.cc !== cc);
      writeMidiMappings(next);
      return next;
    });
  }, []);

  const clearMappings = useCallback(() => {
    setMappings([]);
    writeMidiMappings([]);
  }, []);

  /* ---- sample loading ------------------------------------------- */
  const handleFiles = useCallback(
    async (files: FileList | File[]) => {
      const file = Array.from(files).find(Boolean);
      if (!file) return;
      notify(`Loading ${file.name}…`, 'info');
      try {
        const loaded = await decodeAudioFile(file);
        actionsRef.current.loadUserSample(loaded);
        void persistUserSample(loaded);
        notify(`${loaded.name} loaded and sliced.`, 'success');
      } catch (error) {
        console.error('[SliceForge] sample load failed', error);
        notify(error instanceof Error ? error.message : 'Could not decode that file.', 'error');
      }
    },
    [notify],
  );

  const loadLibrarySource = useCallback(
    (sourceId: string) => {
      actionsRef.current.loadLibrarySource(sourceId);
      const source = LIBRARY.find((entry) => entry.id === sourceId);
      notify(`Source loaded: ${source?.name ?? sourceId}.`, 'success');
    },
    [notify],
  );

  const recordSample = useCallback(
    (loaded: SourceSample) => {
      actionsRef.current.loadUserSample(loaded);
      void persistUserSample(loaded);
      setModal(null);
      notify(`Recording loaded: ${loaded.duration.toFixed(1)}s, sliced.`, 'success');
    },
    [notify],
  );

  /* ---- slice navigation ------------------------------------------ */
  const navigateSlice = useCallback(
    (direction: 1 | -1 | 'random') => {
      const list = slicesRef.current;
      if (list.length === 0) return;
      const pad = selectedPadRef.current;
      const current = list.reduce(
        (best, slice, index) =>
          Math.abs(slice.time - pad.sampleStart) < Math.abs(list[best].time - pad.sampleStart) ? index : best,
        0,
      );
      let next = current;
      if (direction === 'random') {
        next = Math.floor(Math.random() * list.length);
      } else {
        next = (current + direction + list.length) % list.length;
      }
      actionsRef.current.beginGesture();
      actionsRef.current.updatePad(pad.id, { sampleStart: list[next].time, sliceIndex: next });

      audioEngine.audition({ ...pad, sampleStart: list[next].time, sliceIndex: next }, 0.9);
      flashPad(pad.id);
    },
    [flashPad],
  );

  /* ---- keyboard -------------------------------------------------- */
  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      if (isEditable(event.target)) return;
      const modifier = event.metaKey || event.ctrlKey;
      if (modifier && event.key.toLowerCase() === 'z') {
        event.preventDefault();
        if (event.shiftKey) actionsRef.current.redo();
        else actionsRef.current.undo();
        return;
      }
      if (modifier && event.key.toLowerCase() === 'y') {
        event.preventDefault();
        actionsRef.current.redo();
        return;
      }
      if (modal) return;
      if (event.repeat) return;
      if (modifier || event.altKey) return;

      if (event.code === 'Space') {
        event.preventDefault();
        audioEngine.allOff();
        return;
      }
      if (event.code === 'BracketLeft') {
        event.preventDefault();
        navigateSlice(-1);
        return;
      }
      if (event.code === 'BracketRight') {
        event.preventDefault();
        navigateSlice(1);
        return;
      }
      if (event.code === 'KeyT' && !event.shiftKey) {
        event.preventDefault();
        navigateSlice('random');
        return;
      }
      const padIndex = KEY_TO_PAD[event.code];
      if (padIndex !== undefined) {
        event.preventDefault();
        const velocity = event.shiftKey ? 1 : 0.82;
        triggerRef.current(padIndex, velocity, 'key');
      }
    };
    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, [modal, navigateSlice]);

  /* ---- preset actions ------------------------------------------- */
  const loadPreset = useCallback(
    (id: string) => {
      const preset = FACTORY_PRESETS.find((entry) => entry.id === id);
      actionsRef.current.loadPreset(id);
      notify(`Loaded ${preset?.name ?? 'preset'}.`, 'success');
    },
    [notify],
  );

  const restoreKit = useCallback(async (savedKit: typeof kit): Promise<boolean> => {
    let providedSample: SourceSample | undefined;
    if (savedKit.sourceId.startsWith('user-') && savedKit.sourceId !== kitRef.current.sourceId) {
      providedSample = (await restoreUserSample(savedKit.sourceId)) ?? undefined;
      if (!providedSample) return false;
    }
    return actionsRef.current.loadSavedKit(savedKit, providedSample);
  }, []);

  const loadSavedKit = useCallback(
    async (savedKit: typeof kit) => {
      const ok = await restoreKit(savedKit);
      if (ok) notify(`Loaded ${savedKit.presetName}.`, 'success');
      else notify('This kit uses a custom sample that is not available. Load the audio file first, then try again.', 'error');
    },
    [notify, restoreKit],
  );

  const savePreset = useCallback(
    (name: string) => {
      const next = saveUserKit(name, kitRef.current);
      setSavedKits(next);
      actionsRef.current.renameKit(name);
      setModal(null);
      notify(`Kit saved as ${name}.`, 'success');
    },
    [notify],
  );

  const exportPreset = useCallback(() => {
    const payload = {
      type: 'vrac-plus-preset',
      version: 1,
      exportedAt: new Date().toISOString(),
      kit: kitRef.current,
    };
    const blob = new Blob([JSON.stringify(payload, null, 2)], { type: 'application/json' });
    downloadBlob(blob, `${slugify(kitRef.current.presetName)}.vracpreset.json`);
    notify('Preset JSON exported.', 'success');
  }, [notify]);

  const importPreset = useCallback(
    async (file: File) => {
      try {
        const text = await file.text();
        const parsed = JSON.parse(text) as { kit?: unknown } | unknown;
        const kitCandidate =
          parsed && typeof parsed === 'object' && 'kit' in (parsed as Record<string, unknown>)
            ? (parsed as { kit: unknown }).kit
            : parsed;
        if (!kitCandidate || typeof kitCandidate !== 'object') throw new Error('That is not a SliceForge preset.');
        const typed = kitCandidate as typeof kit;
        if (!Array.isArray(typed.pads) || typed.pads.length !== PAD_COUNT) throw new Error('The preset has no pad data.');
        const ok = await restoreKit(typed);
        if (!ok) throw new Error('The preset references a custom sample that is not available. Load that audio file first.');
        notify(`Imported ${typed.presetName ?? 'preset'}.`, 'success');
      } catch (error) {
        notify(error instanceof Error ? error.message : 'Could not import preset.', 'error');
      }
    },
    [kit, notify, restoreKit],
  );

  /* ---- pad editing ---------------------------------------------- */
  const toggleMute = useCallback(
    (index: number) => {
      actions.toggleMute(index);
    },
    [actions],
  );

  const handleModelSelect = useCallback(
    (family: PadParams['family'], modelIndex: number) => {
      const pad = selectedPadRef.current;
      const next = { ...pad, family, model: modelIndex };
      actions.beginGesture();
      actions.updatePad(pad.id, { family, model: modelIndex });
      audioEngine.audition(next, 0.9);
      flashPad(pad.id);
    },
    [actions, flashPad],
  );

  const setChoke = useCallback(
    (group: number) => {
      actions.beginGesture();
      actions.updatePad(selectedPadRef.current.id, { choke: group });
    },
    [actions],
  );

  const setBus = useCallback(
    (bus: number) => {
      actions.beginGesture();
      actions.updatePad(selectedPadRef.current.id, { bus });
    },
    [actions],
  );

  const copyPad = useCallback(() => {
    setCopiedPad({ ...selectedPadRef.current });
    notify(`${selectedPadRef.current.note} settings copied.`, 'success');
  }, [notify]);

  const pastePad = useCallback(() => {
    if (!copiedPad) {
      notify('Copy a pad first.', 'info');
      return;
    }
    const target = selectedPadRef.current;
    const patch = {
      family: copiedPad.family,
      model: copiedPad.model,
      drumify: copiedPad.drumify,
      originalGain: copiedPad.originalGain,
      drumifiedGain: copiedPad.drumifiedGain,
      pitch: copiedPad.pitch,
      length: copiedPad.length,
      volume: copiedPad.volume,
      pan: copiedPad.pan,
      choke: copiedPad.choke,
      velocityAmount: copiedPad.velocityAmount,
      warp: copiedPad.warp,
      impact: copiedPad.impact,
      instability: copiedPad.instability,
      toneShift: copiedPad.toneShift,
      super: copiedPad.super,
      eqLow: copiedPad.eqLow,
      eqMid: copiedPad.eqMid,
      eqHigh: copiedPad.eqHigh,
    };
    actions.beginGesture();
    actions.updatePad(target.id, patch);
    notify(`Settings pasted to ${target.note}.`, 'success');
  }, [actions, copiedPad, notify]);

  const auditionOverride = useCallback(
    (pad: PadParams) => {
      audioEngine.audition(pad, 0.9);
      flashPad(pad.id);
    },
    [flashPad],
  );

  /* ---- render ---------------------------------------------------- */
  return (
    <div
      className={`app ${dragActive ? 'is-dragging-file' : ''}`}
      data-theme={theme}
      onDragEnter={(event) => {
        if (!event.dataTransfer.types.includes('Files')) return;
        event.preventDefault();
        dragDepth.current += 1;
        setDragActive(true);
      }}
      onDragOver={(event) => {
        if (event.dataTransfer.types.includes('Files')) event.preventDefault();
      }}
      onDragLeave={(event) => {
        if (!event.dataTransfer.types.includes('Files')) return;
        dragDepth.current = Math.max(0, dragDepth.current - 1);
        if (dragDepth.current === 0) setDragActive(false);
      }}
      onDrop={(event) => {
        if (!event.dataTransfer.files.length) return;
        event.preventDefault();
        dragDepth.current = 0;
        setDragActive(false);
        void handleFiles(event.dataTransfer.files);
      }}
    >
      <div className="app-shell">
        <Header
          kit={kit}
          savedKits={savedKits}
          audioStatus={audioStatus}
          sampleRate={audioEngine.sampleRate}
          midiStatus={midiStatus}
          theme={theme}
          canUndo={actions.canUndo}
          canRedo={actions.canRedo}
          onUndo={actions.undo}
          onRedo={actions.redo}
          onRandomize={() => {
            actions.randomize();
            notify('Randomised kit. Undo is available.', 'success');
          }}
          onLoadPreset={loadPreset}
          onLoadSavedKit={loadSavedKit}
          onOpenSave={() => setModal('save')}
          onOpenExport={() => setModal('export')}
          onOpenMidi={() => setModal('midi')}
          onOpenHelp={() => setModal('help')}
          onOpenAbout={() => setModal('about')}
          onToggleTheme={() => setTheme((current) => (current === 'light' ? 'dark' : 'light'))}
          onStartAudio={startAudio}
          onStopAll={() => {
            audioEngine.allOff();
            notify('All voices stopped.', 'info');
          }}
          onExportPreset={exportPreset}
          onImportPreset={(file) => void importPreset(file)}
        />

        <main className="workspace">
          <SourcePanel
            sample={sample}
            slices={slices}
            selectedPad={selectedPad}
            global={kit.global}
            sensitivity={kit.sensitivity}
            onSampleStart={(time) => actions.setPadStart(selectedPad.id, time)}
            onGestureStart={actions.beginGesture}
            onSensitivity={actions.setSensitivity}
            onAddSlice={() => {
              actions.addSlice(selectedPad.sampleStart);
              notify(`Slice added at ${selectedPad.sampleStart.toFixed(2)}s.`, 'success');
            }}
            onDeleteSlice={() => {
              actions.deleteSlice(selectedPad.sampleStart);
              notify('Nearest slice deleted.', 'info');
            }}
            onGlobal={actions.setGlobal}
            onOpenLibrary={() => setModal('library')}
            onBrowse={() => fileInputRef.current?.click()}
            onRecord={() => setModal('record')}
            dragActive={dragActive}
            onPrevSlice={() => navigateSlice(-1)}
            onNextSlice={() => navigateSlice(1)}
            onRandomSlice={() => navigateSlice('random')}
            onMidiLearn={requestMidiLearn}
          />

          <PadsPanel
            pads={kit.pads}
            selectedPad={kit.selectedPad}
            activePads={activePads}
            autoSelect={kit.autoSelect}
            onAutoSelect={actions.setAutoSelect}
            onTrigger={triggerPad}
            onSelect={actions.selectPad}
            onToggleMute={toggleMute}
            onToggleSolo={actions.toggleSolo}
            onPanic={() => audioEngine.allOff()}
          />

          <PadEditor
            pad={selectedPad}
            sample={sample}
            slices={slices}
            canPaste={Boolean(copiedPad)}
            onPadChange={(patch) => actions.updatePad(selectedPad.id, patch)}
            onPadStart={(time) => actions.setPadStart(selectedPad.id, time)}
            onGestureStart={actions.beginGesture}
            onModelSelect={handleModelSelect}
            onChoke={setChoke}
            onBus={setBus}
            onTrigger={(velocity) => triggerPad(selectedPad.id, velocity ?? 0.9, 'ui')}
            onAuditionOverride={auditionOverride}
            onMidiLearn={requestMidiLearn}
            onPrevSlice={() => navigateSlice(-1)}
            onNextSlice={() => navigateSlice(1)}
            onRandomSlice={() => navigateSlice('random')}
            onCopy={copyPad}
            onPaste={pastePad}
            onReset={() => {
              actions.resetPad(selectedPad.id);
              notify(`${selectedPad.note} reset.`, 'success');
            }}
          />
        </main>

        <Footer audioStatus={audioStatus} sampleRate={audioEngine.sampleRate} />
      </div>

      <input
        ref={fileInputRef}
        type="file"
        accept="audio/*,.wav,.aif,.aiff,.mp3,.flac,.ogg,.m4a"
        hidden
        onChange={(event) => {
          if (event.currentTarget.files) void handleFiles(event.currentTarget.files);
          event.currentTarget.value = '';
        }}
      />

      <HelpModal open={modal === 'help'} onClose={() => setModal(null)} />
      <AboutModal open={modal === 'about'} onClose={() => setModal(null)} />
      <LibraryModal
        open={modal === 'library'}
        onClose={() => setModal(null)}
        currentSourceId={kit.sourceId}
        onSelect={loadLibrarySource}
        onBrowse={() => {
          setModal(null);
          fileInputRef.current?.click();
        }}
        onRecord={() => setModal('record')}
      />
      <RecordModal
        open={modal === 'record'}
        onClose={() => setModal(null)}
        onSample={recordSample}
      />
      <ExportModal
        open={modal === 'export'}
        onClose={() => setModal(null)}
        pad={selectedPad}
        pads={kit.pads}
        global={kit.global}
        sample={sample}
        kitName={kit.presetName}
      />
      <SavePresetModal
        open={modal === 'save'}
        onClose={() => setModal(null)}
        defaultName={kit.presetName}
        onSave={savePreset}
      />
      <MidiModal
        open={modal === 'midi'}
        onClose={() => setModal(null)}
        status={midiStatus}
        inputs={midiInputs}
        mappings={mappings}
        learnTarget={learnTarget}
        pads={kit.pads}
        onEnable={() => void enableMidi()}
        onClearMappings={clearMappings}
        onRemoveMapping={removeMapping}
      />

      <Toasts toasts={toasts} onDismiss={dismissToast} />

      {learnTarget && (
        <div className="learn-float" role="status">
          <span className="pulse-dot" />
          <span>
            MIDI learn: <strong>{formatMidiTarget(learnTarget, kit.pads)}</strong> — move a CC on your controller
          </span>
          <button type="button" className="text-button" onClick={cancelMidiLearn}>
            CANCEL
          </button>
        </div>
      )}

      {dragActive && (
        <div className="app-drop-overlay" aria-hidden="true">
          <div className="app-drop-card">
            <Icon name="upload" size={26} />
            <strong>DROP ANY SOUND</strong>
            <span>WAV · AIFF · MP3 · FLAC · OGG · M4A</span>
          </div>
        </div>
      )}

    </div>
  );
}
