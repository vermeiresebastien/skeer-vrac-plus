#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"

namespace vrac::ui
{
    class FooterBar final : public juce::Component
    {
    public:
        explicit FooterBar (VRACPlusAudioProcessor&);
        void paint (juce::Graphics&) override;
    private:
        VRACPlusAudioProcessor& processorRef;
    };
} // namespace vrac::ui

