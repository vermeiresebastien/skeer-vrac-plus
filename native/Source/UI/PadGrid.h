#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"

namespace sliceforge::ui
{
    class PadGrid final : public juce::Component
    {
    public:
        explicit PadGrid (SliceForgeAudioProcessor& processor);

        void setSelectedPad (int pad);
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;

        std::function<void (int, float)> onPadTriggered;
        std::function<void (int)> onPadSelected;
        std::function<void (int)> onMuteClicked;
        std::function<void (int)> onSoloClicked;

    private:
        SliceForgeAudioProcessor& processorRef;
        int selected = 0;
        std::array<juce::Rectangle<float>, sliceforge::kNumPads> padRects;
        std::array<juce::Rectangle<float>, sliceforge::kNumPads> muteRects;
        std::array<juce::Rectangle<float>, sliceforge::kNumPads> soloRects;
        void updateRects();
    };
} // namespace sliceforge::ui

