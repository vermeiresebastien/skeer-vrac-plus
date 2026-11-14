#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../Sequencer.h"
#include "SequencerGrid.h"

#include <optional>

namespace sliceforge::ui
{
    class SequencerPanel final : public juce::Component,
                                 private juce::Timer
    {
    public:
        explicit SequencerPanel (SliceForgeAudioProcessor&);
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
        void commit (std::shared_ptr<sliceforge::SequencerData> newData, bool refreshControls);
        void edit (const std::function<void (sliceforge::SequencerData&)>& mutation, bool undo);
        void setActivePattern (int pattern);
        void setEditMode (SequencerGrid::EditMode mode);
        void showRandomMenu();
        void applyRandomChoice (int choice);
        void randomizeLane (int pad, float density);
        void euclidLane (int pulses);
        void copyCurrentPattern();
        void pasteCurrentPattern();
        void showPatternCopyMenu (int sourcePattern, juce::Component* targetComponent);
        void applyPatternCopy (int source, int target);
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

        /** Right-clicking an A/B/C/D chip opens the copy menu with that pattern already picked as
            the source, so copying *from* any pattern needs no separate picker. The listeners are
            owned by the panel and are removed with the buttons it is built from, so the raw
            back-pointer can never outlive its target. */
        struct ChipPopupMenu final : public juce::MouseListener
        {
            ChipPopupMenu (int patternIndex, SequencerPanel* owner)
                : index (patternIndex), panel (owner) {}

            void mouseDown (const juce::MouseEvent& event) override
            {
                if (event.mods.isPopupMenu ())
                    panel->showPatternCopyMenu (index, event.eventComponent);
            }

            int index;
            SequencerPanel* panel;
        };

        SliceForgeAudioProcessor& processorRef;
        std::shared_ptr<const sliceforge::SequencerData> data;
        SequencerGrid grid;
        std::array<juce::TextButton, sliceforge::kSeqPatterns> patternButtons;
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
        juce::TextButton copyToButton { "COPY TO" };
        juce::TextButton laneRandomButton { "LANE RND" };
        juce::TextButton euclidButton { "EUCLID" };
        juce::TextButton songButton { "SONG" };
        juce::TextButton patternPresetButton { "PAT PRESET" };
        juce::TextButton midiButton { "MIDI" };
        juce::ComboBox laneLengthBox;
        std::array<juce::TextButton, sliceforge::kSeqChainLength> chainButtons;
        std::optional<sliceforge::SeqPattern> patternClipboard;
        std::array<std::unique_ptr<ChipPopupMenu>, sliceforge::kSeqPatterns> chipPopups;
        std::unique_ptr<juce::FileChooser> fileChooser;
        int selectedLane = 0;
        SequencerGrid::EditMode editMode = SequencerGrid::EditMode::Draw;
        bool updatingControls = false;
    };
} // namespace sliceforge::ui
