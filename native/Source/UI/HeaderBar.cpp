#include "HeaderBar.h"
#include "Theme.h"
#include "Controls.h"

#include <BinaryData.h>

namespace vrac::ui
{
    namespace
    {
        juce::Image makeDarkWordmark (const juce::Image& source)
        {
            if (! source.isValid())
                return {};
            auto result = source.createCopy();
            juce::Image::BitmapData pixels (result, juce::Image::BitmapData::readWrite);
            for (int y = 0; y < source.getHeight(); ++y)
            {
                for (int x = 0; x < source.getWidth(); ++x)
                {
                    const auto colour = pixels.getPixelColour (x, y);
                    if (colour.getAlpha() > 0 && colour.getBrightness() < 0.38f
                        && colour.getSaturation() < 0.45f)
                        pixels.setPixelColour (x, y, juce::Colour (0xfff5f2eb).withAlpha (colour.getAlpha()));
                }
            }
            return result;
        }
    }

    HeaderBar::HeaderBar (VRACPlusAudioProcessor& processor) : processorRef (processor)
    {
        skeerLogo = juce::ImageFileFormat::loadFrom (VRACAssets::skeerwordmark_png,
                                                     (size_t) VRACAssets::skeerwordmark_pngSize);
        skeerLogoDark = makeDarkWordmark (skeerLogo);
        addAndMakeVisible (presetBox);
        presetBox.setTextWhenNothingSelected ("CURRENT KIT");
        presetBox.setEditableText (false);
        const auto& presets = vrac::factoryPresets();
        presetBox.addItem ("CURRENT KIT", 100);
        for (int i = 0; i < (int) presets.size(); ++i)
            presetBox.addItem (presets[(size_t) i].name, i + 1);
        presetBox.onChange = [this]
        {
            const auto id = presetBox.getSelectedId();
            if (id > 0 && id <= (int) vrac::factoryPresets().size())
                processorRef.applyFactoryPresetFromUi (id - 1);
        };

        addAndMakeVisible (kitTabButton);
        addAndMakeVisible (sequencerTabButton);
        addAndMakeVisible (vaultTabButton);
        styleChip (kitTabButton, true);
        styleChip (sequencerTabButton, false);
        styleChip (vaultTabButton, false);
        kitTabButton.onClick = [this]
        {
            if (onPageSelected)
                onPageSelected (Page::kit);
        };
        sequencerTabButton.onClick = [this]
        {
            if (onPageSelected)
                onPageSelected (Page::sequencer);
        };
        vaultTabButton.onClick = [this]
        {
            if (onPageSelected)
                onPageSelected (Page::vault);
        };

        addAndMakeVisible (themeButton);
        styleChip (themeButton, false);
        themeButton.onClick = [this] { if (onThemeToggle) onThemeToggle(); };

        for (auto* button : { &undoButton, &redoButton, &randomButton, &saveButton, &loadButton, &helpButton, &aboutButton })
            addAndMakeVisible (*button);
        styleChip (undoButton);
        styleChip (redoButton);
        styleChip (randomButton);
        styleChip (saveButton);
        styleChip (loadButton);
        styleChip (helpButton);
        styleChip (aboutButton);

        undoButton.onClick = [this] { processorRef.undo(); refresh(); };
        redoButton.onClick = [this] { processorRef.redo(); refresh(); };
        randomButton.onClick = [this] { processorRef.randomizePads(); refresh(); };
        saveButton.onClick = [this] { if (onSavePreset) onSavePreset(); };
        loadButton.onClick = [this] { if (onLoadPreset) onLoadPreset(); };
        helpButton.onClick = [this] { if (onHelp) onHelp(); };
        aboutButton.onClick = [this] { if (onAbout) onAbout(); };
    }

