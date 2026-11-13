#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "PadGrid.h"

namespace vrac::ui
{
    class PadsPanel final : public juce::Component
    {
    public:
        explicit PadsPanel (VRACPlusAudioProcessor&);
        void paint (juce::Graphics&) override;
        void resized() override;

        PadGrid& getPadGrid() { return padGrid; }
        void setAutoSelect (bool shouldAutoSelect);
        bool isAutoSelect() const noexcept { return autoSelect; }
        std::function<void (bool)> onAutoSelectChanged;

    private:
        VRACPlusAudioProcessor& processorRef;
        PadGrid padGrid;
        juce::TextButton autoSelectButton { "AUTO SELECT" };
        juce::TextButton stopButton { "STOP" };
        bool autoSelect = true;
    };
} // namespace vrac::ui

