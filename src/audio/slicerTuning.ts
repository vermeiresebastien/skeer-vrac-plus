/*  The auto-slicer's tuning, stated once per edition — the mirror of
 *  `native/Source/Engine/SlicerTuning.h`. See that file for why the six numbers live in one place
 *  each; `src/lib/__tests__/slicerTuning.test.ts` is the gate that keeps the two lists honest, and
 *  it reads the C++ header as text, so a native-only edit reddens `npm test`.
 *
 *  MAX_SLICES in `../types` stays where it is: it is a product ceiling on the kit, not a tuning. */

/** Slice count at sensitivity 0: `TARGET_BASE + TARGET_SPAN * sens`, rounded. */
export const TARGET_BASE = 8;
export const TARGET_SPAN = 46;

/** Minimum spacing between two accepted onsets, in seconds. It keeps the ghost echoes of one hit
 *  from being read as the next one, and it is a time rather than a hop count so the same take
 *  slices identically at 44.1 and 96 kHz. */
export const MIN_GAP_SECONDS = 0.115;

/** A rise must reach this fraction of the loudest rise in the file to be a candidate at all. */
export const FLOOR_RATIO = 0.012;

/** How far the reported onset is pulled back from the energy peak, so the attack survives. */
export const ONSET_BIAS_SECONDS = 0.006;

/** After the bias, two accepted onsets closer than this collapse to the first one. */
export const DEDUP_GATE_SECONDS = 0.07;
