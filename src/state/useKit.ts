import { useCallback, useMemo, useReducer, useRef } from 'react';
import type { GlobalParams, KitState, PadParams, SourceSample } from '../types';
import { PAD_COUNT, PAD_NOTES, clamp, createDefaultPads, makePad } from '../types';
import { detectSlices, remapStartsAfterReslice, type Slice } from '../audio/slicer';
import { generateLibrarySample } from '../audio/sampleLibrary';
import {
  FACTORY_PRESETS,
  buildInitialSession,
  buildPresetSession,
  buildSourceSession,
  type SessionData,
} from './factoryPresets';
import { randomizeGlobal, randomizePads } from './randomize';

const HISTORY_LIMIT = 80;

interface HistoryState {
  present: SessionData;
  past: SessionData[];
  future: SessionData[];
}

type Action =
  | { type: 'push' }
  | { type: 'undo' }
  | { type: 'redo' }
  | { type: 'pad'; id: number; patch: Partial<PadParams>; history?: boolean }
  | { type: 'padStart'; id: number; time: number; history?: boolean }
  | { type: 'global'; patch: Partial<GlobalParams>; history?: boolean }
  | { type: 'select'; id: number }
  | { type: 'autoSelect'; value: boolean }
  | { type: 'mute'; id: number }
  | { type: 'solo'; id: number }
  | { type: 'sensitivity'; value: number; history?: boolean }
  | { type: 'addSlice'; time: number }
  | { type: 'deleteSlice'; time: number }
  | { type: 'loadSession'; session: SessionData; history?: boolean }
  | { type: 'reslice'; slices: Slice[]; history?: boolean }
  | { type: 'randomize' }
  | { type: 'resetPad'; id: number }
  | { type: 'rename'; name: string }
  | { type: 'setKit'; kit: KitState; history?: boolean };

function pushPast(state: HistoryState): SessionData[] {
  return [...state.past, state.present].slice(-HISTORY_LIMIT);
}

function withPresent(state: HistoryState, present: SessionData, history: boolean): HistoryState {
  if (history) {
    return { present, past: pushPast(state), future: [] };
  }
  return { ...state, present, future: [] };
}

function mapPads(session: SessionData, fn: (pad: PadParams) => PadParams): SessionData {
  const pads = session.kit.pads.map(fn);
  return { ...session, kit: { ...session.kit, pads } };
}

