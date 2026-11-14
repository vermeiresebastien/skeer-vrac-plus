#pragma once

#include <JuceHeader.h>
#include "../Engine/PreviewPlayer.h"
#include "../PluginProcessor.h"
#include "../VaultLibrary.h"

#include <atomic>
#include <memory>

namespace sliceforge::ui
{
    class VaultPanel final : public juce::Component,
                             private juce::Timer,
                             private juce::ListBoxModel
    {
    public:
        explicit VaultPanel (SliceForgeAudioProcessor&);
        ~VaultPanel() override;

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
            void setProgress (double playhead);
            void paint (juce::Graphics&) override;

        private:
            std::vector<float> minimum, maximum;
            double head = -1.0;
        };

        void timerCallback() override;
        void rebuildResults();
        void rebuildCollections();
        void updateDetails();
        void loadSelected();
        void locateDatabase();
        void revealSelectedFolder();
        void startSearchTimer();
        juce::Rectangle<int> detailRect() const;
        void beginPreview();
        void restartPreview();
        void abandonPreview();
        void finishPreview (unsigned generation, sliceforge::SamplePtr decoded);
        void pollPreview();
        void stopPreview (const juce::String& idleStatus = {});
        void paintListBoxItem (int rowNumber, juce::Graphics&, int width, int height, bool rowIsSelected) override;
        int getNumRows() override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
        void selectedRowsChanged (int lastRowSelected) override;
        VaultTrack* selectedTrack();
        juce::String formatDuration (double milliseconds) const;
        juce::String formatKey (const VaultTrack&) const;

        SliceForgeAudioProcessor& processorRef;
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
        juce::TextButton previewButton { "PLAY" };
        juce::TextButton loopButton { "LOOP" };
        juce::TextButton stopButton { "STOP" };
        juce::ListBox list { "vault-tracks", this };
        WaveformView waveform;
        std::unique_ptr<juce::FileChooser> fileChooser;

        // Decoding a vault file touches disk, so it runs on one worker thread and hands the
        // result back by generation number: only the most recent click is ever allowed to
        // start audio, however fast the list is driven.
        juce::ThreadPool previewPool { 1 };
        std::shared_ptr<std::atomic<unsigned>> previewGeneration { std::make_shared<std::atomic<unsigned>> (0) };
        sliceforge::SamplePtr previewSample;
        juce::TimedCallback searchTimer;
        int64_t previewedTrackId = 0;
        bool previewWasPlaying = false;
        bool previewSuppressed = false;
        uint64_t previewStamp = 0;
        int previewStallPolls = 0;
        juce::String previewStatus;

        std::vector<VaultTrack> tracks;
        VaultTrack details;
        int selectedRow = -1;
        bool active = false;
        bool updating = false;
        juce::String statusLine;
        juce::String statusDetail;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VaultPanel)
    };
} // namespace sliceforge::ui
