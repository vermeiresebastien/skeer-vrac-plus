/* Local persistence: user presets, MIDI mappings and preferences. */

import type { KitState } from '../types';

const PRESET_KEY = 'vrac-plus.presets.v1';
const THEME_KEY = 'vrac-plus.theme.v1';
const MIDI_KEY = 'vrac-plus.midi.v1';

export interface SavedKit {
  id: string;
  name: string;
  createdAt: number;
  kit: KitState;
}

function safeParse<T>(raw: string | null, fallback: T): T {
  if (!raw) return fallback;
  try {
    return JSON.parse(raw) as T;
  } catch {
    return fallback;
  }
}

export function loadSavedKits(): SavedKit[] {
  if (typeof localStorage === 'undefined') return [];
  const parsed = safeParse<SavedKit[]>(localStorage.getItem(PRESET_KEY), []);
  if (!Array.isArray(parsed)) return [];
  return parsed.filter((item) => item && item.kit && item.id && item.name);
}

/** What a write actually produced: the list the UI should show, and whether the browser really
 *  kept it. `persisted: false` means the bytes never left the process, so nothing may claim the
 *  kit is saved. */
export interface KitWriteResult {
  kits: SavedKit[];
  persisted: boolean;
}

/** localStorage throws on quota, refuses writes in some private modes and can be poisoned by a
 *  corrupt origin, so the list is read back through the same loader the UI uses instead of
 *  trusting that setItem returned. */
function writeSavedKits(kits: SavedKit[]): boolean {
  if (typeof localStorage === 'undefined') return false;
  try {
    localStorage.setItem(PRESET_KEY, JSON.stringify(kits));
    return loadSavedKits().length === kits.length;
  } catch (error) {
    console.warn('[SliceForge] could not save presets', error);
    return false;
  }
}

export function saveUserKit(name: string, kit: KitState): KitWriteResult {
  const kits = loadSavedKits();
  const trimmed = name.trim() || 'MY KIT';
  const existing = kits.findIndex((item) => item.name.toLowerCase() === trimmed.toLowerCase());
  const saved: SavedKit = {
    id: existing >= 0 ? kits[existing].id : `kit-${Date.now()}`,
    name: trimmed,
    createdAt: Date.now(),
    kit: { ...kit, presetName: trimmed },
  };
  if (existing >= 0) kits[existing] = saved;
  else kits.unshift(saved);
  return { kits, persisted: writeSavedKits(kits) };
}

export function deleteUserKit(id: string): KitWriteResult {
  const kits = loadSavedKits().filter((item) => item.id !== id);
  return { kits, persisted: writeSavedKits(kits) };
}

export function getThemePreference(): 'light' | 'dark' {
  if (typeof localStorage === 'undefined') return 'light';
  const stored = localStorage.getItem(THEME_KEY);
  if (stored === 'light' || stored === 'dark') return stored;
  return window.matchMedia?.('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
}

export function setThemePreference(theme: 'light' | 'dark'): void {
  try {
    localStorage.setItem(THEME_KEY, theme);
  } catch {
    /* ignore */
  }
}

export interface MidiMapping {
  target: string;
  cc: number;
}

export function loadMidiMappings(): MidiMapping[] {
  if (typeof localStorage === 'undefined') return [];
  const parsed = safeParse<MidiMapping[]>(localStorage.getItem(MIDI_KEY), []);
  return Array.isArray(parsed) ? parsed.filter((m) => m && typeof m.target === 'string' && Number.isInteger(m.cc)) : [];
}

export function writeMidiMappings(mappings: MidiMapping[]): void {
  try {
    localStorage.setItem(MIDI_KEY, JSON.stringify(mappings));
  } catch {
    /* ignore */
  }
}

export function downloadBlob(blob: Blob, filename: string): void {
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 2000);
}

export function slugify(value: string): string {
  return (
    value
      .toLowerCase()
      .replace(/[^a-z0-9]+/g, '-')
      .replace(/^-+|-+$/g, '') || 'kit'
  );
}
