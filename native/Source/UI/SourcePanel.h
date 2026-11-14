#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "WaveformDisplay.h"
#include "Controls.h"

namespace sliceforge::ui
{
    class SourcePanel final : public juce::Component
    {
    public:
        explicit SourcePanel (SliceForgeAudioProcessor&);
        void paint (juce::Graphics&) override;
        void resized() override;

        void refreshFromProcessor();
        void setSelectedPad (int pad);
        int getSelectedPad() const noexcept { return selectedPad; }

        std::function<void()> onLoadFile;
        std::function<void()> onToggleRecording;
        std::function<void()> onSourceChanged;
        std::function<void (int)> onSliceNav; // -1 previous, +1 next, 0 random
        std::function<void (bool)> onAddDeleteSlice; // true add, false delete
        std::function<void()> onRecordReady;

    private:
        void showLibraryMenu();
        SliceForgeAudioProcessor& processorRef;
        WaveformDisplay waveform;
        juce::TextButton loadButton { "LOAD" };
        juce::TextButton libraryButton { "LIBRARY" };
        juce::TextButton recordButton { "REC" };
        juce::TextButton previousSlice { "<" };
        juce::TextButton nextSlice { ">" };
        juce::TextButton randomSlice { "RND" };
        juce::TextButton addSlice { "+" };
        juce::TextButton deleteSlice { "DEL" };
        ParamSlider sensitivity;
        std::array<std::unique_ptr<ParamKnob>, 10> globals;
        int selectedPad = 0;
    };
} // namespace sliceforge::ui

