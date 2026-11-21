#pragma once

#include "SampleData.h"
#include "Params.h"
#include <array>
#include <vector>

namespace sliceforge
{
    struct Slice
    {
        double time = 0.0;
        float strength = 0.5f;
    };

    struct SliceMapping
    {
        std::vector<Slice> slices;
        std::array<double, kNumPads> padStarts {};
        std::array<double, kNumPads> padLengths {};
        std::array<int, kNumPads> padSliceIndices {};
    };

    std::vector<Slice> detectSlices (const SampleData& sample, float sensitivity);
    std::vector<double> padStartsFromSlices (const std::vector<Slice>& slices);

    /** Seconds from one slice's onset to the next, and to the end of the material for the last
        slice. The single statement of where a slice ends: the pad map reads it and the plugin reads
        it when it writes LENGTH, so the derivation cannot change in one gate and not the other. */
    double sliceGapSeconds (const std::vector<Slice>& slices, int index, double durationSeconds);

    /*  One pad's start and its length are the same decision: both come from the slice the pad was
        given. Splitting them across two functions is how a pad ends up playing past the onset that
        follows it, so `sliceMapFromSlices` answers both at once and `padStartsFromSlices` is the
        starts-only view of it for the paths that must not touch LENGTH (a factory preset carries its
        own per-pad lengths, and re-deriving them there would overwrite shipped settings).
        Lengths are seconds, and they come from `sliceGapSeconds`. */
    SliceMapping sliceMapFromSlices (const std::vector<Slice>& slices, double durationSeconds);
} // namespace sliceforge
