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
        /** Only the record button, its DISCARD companion and the status line. Called on a
            slow UI tick, where refreshing the waveform would fight a marker being dragged. */
        void refreshRecordingState();
        void setSelectedPad (int pad);
        int getSelectedPad() const noexcept { return selectedPad; }

        /** Test hooks for the record affordance: REC must not look armed when the host has not
            opened the sidechain input, and DISCARD must only exist while a take is waiting. */
        bool isRecordControlEnabledForTesting() const { return recordButton.isEnabled(); }
        bool isDiscardControlVisibleForTesting() const { return discardTakeButton.isVisible(); }
        juce::String getRecordControlTextForTesting() const { return recordButton.getButtonText(); }

        std::function<void()> onLoadFile;
        std::function<void()> onToggleRecording;
        std::function<void()> onDiscardRecording;
        std::function<void()> onSourceChanged;
        std::function<void (int)> onSliceNav; // -1 previous, +1 next, 0 random
        std::function<void (bool)> onAddDeleteSlice; // true add, false delete
        std::function<void()> onRecordReady;

    private:
        void showLibraryMenu();
        void applyRecordingState();
        SliceForgeAudioProcessor& processorRef;
        WaveformDisplay waveform;
        juce::TextButton loadButton { "LOAD" };
        juce::TextButton libraryButton { "LIBRARY" };
        juce::TextButton recordButton { "REC" };
        juce::TextButton discardTakeButton { "DISCARD" };
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

