#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"

namespace sliceforge::ui
{
    class FooterBar final : public juce::Component
    {
    public:
        explicit FooterBar (SliceForgeAudioProcessor&);
        void paint (juce::Graphics&) override;
    private:
        SliceForgeAudioProcessor& processorRef;
    };
} // namespace sliceforge::ui

