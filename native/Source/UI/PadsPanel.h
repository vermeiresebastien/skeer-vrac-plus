#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "PadGrid.h"

namespace sliceforge::ui
{
    class PadsPanel final : public juce::Component
    {
    public:
        explicit PadsPanel (SliceForgeAudioProcessor&);
        void paint (juce::Graphics&) override;
        void resized() override;

        PadGrid& getPadGrid() { return padGrid; }
        void setAutoSelect (bool shouldAutoSelect);
        bool isAutoSelect() const noexcept { return autoSelect; }
        std::function<void (bool)> onAutoSelectChanged;

    private:
        SliceForgeAudioProcessor& processorRef;
        PadGrid padGrid;
        juce::TextButton autoSelectButton { "AUTO SELECT" };
        juce::TextButton stopButton { "STOP" };
        bool autoSelect = true;
    };
} // namespace sliceforge::ui

