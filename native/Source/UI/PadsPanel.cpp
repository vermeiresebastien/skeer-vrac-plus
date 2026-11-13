#include "PadsPanel.h"
#include "Theme.h"
#include "Controls.h"

namespace vrac::ui
{
    PadsPanel::PadsPanel (VRACPlusAudioProcessor& processor) : processorRef (processor), padGrid (processor)
    {
        styleChip (autoSelectButton, true);
        autoSelectButton.setClickingTogglesState (true);
        autoSelectButton.setToggleState (true, juce::dontSendNotification);
        autoSelectButton.onClick = [this]
        {
            autoSelect = autoSelectButton.getToggleState();
            styleChip (autoSelectButton, autoSelect);
            if (onAutoSelectChanged)
                onAutoSelectChanged (autoSelect);
        };
        addAndMakeVisible (autoSelectButton);

        styleChip (stopButton, false);
        stopButton.onClick = [this] { processorRef.chokeAll(); };
        addAndMakeVisible (stopButton);
        addAndMakeVisible (padGrid);
    }

    void PadsPanel::setAutoSelect (bool shouldAutoSelect)
    {
        autoSelect = shouldAutoSelect;
        autoSelectButton.setToggleState (autoSelect, juce::dontSendNotification);
        styleChip (autoSelectButton, autoSelect);
    }

    void PadsPanel::resized()
    {
        auto area = getLocalBounds();
        auto header = area.removeFromTop (24);
        autoSelectButton.setBounds (header.withSizeKeepingCentre (112, 22));
        stopButton.setBounds (header.removeFromRight (64).reduced (2, 1));
        area.removeFromTop (6);
        padGrid.setBounds (area.removeFromTop (juce::jmax (0, area.getHeight() - 120)).reduced (2, 0));
    }

    void PadsPanel::paint (juce::Graphics& g)
    {
        const auto& c = palette();
        drawPanelBackground (g, getLocalBounds().toFloat(), true);
        drawSectionLabel (g, { 13, 8 }, "PADS");
        g.setColour (c.muted);
        g.setFont (font (7.0f, true));
        g.drawText ("16 PADS / 4 FAMILIES", 55, 8, 130, 14, juce::Justification::centredLeft, false);
        g.setColour (c.muted);
        g.setFont (font (7.0f, true));
        g.drawText ("OUTPUTS", 13, getHeight() - 113, 100, 12, juce::Justification::centredLeft, false);
        g.setColour (c.accent);
        g.setFont (mono (8.0f));
        g.drawText (juce::String (processorRef.getActiveVoiceCount()) + " VOICES", getWidth() - 90, getHeight() - 113,
                    77, 12, juce::Justification::centredRight, false);

        // Eight stereo output meters.
        const auto meterTop = getHeight() - 96;
        const auto width = getWidth() - 26;
        for (int bus = 0; bus < vrac::kNumBuses; ++bus)
        {
            const auto y = meterTop + bus * 8;
            g.setColour (c.muted);
            g.setFont (mono (6.5f));
            g.drawText (juce::String (bus + 1).paddedLeft ('0', 2), 13, y - 2, 15, 8,
                        juce::Justification::centredLeft, false);
            auto track = juce::Rectangle<float> (33.0f, (float) y + 2.0f, (float) width - 33.0f, 3.0f);
            g.setColour (c.line);
            g.fillRoundedRectangle (track, 1.5f);
            const auto peak = processorRef.getBusPeak (bus);
            if (peak > 0.002f)
            {
                g.setColour (peak > 0.96f ? c.red : c.accent);
                g.fillRoundedRectangle (track.withWidth (juce::jmin (track.getWidth(), peak * track.getWidth() * 1.4f)), 1.5f);
            }
        }

        g.setColour (c.muted);
        g.setFont (font (7.0f, true));
        g.drawText ("PLAY WITH 1 2 3 4 / Q W E R / A S D F / Z X C V", 13, getHeight() - 24,
                    getWidth() - 26, 14, juce::Justification::centredLeft, false);
        g.setFont (font (6.8f, true));
        g.setColour (c.muted);
        g.drawText ("SHIFT = ACCENT   ALT = GHOST", 13, getHeight() - 12, getWidth() - 26, 10,
                    juce::Justification::centredRight, false);
    }
} // namespace vrac::ui

