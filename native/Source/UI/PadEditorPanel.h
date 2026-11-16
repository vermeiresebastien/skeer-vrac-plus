#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "WaveformDisplay.h"
#include "Controls.h"

namespace sliceforge::ui
{
    class PadEditorPanel final : public juce::Component
    {
    public:
        explicit PadEditorPanel (SliceForgeAudioProcessor&);
        void paint (juce::Graphics&) override;
        void resized() override;

        void setSelectedPad (int pad);
        void refreshFromProcessor();
        int getSelectedPad() const noexcept { return selectedPad; }
        std::function<void (int)> onSelectedPadChanged;

        // For the layout test: these three rows are the controls a smaller window is most likely
        // to push out, and each only counts if it is actually on screen.
        int visibleModelButtonsForTesting() const;
        int visibleBusButtonsForTesting() const;
        int visibleChokeButtonsForTesting() const;

    private:
        void updateParameterIds();
        void updateButtonStates();
        void setPadParam (const juce::String& suffix, float value);
        void setModel (int modelIndex);
        void audition();
        void copyPad();
        void pastePad();
        void resetPad();

        SliceForgeAudioProcessor& processorRef;
        int selectedPad = 0;
        WaveformDisplay waveform;
        ParamKnob originalKnob;
        ParamKnob drumifyKnob;
        ParamKnob drumifiedKnob;
        std::array<std::unique_ptr<ParamKnob>, 6> controlKnobs;
        std::array<std::unique_ptr<ParamKnob>, 4> sculptKnobs;
        std::array<std::unique_ptr<ParamKnob>, 3> eqKnobs;
        juce::TextButton auditionButton { "PLAY" };
        juce::TextButton copyButton { "COPY" };
        juce::TextButton pasteButton { "PASTE" };
        juce::TextButton resetButton { "RESET" };
        std::array<juce::TextButton, 5> chokeButtons;
        std::array<juce::TextButton, 8> busButtons;
        std::array<juce::TextButton, 16> modelButtons;
        PadSettings copiedPad;
        bool hasCopiedPad = false;
    };
} // namespace sliceforge::ui