function reducer(state: HistoryState, action: Action): HistoryState {
  switch (action.type) {
    case 'push':
      return { ...state, past: pushPast(state), future: [] };
    case 'undo': {
      if (state.past.length === 0) return state;
      const previous = state.past[state.past.length - 1];
      return {
        present: previous,
        past: state.past.slice(0, -1),
        future: [state.present, ...state.future].slice(0, HISTORY_LIMIT),
      };
    }
    case 'redo': {
      if (state.future.length === 0) return state;
      const next = state.future[0];
      return {
        present: next,
        past: [...state.past, state.present].slice(-HISTORY_LIMIT),
        future: state.future.slice(1),
      };
    }
    case 'pad': {
      const present = mapPads(state.present, (pad) =>
        pad.id === action.id ? { ...pad, ...action.patch, id: pad.id, note: pad.note } : pad,
      );
      return withPresent(state, present, Boolean(action.history));
    }
    case 'padStart': {
      const present = mapPads(state.present, (pad) =>
        pad.id === action.id
          ? { ...pad, sampleStart: Math.max(0, action.time), sliceIndex: -1 }
          : pad,
      );
      return withPresent(state, present, Boolean(action.history));
    }
    case 'global': {
      const present = {
        ...state.present,
        kit: { ...state.present.kit, global: { ...state.present.kit.global, ...action.patch } },
      };
      return withPresent(state, present, Boolean(action.history));
    }
    case 'select': {
      const id = clamp(action.id | 0, 0, PAD_COUNT - 1);
      if (id === state.present.kit.selectedPad) return state;
      return {
        ...state,
        present: { ...state.present, kit: { ...state.present.kit, selectedPad: id } },
      };
    }
    case 'autoSelect':
      return {
        ...state,
        present: { ...state.present, kit: { ...state.present.kit, autoSelect: action.value } },
      };
    case 'mute': {
      const present = mapPads(state.present, (pad) =>
        pad.id === action.id ? { ...pad, mute: !pad.mute } : pad,
      );
      return withPresent(state, present, true);
    }
    case 'solo': {
      const present = mapPads(state.present, (pad) =>
        pad.id === action.id ? { ...pad, solo: !pad.solo } : pad,
      );
      return withPresent(state, present, true);
    }
    case 'sensitivity': {
      const value = clamp(action.value, 0, 1);
      if (Math.abs(value - state.present.kit.sensitivity) < 0.0001) return state;
      const { slices } = detectSlices(
        state.present.sample.left,
        state.present.sample.right,
        state.present.sample.sampleRate,
        value,
      );
      const pads = remapStartsAfterReslice(state.present.kit.pads, state.present.slices, slices);
      const present: SessionData = {
        ...state.present,
        slices,
        kit: { ...state.present.kit, sensitivity: value, pads },
      };
      return withPresent(state, present, Boolean(action.history));
    }
    case 'addSlice': {
      const time = Math.max(0, action.time);
      if (state.present.slices.some((slice) => Math.abs(slice.time - time) < 0.004)) return state;
      const slices = [...state.present.slices, { time, strength: 0.6 }].sort((a, b) => a.time - b.time);
      const present: SessionData = { ...state.present, slices };
      return withPresent(state, present, true);
    }
    case 'deleteSlice': {
      if (state.present.slices.length === 0) return state;
      let nearest = 0;
      let best = Infinity;
      state.present.slices.forEach((slice, index) => {
        const distance = Math.abs(slice.time - action.time);
        if (distance < best) {
          best = distance;
          nearest = index;
        }
      });
      const removed = state.present.slices[nearest];
      if (!removed) return state;
      const slices = state.present.slices.filter((_, index) => index !== nearest);
      const pads = state.present.kit.pads.map((pad) => {
        const nearestSlice = slices.reduce(
          (result, slice, index) =>
            Math.abs(slice.time - pad.sampleStart) < Math.abs(slices[result]?.time - pad.sampleStart)
              ? index
              : result,
          0,
        );
        const slice = slices[nearestSlice];
        const matches = slice && Math.abs(slice.time - pad.sampleStart) < 0.004;
        return { ...pad, sliceIndex: matches ? nearestSlice : -1 };
      });
      const present: SessionData = {
        ...state.present,
        slices,
        kit: { ...state.present.kit, pads },
      };
      return withPresent(state, present, true);
    }
    case 'loadSession':
      return withPresent(state, action.session, action.history !== false);
    case 'reslice': {
      const present = {
        ...state.present,
        slices: action.slices,
      };
      return withPresent(state, present, Boolean(action.history));
    }
    case 'randomize': {
      const seed = Date.now();
      const pads = randomizePads(state.present.kit.pads, seed);
      const global = randomizeGlobal(state.present.kit.global, seed + 1);
      const present: SessionData = {
        ...state.present,
        kit: {
          ...state.present.kit,
          pads,
          global,
          presetName: 'RANDOM KIT',
        },
      };
      return withPresent(state, present, true);
    }
    case 'resetPad': {
      const base = makePad(action.id);
      const present = mapPads(state.present, (pad) =>
        pad.id === action.id
          ? { ...base, sampleStart: pad.sampleStart, sliceIndex: pad.sliceIndex, note: pad.note }
          : pad,
      );
      return withPresent(state, present, true);
    }
    case 'rename':
      return {
        ...state,
        present: {
          ...state.present,
          kit: { ...state.present.kit, presetName: action.name.slice(0, 40) },
        },
      };
    case 'setKit':
      return withPresent(
        state,
        {
          ...state.present,
          kit: action.kit,
        },
        Boolean(action.history),
      );
    default:
      return state;
  }
}

export interface KitActions {
  beginGesture: () => void;
  undo: () => void;
  redo: () => void;
  canUndo: boolean;
  canRedo: boolean;
  updatePad: (id: number, patch: Partial<PadParams>) => void;
  setPadStart: (id: number, time: number) => void;
  setGlobal: (patch: Partial<GlobalParams>) => void;
  selectPad: (id: number) => void;
  setAutoSelect: (value: boolean) => void;
  toggleMute: (id: number) => void;
  toggleSolo: (id: number) => void;
  setSensitivity: (value: number) => void;
  addSlice: (time: number) => void;
  deleteSlice: (time: number) => void;
  loadPreset: (id: string) => void;
  loadLibrarySource: (sourceId: string) => void;
  loadUserSample: (sample: SourceSample) => void;
  loadSavedKit: (kit: KitState, providedSample?: SourceSample) => boolean;
  randomize: () => void;
  resetPad: (id: number) => void;
  renameKit: (name: string) => void;
  setKit: (kit: KitState) => void;
}

