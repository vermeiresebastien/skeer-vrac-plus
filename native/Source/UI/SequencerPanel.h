#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../Sequencer.h"
#include "SequencerGrid.h"

#include <optional>

namespace vrac::ui
{
    class SequencerPanel final : public juce::Component,
                                 private juce::Timer
    {
    public:
        explicit SequencerPanel (VRACPlusAudioProcessor&);
        ~SequencerPanel() override { stopTimer(); }

        void refreshFromProcessor();
        void refreshPlayhead();
        void setActive (bool shouldBeActive);
        void togglePlayback();
        void randomizeShortcut();
        void refreshTheme();
        void showStepInspectorForTesting (int pad, int step);

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void timerCallback() override;
        void updateControls();
        void updateTransportButton();
        void pushUndo();
        void commit (std::shared_ptr<vrac::SequencerData> newData, bool refreshControls);
        void edit (const std::function<void (vrac::SequencerData&)>& mutation, bool undo);
        void setActivePattern (int pattern);
        void setEditMode (SequencerGrid::EditMode mode);
        void showRandomMenu();
        void applyRandomChoice (int choice);
        void randomizeLane (int pad, float density);
        void euclidLane (int pulses);
        void copyCurrentPattern();
        void pasteCurrentPattern();
        void clearCurrentPattern();
        void openStepInspector (int pad, int step, juce::Point<int> screenPosition);
        void showPatternPresetMenu();
        void applyPatternPreset (int presetIndex);
        void savePatternPresetFile();
        void loadPatternPresetFile();
        void savePatternToFile (const juce::File& file);
        void loadPatternFromFile (const juce::File& file);
        void showMidiMenu();
        void exportMidiFile();
        void dragMidiFile();
        void writeMidiToFile (const juce::File& file);
        void setSongMode (bool enabled);
        void cycleChainSlot (int slot);
        void setLaneLength (int length);
        int laneLengthForSelected() const;

        VRACPlusAudioProcessor& processorRef;
        std::shared_ptr<const vrac::SequencerData> data;
        SequencerGrid grid;
        std::array<juce::TextButton, vrac::kSeqPatterns> patternButtons;
        juce::TextButton playButton { "PLAY" };
        juce::TextButton hostButton { "HOST" };
        juce::Slider bpmSlider;
        juce::Slider swingSlider;
        juce::Slider lengthSlider;
        juce::ComboBox divisionBox;
        std::array<juce::TextButton, 6> modeButtons;
        juce::TextButton randomButton { "RANDOM" };
        juce::TextButton clearButton { "CLEAR" };
        juce::TextButton copyButton { "COPY" };
        juce::TextButton pasteButton { "PASTE" };
        juce::TextButton laneRandomButton { "LANE RND" };
        juce::TextButton euclidButton { "EUCLID" };
        juce::TextButton songButton { "SONG" };
        juce::TextButton patternPresetButton { "PAT PRESET" };
        juce::TextButton midiButton { "MIDI" };
        juce::ComboBox laneLengthBox;
        std::array<juce::TextButton, vrac::kSeqChainLength> chainButtons;
        std::optional<vrac::SeqPattern> patternClipboard;
        std::unique_ptr<juce::FileChooser> fileChooser;
        int selectedLane = 0;
        SequencerGrid::EditMode editMode = SequencerGrid::EditMode::Draw;
        bool updatingControls = false;
    };
} // namespace vrac::ui
