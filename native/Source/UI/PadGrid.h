#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"

namespace vrac::ui
{
    class PadGrid final : public juce::Component
    {
    public:
        explicit PadGrid (VRACPlusAudioProcessor& processor);

        void setSelectedPad (int pad);
        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;

        std::function<void (int, float)> onPadTriggered;
        std::function<void (int)> onPadSelected;
        std::function<void (int)> onMuteClicked;
        std::function<void (int)> onSoloClicked;

    private:
        VRACPlusAudioProcessor& processorRef;
        int selected = 0;
        std::array<juce::Rectangle<float>, vrac::kNumPads> padRects;
        std::array<juce::Rectangle<float>, vrac::kNumPads> muteRects;
        std::array<juce::Rectangle<float>, vrac::kNumPads> soloRects;
        void updateRects();
    };
} // namespace vrac::ui

