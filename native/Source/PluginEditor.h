#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/LookAndFeel.h"
#include "UI/HeaderBar.h"
#include "UI/SourcePanel.h"
#include "UI/PadsPanel.h"
#include "UI/PadEditorPanel.h"
#include "UI/FooterBar.h"
#include "UI/SequencerPanel.h"
#include "UI/VaultPanel.h"

class SliceForgeAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer,
                                           private juce::ChangeListener,
                                           private juce::FileDragAndDropTarget
{
public:
    explicit SliceForgeAudioProcessorEditor (SliceForgeAudioProcessor&);
    ~SliceForgeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    void showSequencerPage (bool shouldShow);
    void showVaultPage (bool shouldShow);
    void setDarkThemeForTesting (bool dark);

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refreshAll();
    void showPage (sliceforge::ui::Page page);
    void toggleTheme();
    void applyThemeChange();
    void repaintAll (juce::Component& component);
    void selectPad (int pad);
    void navigateSlice (int direction);
    void addOrDeleteSlice (bool add);
    void setBoolPadParam (int pad, const juce::String& suffix);
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void chooseSampleFile();
    void savePresetFile();
    void loadPresetFile();
    void showHelp();
    void showAbout();
    void updateAutoSelection();

    SliceForgeAudioProcessor& processorRef;
    sliceforge::ui::SliceForgeLookAndFeel lookAndFeel;
    std::unique_ptr<juce::Component> content;
    std::unique_ptr<sliceforge::ui::HeaderBar> header;
    std::unique_ptr<sliceforge::ui::SourcePanel> sourcePanel;
    std::unique_ptr<sliceforge::ui::PadsPanel> padsPanel;
    std::unique_ptr<sliceforge::ui::PadEditorPanel> padEditor;
    std::unique_ptr<sliceforge::ui::SequencerPanel> sequencerPanel;
    std::unique_ptr<sliceforge::ui::VaultPanel> vaultPanel;
    std::unique_ptr<sliceforge::ui::FooterBar> footer;
    std::unique_ptr<juce::FileChooser> fileChooser;
    sliceforge::ui::Page currentPage = sliceforge::ui::Page::kit;
    bool autoSelect = true;
    uint32_t lastTriggerStamp = 0;
    int uiTicks = 0;
    int statusTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SliceForgeAudioProcessorEditor)
};
