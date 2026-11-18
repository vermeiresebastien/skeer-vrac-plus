/*
 * One place states how much audio an import may hold. Two doors fill the kit source — the recorded
 * take (components/RecordModal.tsx) and the decoded file (lib/audioFiles.ts) — and before this file
 * they were four unrelated literals spread over both, so "Up to 2 minutes" in the footer, `elapsed >= 120`
 * in the timer and `10 * 60` in the decoder could each drift on their own.
 *
 * The numbers mirror the native edition's Engine/ImportLimits.h, which is what makes a change here
 * a change there too: the web edition is the browser app, the native one is the plug-in, and a kit
 * built from a 5-minute take has to be sliceable in both.
 */

/** Seconds a recorded take may hold — five minutes. */
export const MAX_TAKE_SECONDS = 300;

/** Seconds a decoded file may hold — ten minutes. */
export const MAX_FILE_SECONDS = 600;

/** Encoded bytes accepted before decoding starts, so a huge file is refused rather than attempted. */
export const MAX_FILE_BYTES = 160 * 1024 * 1024;

/** Shortest file worth slicing: below this there is no attack to place a slice on. */
export const MIN_FILE_SECONDS = 0.05;

/** How the take ceiling reads in the UI, so the copy cannot outlive the constant it describes. */
export const takeLimitLabel = `Up to ${MAX_TAKE_SECONDS / 60} minutes`;

/** The one rule the record deck stops on. Stated as a function so the boundary has a test: the
 * ceiling used to be a bare `elapsed >= 120` inside a setInterval, where nothing could reach it. */
export function takeIsFull(seconds: number): boolean {
  return seconds >= MAX_TAKE_SECONDS;
}
