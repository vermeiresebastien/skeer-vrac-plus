#pragma once

#include "SampleData.h"
#include "Params.h"
#include <array>
#include <vector>

namespace vrac
{
    struct Slice
    {
        double time = 0.0;
        float strength = 0.5f;
    };

    std::vector<Slice> detectSlices (const SampleData& sample, float sensitivity);
    std::vector<double> padStartsFromSlices (const std::vector<Slice>& slices);

    struct SliceMapping
    {
        std::vector<Slice> slices;
        std::array<double, kNumPads> padStarts {};
        std::array<int, kNumPads> padSliceIndices {};
    };
} // namespace vrac
