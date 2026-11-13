/* IndexedDB storage for user samples, so locally saved kits that use a
 * dropped file can be restored after a reload. */

import { buildPeaks } from '../audio/sampleLibrary';
import type { SourceSample } from '../types';

const DB_NAME = 'vrac-plus';
const DB_VERSION = 1;
const STORE = 'samples';

interface StoredSample {
  id: string;
  name: string;
  blurb: string;
  sampleRate: number;
  left: Float32Array;
  right: Float32Array;
}

let dbPromise: Promise<IDBDatabase> | null = null;

function openDatabase(): Promise<IDBDatabase> {
  if (dbPromise) return dbPromise;
  dbPromise = new Promise((resolve, reject) => {
    const request = indexedDB.open(DB_NAME, DB_VERSION);
    request.onupgradeneeded = () => {
      const db = request.result;
      if (!db.objectStoreNames.contains(STORE)) db.createObjectStore(STORE, { keyPath: 'id' });
    };
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error ?? new Error('Could not open local sample store'));
  });
  return dbPromise;
}

export async function persistUserSample(sample: SourceSample): Promise<void> {
  if (!sample.isUser || typeof indexedDB === 'undefined') return;
  try {
    const db = await openDatabase();
    await new Promise<void>((resolve, reject) => {
      const tx = db.transaction(STORE, 'readwrite');
      tx.objectStore(STORE).put({
        id: sample.id,
        name: sample.name,
        blurb: sample.blurb,
        sampleRate: sample.sampleRate,
        left: sample.left,
        right: sample.right,
      } satisfies StoredSample);
      tx.oncomplete = () => resolve();
      tx.onerror = () => reject(tx.error ?? new Error('Could not save the sample'));
    });
  } catch (error) {
    console.warn('[VRAC+] could not persist user sample', error);
  }
}

export async function loadUserSample(id: string): Promise<SourceSample | null> {
  if (typeof indexedDB === 'undefined') return null;
  try {
    const db = await openDatabase();
    const stored = await new Promise<StoredSample | undefined>((resolve, reject) => {
      const tx = db.transaction(STORE, 'readonly');
      const request = tx.objectStore(STORE).get(id);
      request.onsuccess = () => resolve(request.result as StoredSample | undefined);
      request.onerror = () => reject(request.error ?? new Error('Could not read the sample'));
    });
    if (!stored || !(stored.left instanceof Float32Array) || !(stored.right instanceof Float32Array)) {
      return null;
    }
    return {
      id: stored.id,
      name: stored.name,
      blurb: stored.blurb || 'A saved user sample.',
      duration: stored.left.length / stored.sampleRate,
      sampleRate: stored.sampleRate,
      left: stored.left,
      right: stored.right,
      peaks: buildPeaks(stored.left, stored.right),
      isUser: true,
    };
  } catch (error) {
    console.warn('[VRAC+] could not restore user sample', error);
    return null;
  }
}
