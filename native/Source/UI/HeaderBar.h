#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"

namespace vrac::ui
{
    enum class Page
    {
        kit,
        sequencer,
        vault
    };

    class HeaderBar final : public juce::Component
    {
    public:
        explicit HeaderBar (VRACPlusAudioProcessor&);
        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh();
        void setPageActive (Page page);

        std::function<void()> onSavePreset;
        std::function<void()> onLoadPreset;
        std::function<void()> onHelp;
        std::function<void()> onAbout;
        std::function<void (Page)> onPageSelected;
        std::function<void()> onThemeToggle;

    private:
        VRACPlusAudioProcessor& processorRef;
        juce::Image skeerLogo;
        juce::Image skeerLogoDark;
        juce::ComboBox presetBox;
        juce::TextButton kitTabButton { "KIT" };
        juce::TextButton sequencerTabButton { "SEQUENCER" };
        juce::TextButton vaultTabButton { "VAULT" };
        juce::TextButton themeButton { "DARK" };
        juce::TextButton undoButton { "UNDO" };
        juce::TextButton redoButton { "REDO" };
        juce::TextButton randomButton { "RND" };
        juce::TextButton saveButton { "SAVE" };
        juce::TextButton loadButton { "LOAD" };
        juce::TextButton helpButton { "HELP" };
        juce::TextButton aboutButton { "INFO" };
    };
} // namespace vrac::ui