export function useKit() {
  const [state, dispatch] = useReducer(reducer, undefined, (): HistoryState => ({
    present: buildInitialSession(),
    past: [],
    future: [],
  }));
  const presentRef = useRef(state.present);
  presentRef.current = state.present;

  const beginGesture = useCallback(() => dispatch({ type: 'push' }), []);
  const updatePad = useCallback((id: number, patch: Partial<PadParams>) => {
    dispatch({ type: 'pad', id, patch });
  }, []);
  const setPadStart = useCallback((id: number, time: number) => {
    dispatch({ type: 'padStart', id, time });
  }, []);
  const setGlobal = useCallback((patch: Partial<GlobalParams>) => {
    dispatch({ type: 'global', patch });
  }, []);
  const selectPad = useCallback((id: number) => dispatch({ type: 'select', id }), []);
  const setAutoSelect = useCallback((value: boolean) => dispatch({ type: 'autoSelect', value }), []);
  const toggleMute = useCallback((id: number) => dispatch({ type: 'mute', id }), []);
  const toggleSolo = useCallback((id: number) => dispatch({ type: 'solo', id }), []);
  const setSensitivity = useCallback((value: number) => {
    dispatch({ type: 'sensitivity', value });
  }, []);
  const addSlice = useCallback((time: number) => dispatch({ type: 'addSlice', time }), []);
  const deleteSlice = useCallback((time: number) => dispatch({ type: 'deleteSlice', time }), []);
  const undo = useCallback(() => dispatch({ type: 'undo' }), []);
  const redo = useCallback(() => dispatch({ type: 'redo' }), []);
  const randomize = useCallback(() => dispatch({ type: 'randomize' }), []);
  const resetPad = useCallback((id: number) => dispatch({ type: 'resetPad', id }), []);
  const renameKit = useCallback((name: string) => dispatch({ type: 'rename', name }), []);
  const setKit = useCallback((kit: KitState) => dispatch({ type: 'setKit', kit }), []);

  const loadPreset = useCallback((id: string) => {
    try {
      const session = buildPresetSession(id);
      dispatch({ type: 'loadSession', session });
    } catch (error) {
      console.error('[SliceForge] could not build preset', id, error);
    }
  }, []);

  const loadLibrarySource = useCallback((sourceId: string) => {
    try {
      const sample = generateLibrarySample(sourceId as never);
      const previous = presentRef.current;
      const session = buildSourceSession(sourceId, sample.name, sample, previous.kit, previous.kit.sensitivity);
      dispatch({ type: 'loadSession', session });
    } catch (error) {
      console.error('[SliceForge] could not load source', sourceId, error);
    }
  }, []);

  const loadUserSample = useCallback((sample: SourceSample) => {
    const previous = presentRef.current;
    const session = buildSourceSession(sample.id, sample.name, sample, previous.kit, previous.kit.sensitivity);
    dispatch({ type: 'loadSession', session });
  }, []);

  const loadSavedKit = useCallback((kit: KitState, providedSample?: SourceSample): boolean => {
    if (!kit || !Array.isArray(kit.pads) || kit.pads.length !== PAD_COUNT) return false;
    const current = presentRef.current;
    let sample: SourceSample | null = null;
    if (providedSample && providedSample.id === kit.sourceId) {
      sample = providedSample;
    } else if (current.sample.id === kit.sourceId) {
      sample = current.sample;
    } else if (FACTORY_PRESETS.some((preset) => preset.sourceId === kit.sourceId)) {
      sample = generateLibrarySample(kit.sourceId as never);
    }
    if (!sample) return false;
    const sensitivity = clamp(kit.sensitivity ?? 0.55, 0, 1);
    const { slices } = detectSlices(sample.left, sample.right, sample.sampleRate, sensitivity);
    const pads = kit.pads.map((pad, index) => ({
      ...makePad(index),
      ...pad,
      id: index,
      note: PAD_NOTES[index],
    }));
    const session: SessionData = {
      kit: {
        ...kit,
        version: 1,
        sensitivity,
        sourceName: sample.name,
        pads,
        selectedPad: clamp(kit.selectedPad ?? 0, 0, PAD_COUNT - 1),
        global: { ...current.kit.global, ...kit.global },
      },
      sample,
      slices,
    };
    dispatch({ type: 'loadSession', session });
    return true;
  }, []);

  const actions = useMemo<KitActions>(
    () => ({
      beginGesture,
      undo,
      redo,
      canUndo: state.past.length > 0,
      canRedo: state.future.length > 0,
      updatePad,
      setPadStart,
      setGlobal,
      selectPad,
      setAutoSelect,
      toggleMute,
      toggleSolo,
      setSensitivity,
      addSlice,
      deleteSlice,
      loadPreset,
      loadLibrarySource,
      loadUserSample,
      loadSavedKit,
      randomize,
      resetPad,
      renameKit,
      setKit,
    }),
    [
      beginGesture,
      undo,
      redo,
      state.past.length,
      state.future.length,
      updatePad,
      setPadStart,
      setGlobal,
      selectPad,
      setAutoSelect,
      toggleMute,
      toggleSolo,
      setSensitivity,
      addSlice,
      deleteSlice,
      loadPreset,
      loadLibrarySource,
      loadUserSample,
      loadSavedKit,
      randomize,
      resetPad,
      renameKit,
      setKit,
    ],
  );

  const selectedPad = state.present.kit.pads[state.present.kit.selectedPad] ?? createDefaultPads()[0];

  return {
    session: state.present,
    kit: state.present.kit,
    sample: state.present.sample,
    slices: state.present.slices,
    selectedPad,
    actions,
  };
}
