#pragma once

#include "Params.h"
#include <array>
#include <limits>

namespace vrac
{
    struct PadOverride
    {
        int modelIndex = -1;
        float drumify = std::numeric_limits<float>::quiet_NaN();
        float length = std::numeric_limits<float>::quiet_NaN();
        float pitch = std::numeric_limits<float>::quiet_NaN();
        float pan = std::numeric_limits<float>::quiet_NaN();
        int choke = -1;
        float drumifiedGain = std::numeric_limits<float>::quiet_NaN();
        float originalGain = std::numeric_limits<float>::quiet_NaN();
    };

    struct FactoryPreset
    {
        juce::String name;
        juce::String tag;
        juce::String sourceId;
        float sensitivity = 0.55f;
        int selectedPad = 3;
        GlobalSettings global;
        std::array<PadOverride, kNumPads> pads {};
    };

    const std::array<FactoryPreset, 6>& factoryPresets();
} // namespace vrac
