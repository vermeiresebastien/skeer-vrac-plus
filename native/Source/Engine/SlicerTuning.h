#pragma once

/*  The auto-slicer's tuning, stated once per edition.
 *
 *  These six numbers are the whole personality of the detector: how many slices you get, how close
 *  two of them may sit, how weak a rise may be and still count, and where the reported onset is
 *  placed relative to the energy peak. The browser edition mirrors them in
 *  `src/audio/slicerTuning.ts`, and `src/lib/__tests__/slicerTuning.test.ts` parses THIS file and
 *  asserts the pairs match — so changing one here and not there reddens `npm test`. That is the same
 *  discipline `Engine/ImportLimits.h` enforces on the import ceilings, for the same reason: two
 *  implementations of one engine drift by a literal, not by an argument.
 *
 *  The slice-count ceiling is deliberately NOT here. `kMaxSlices` (`Params.h:13`) is a product
 *  ceiling on the kit, not a tuning of the detector, and moving it changes what a host sees. */
namespace sliceforge
{
    /** Slice count at sensitivity 0: `kTargetBase + kTargetSpan * sens`, rounded. */
    constexpr float kTargetBase = 8.0f;
    constexpr float kTargetSpan = 46.0f;

    /** Minimum spacing between two accepted onsets, in seconds. It keeps the ghost echoes of one
        hit from being read as the next one, and it is a time rather than a hop count on purpose, so
        the same take slices identically at 44.1 and 96 kHz. */
    constexpr double kMinGapSeconds = 0.115;

    /** A rise must reach this fraction of the loudest rise in the file to be a candidate at all.
        Being relative to the file's own peak is what lets one crash bury every hi-hat behind it. */
    constexpr float kFloorRatio = 0.012f;

    /** How far the reported onset is pulled *back* from the energy peak, so the attack survives.
        Stored positive and subtracted, which is what the detector has always done; the parity test
        reads unsigned literals, so a negative here would not parse. */
    constexpr double kOnsetBiasSeconds = 0.006;

    /** After the bias, two accepted onsets closer than this collapse to the first one - but as things
        stand it never binds, and a reader should not tune it expecting an effect. The selection pass
        already keeps every accepted onset at least `kMinGapSeconds` apart in hops, the bias is a
        constant so it cancels out of a difference, and `0.07 < 0.115`. Measured, not inferred: setting
        this to 0.0 moves no score on any fixture (see the Step 25 falsifier table). It only becomes
        reachable if it is raised above `kMinGapSeconds`, at which point it, not the min gap, decides
        how close two slices may sit. */
    constexpr double kDedupGateSeconds = 0.07;
} // namespace sliceforge