    void HeaderBar::refresh()
    {
        const auto name = processorRef.getPresetName();
        int selectedId = 0;
        const auto& presets = vrac::factoryPresets();
        for (int i = 0; i < (int) presets.size(); ++i)
            if (presets[(size_t) i].name == name)
                selectedId = i + 1;
        if (selectedId == 0)
        {
            presetBox.changeItemText (100, name.isNotEmpty() ? name : "CURRENT KIT");
            selectedId = 100;
        }
        if (presetBox.getSelectedId() != selectedId)
            presetBox.setSelectedId (selectedId, juce::dontSendNotification);
        undoButton.setEnabled (processorRef.canUndo());
        redoButton.setEnabled (processorRef.canRedo());
        themeButton.setButtonText (isDarkTheme() ? "LIGHT" : "DARK");
        repaint();
    }

    void HeaderBar::resized()
    {
        auto area = getLocalBounds().reduced (12, 8);
        area.removeFromLeft (318); // SKEER wordmark, rule, product overline
        presetBox.setBounds (area.removeFromLeft (210).reduced (0, 4));
        area.removeFromLeft (8);
        kitTabButton.setBounds (area.removeFromLeft (52).reduced (0, 4));
        sequencerTabButton.setBounds (area.removeFromLeft (74).reduced (0, 4));
        vaultTabButton.setBounds (area.removeFromLeft (54).reduced (0, 4));
        area.removeFromLeft (8);
        undoButton.setBounds (area.removeFromLeft (44).reduced (0, 4));
        redoButton.setBounds (area.removeFromLeft (44).reduced (0, 4));
        randomButton.setBounds (area.removeFromLeft (38).reduced (0, 4));
        area.removeFromLeft (4);
        saveButton.setBounds (area.removeFromLeft (42).reduced (0, 4));
        loadButton.setBounds (area.removeFromLeft (42).reduced (0, 4));
        area.removeFromLeft (6);
        helpButton.setBounds (area.removeFromLeft (40).reduced (0, 4));
        aboutButton.setBounds (area.removeFromLeft (38).reduced (0, 4));
        area.removeFromLeft (4);
        themeButton.setBounds (area.removeFromLeft (44).reduced (0, 4));
    }

    void HeaderBar::setPageActive (Page page)
    {
        styleChip (kitTabButton, page == Page::kit);
        styleChip (sequencerTabButton, page == Page::sequencer);
        styleChip (vaultTabButton, page == Page::vault);
    }

    void HeaderBar::paint (juce::Graphics& g)
    {
        const auto& c = palette();
        g.setColour (c.panel);
        g.fillAll();
        g.setColour (c.line);
        g.fillRect (0, getHeight() - 1, getWidth(), 1);

        // Approved SKEER wordmark artwork.
        auto logoArea = juce::Rectangle<int> (14, 10, 160, 40);
        const auto& logo = isDarkTheme() && skeerLogoDark.isValid() ? skeerLogoDark : skeerLogo;
        if (logo.isValid())
        {
            g.drawImage (logo, logoArea.toFloat(),
                         juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
        }
        else
        {
            g.setColour (c.ink);
            g.setFont (display (28.0f));
            g.drawText ("SKEER", logoArea, juce::Justification::centredLeft, false);
        }

        g.setColour (c.line);
        g.fillRect (184, 14, 1, 32);

        g.setFont (display (29.0f));
        g.setColour (c.muted);
        g.drawText ("VRAC", 198, 5, 84, 30, juce::Justification::centredLeft, false);
        g.setColour (c.accent);
        g.drawText ("+", 258, 5, 20, 30, juce::Justification::centredLeft, false);
        g.setColour (c.muted);
        g.setFont (font (6.8f, true));
        g.drawText ("SAMPLE DRUM MACHINE", 199, 35, 150, 10, juce::Justification::centredLeft, false);

        g.setFont (mono (7.5f));
        g.setColour (c.muted);
        const auto right = getWidth() - 14;
        g.drawText (juce::String (processorRef.getEngineSampleRate() / 1000.0, 1) + " kHz  /  VST3",
                    right - 150, 22, 150, 12, juce::Justification::centredRight, false);
        g.setColour (processorRef.getMidiLearnTarget().isNotEmpty() ? c.accent : c.muted);
        g.drawText (processorRef.getMidiLearnTarget().isNotEmpty() ? "MIDI LEARN" : "MIDI READY",
                    right - 150, 35, 150, 10, juce::Justification::centredRight, false);
    }
} // namespace vrac::ui


