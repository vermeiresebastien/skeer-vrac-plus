#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../VaultLibrary.h"

namespace vrac::ui
{
    class VaultPanel final : public juce::Component,
                             private juce::Timer,
                             private juce::ListBoxModel
    {
    public:
        explicit VaultPanel (VRACPlusAudioProcessor&);
        ~VaultPanel() override { stopTimer(); }

        void setActive (bool shouldBeActive);
        void refreshStatus();
        void refreshTheme();
        void selectFirstRowForTesting();
        void selectFirstAnalysedRowForTesting();
        void searchForTesting (const juce::String& text);

        void paint (juce::Graphics&) override;
        void resized() override;

        std::function<void()> onSampleLoaded;

    private:
        class WaveformView final : public juce::Component
        {
        public:
            void setPeaks (std::vector<float> newMin, std::vector<float> newMax);
            void paint (juce::Graphics&) override;

        private:
            std::vector<float> minimum, maximum;
        };

        void timerCallback() override;
        void rebuildResults();
        void rebuildCollections();
        void updateDetails();
        void loadSelected();
        void locateDatabase();
        void revealSelectedFolder();
        void startSearchTimer();
        void paintListBoxItem (int rowNumber, juce::Graphics&, int width, int height, bool rowIsSelected) override;
        int getNumRows() override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
        void selectedRowsChanged (int lastRowSelected) override;
        VaultTrack* selectedTrack();
        juce::String formatDuration (double milliseconds) const;
        juce::String formatKey (const VaultTrack&) const;

        VRACPlusAudioProcessor& processorRef;
        VaultLibrary vault;
        juce::TextEditor search;
        juce::ComboBox familyBox;
        juce::ComboBox typeBox;
        juce::ComboBox collectionBox;
        juce::ComboBox sortBox;
        juce::TextButton favoritesButton { "FAV" };
        juce::TextButton refreshButton { "REFRESH" };
        juce::TextButton locateButton { "LOCATE DB" };
        juce::TextButton loadButton { "LOAD SAMPLE" };
        juce::TextButton folderButton { "OPEN FOLDER" };
        juce::ListBox list { "vault-tracks", this };
        WaveformView waveform;
        std::unique_ptr<juce::FileChooser> fileChooser;

        std::vector<VaultTrack> tracks;
        VaultTrack details;
        int selectedRow = -1;
        bool active = false;
        bool updating = false;
        juce::String statusLine;
        juce::String statusDetail;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VaultPanel)
    };
} // namespace vrac::ui
