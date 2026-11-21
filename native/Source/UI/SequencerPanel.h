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
        ~SequencerPanel() override
        {
            stopTimer();
            // ~MouseListener does not unregister itself in JUCE 8, and chipPopups is declared
            // after patternButtons, so it is destroyed first and would leave dangling listener
            // pointers in buttons that are still alive for a few more destructors.
            for (int i = 0; i < sliceforge::kSeqPatterns; ++i)
                patternButtons[(size_t) i].removeMouseListener (chipPopups[(size_t) i].get());
        }

        void refreshFromProcessor();
        void refreshPlayhead();
        void setActive (bool shouldBeActive);
        void togglePlayback();
        void randomizeShortcut();
        void refreshTheme();
        void showStepInspectorForTesting (int pad, int step);
        // The layout test measures the duplicate control directly: a button that exists but never
        // reaches the screen is the failure it is there to catch.
        const juce::TextButton& copyToControlForTesting() const { return copyToButton; }
        const juce::TextButton& grooveControlForTesting() const { return grooveButton; }
        /*  The GROOVES menu is the only road from the bank to the document, so the layout test drives
            these three handlers and reads the result back off the processor: a groove that builds a
            pattern but never reaches setSequencerData would play for nobody, and MATCH SOUNDS moves
            the player's own sounds, so what it must leave alone matters as much as what it changes. */
        void applyGrooveForTesting (int grooveIndex) { applyGrooveChoice (grooveIndex); }
        void cycleGrooveForTesting() { cycleGrooveVariation(); }
        void matchGrooveSoundsForTesting (int grooveIndex) { matchGrooveSounds (grooveIndex); }
        int lastGrooveForTesting() const { return lastGrooveIndex; }

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
        void showGrooveMenu();
        void applyGrooveChoice (int grooveIndex);
        void cycleGrooveVariation();
        void matchGrooveSounds (int grooveIndex);
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
            the source, so copying *from* any pattern needs no separate picker.
            JUCE 8's Button::mouseUp fires onClick without checking the popup modifier, so the
            right-click that opened this menu can also arrive at the button as an ordinary click and
            silently switch the pattern you are hearing. mouseDown records which chip is expecting
            to be clicked through, and the menu's completion callback clears it whichever way the
            menu closes - chosen, dismissed, or never released. */
        struct ChipPopupMenu final : public juce::MouseListener
        {
            ChipPopupMenu (int patternIndex, SequencerPanel* owner)
                : index (patternIndex), panel (owner) {}

            void mouseDown (const juce::MouseEvent& event) override
            {
                if (! event.mods.isPopupMenu ())
                    return;
                panel->chipClickToIgnore = index;
                panel->showPatternCopyMenu (index, event.eventComponent);
            }

            int index;
            SequencerPanel* panel;
        };

        /** Index of the chip whose onClick must be swallowed because a right-click, not a
            selection, opened the copy menu. -1 when nothing is pending. */
        int chipClickToIgnore = -1;

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
        juce::TextButton grooveButton { "GROOVES" };
        juce::TextButton midiButton { "MIDI" };
        juce::ComboBox laneLengthBox;
        std::array<juce::TextButton, sliceforge::kSeqChainLength> chainButtons;
        std::optional<sliceforge::SeqPattern> patternClipboard;
        /*  Which groove the last menu pick applied, and which genre's models MATCH SOUNDS last put on
            the kit - empty when it never has. The genre, not an index, because all three variations
            of a genre share its voicing, so cycling within a genre must leave the tick where it is.
            Panel-local, like the clipboard: a groove is a starting point the player edits away from,
            so storing it in SequencerData would make a saved set claim to hold a rhythm it no longer
            contains. */
        int lastGrooveIndex = -1;
        juce::String lastVoicedGenre;
        std::array<std::unique_ptr<ChipPopupMenu>, sliceforge::kSeqPatterns> chipPopups;
        std::unique_ptr<juce::FileChooser> fileChooser;
        int selectedLane = 0;
        // The pattern the chips and the grid were last built for, so the timer can tell that a
        // host moved seq.pattern by itself. Nothing else in the panel is told.
        int patternAtLastRefresh = 0;
        SequencerGrid::EditMode editMode = SequencerGrid::EditMode::Draw;
        bool updatingControls = false;
    };
} // namespace sliceforge::ui
