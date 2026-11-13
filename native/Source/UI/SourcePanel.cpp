#include "SourcePanel.h"
#include "Theme.h"

namespace vrac::ui
{
    SourcePanel::SourcePanel (VRACPlusAudioProcessor& processor)
        : processorRef (processor), sensitivity (processor, "AUTO SLICE", "source.sensitivity")
    {
        stylePrimaryButton (loadButton);
        styleChip (libraryButton);
        styleChip (recordButton);
        addAndMakeVisible (loadButton);
        addAndMakeVisible (libraryButton);
        addAndMakeVisible (recordButton);
        loadButton.onClick = [this] { if (onLoadFile) onLoadFile(); };
        libraryButton.onClick = [this] { showLibraryMenu(); };
        recordButton.onClick = [this] { if (onToggleRecording) onToggleRecording(); };

        addAndMakeVisible (waveform);
        waveform.onGestureStart = [this] { processorRef.pushUndoSnapshot(); };
        waveform.onStartChanged = [this] (double time) { processorRef.setPadStart (selectedPad, time); };

        sensitivity.onValueChanged = [this]
        {
            processorRef.onSensitivityChanged();
            if (onSourceChanged)
                onSourceChanged();
        };
        addAndMakeVisible (sensitivity);

        for (auto* button : { &previousSlice, &nextSlice, &randomSlice, &addSlice, &deleteSlice })
        {
            styleSmallPanelButton (*button, palette().panel);
            addAndMakeVisible (*button);
        }
        previousSlice.onClick = [this] { if (onSliceNav) onSliceNav (-1); };
        nextSlice.onClick = [this] { if (onSliceNav) onSliceNav (1); };
        randomSlice.onClick = [this] { if (onSliceNav) onSliceNav (0); };
        addSlice.onClick = [this] { if (onAddDeleteSlice) onAddDeleteSlice (true); };
        deleteSlice.onClick = [this] { if (onAddDeleteSlice) onAddDeleteSlice (false); };

        const std::array<std::pair<const char*, const char*>, 8> ids {{
            { "FINE", "global.fine" }, { "DRUMIFY", "global.drumify" }, { "VOLUME", "global.volume" },
            { "TRANSPOSE", "global.transpose" }, { "LENGTH", "global.length" }, { "TONE", "global.tone" },
            { "FILTER", "global.filter" }, { "WIDTH", "global.width" }
        }};
        for (int i = 0; i < 8; ++i)
        {
            globals[(size_t) i] = std::make_unique<ParamKnob> (processorRef, ids[(size_t) i].first,
                                                               ids[(size_t) i].second, 44);
            addAndMakeVisible (*globals[(size_t) i]);
        }
        refreshFromProcessor();
    }

    void SourcePanel::setSelectedPad (int pad)
    {
        selectedPad = juce::jlimit (0, vrac::kNumPads - 1, pad);
        waveform.setStart (processorRef.getPadStart (selectedPad));
        repaint();
    }

    void SourcePanel::refreshFromProcessor()
    {
        waveform.setSample (processorRef.getSamplePtr());
        waveform.setSlices (processorRef.getSlices());
        waveform.setStart (processorRef.getPadStart (selectedPad));
        const bool recording = processorRef.isRecording();
        recordButton.setButtonText (recording ? "STOP" : "REC");
        styleChip (recordButton, recording);
        if (processorRef.isRecordingTakeReady())
        {
            recordButton.setButtonText ("USE");
            styleChip (recordButton, true);
        }
        repaint();
    }

    void SourcePanel::showLibraryMenu()
    {
        juce::PopupMenu menu;
        int itemId = 1;
        for (const auto& source : vrac::factorySources())
            menu.addItem (itemId++, source.name);
        auto safe = juce::Component::SafePointer<SourcePanel> (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&libraryButton),
                            [safe] (int result)
                            {
                                if (safe == nullptr || result <= 0)
                                    return;
                                const auto& sources = vrac::factorySources();
                                const auto index = juce::jlimit (0, (int) sources.size() - 1, result - 1);
                                safe->processorRef.loadFactorySource (sources[(size_t) index].id);
                                safe->refreshFromProcessor();
                                if (safe->onSourceChanged)
                                    safe->onSourceChanged();
                            });
    }

    void SourcePanel::resized()
    {
        waveform.setBounds (12, 50, getWidth() - 24, 108);

        loadButton.setBounds (12, 174, 72, 24);
        libraryButton.setBounds (89, 174, 84, 24);
        recordButton.setBounds (178, 174, 54, 24);

        sensitivity.setBounds (15, 224, getWidth() - 30, 38);

        const auto navY = 266;
        previousSlice.setBounds (12, navY, 24, 20);
        nextSlice.setBounds (39, navY, 24, 20);
        randomSlice.setBounds (66, navY, 36, 20);
        addSlice.setBounds (104, navY, 24, 20);
        deleteSlice.setBounds (131, navY, 36, 20);

        auto globalArea = juce::Rectangle<int> (8, 316, getWidth() - 16, 160);
        const auto knobW = globalArea.getWidth() / 4;
        for (int row = 0; row < 2; ++row)
            for (int col = 0; col < 4; ++col)
                globals[(size_t) (row * 4 + col)]->setBounds (
                    globalArea.getX() + col * knobW, globalArea.getY() + row * 80, knobW, 78);
    }

    void SourcePanel::paint (juce::Graphics& g)
    {
        const auto& c = palette();
        drawPanelBackground (g, getLocalBounds().toFloat(), true);
        drawSectionLabel (g, { 13, 8 }, "SOURCE");
        g.setColour (c.muted);
        g.setFont (font (6.8f, true));
        g.drawText (processorRef.getSourceName(), 82, 8, getWidth() - 96, 14,
                    juce::Justification::centredRight, false);

        auto waveCard = juce::Rectangle<int> (8, 30, getWidth() - 16, 142);
        g.setColour (c.panel);
        g.fillRoundedRectangle (waveCard.toFloat(), 4.0f);
        g.setColour (c.line);
        g.drawRoundedRectangle (waveCard.toFloat().reduced (0.5f), 4.0f, 1.0f);
        g.setColour (c.muted);
        g.setFont (mono (7.0f));
        g.drawText ("00:00.000", waveCard.getX() + 7, waveCard.getY() + 4, 70, 10,
                    juce::Justification::centredLeft, false);
        if (const auto sample = processorRef.getSamplePtr())
            g.drawText (formatTime (sample->getDurationSeconds()), waveCard.getRight() - 77, waveCard.getY() + 4,
                        70, 10, juce::Justification::centredRight, false);
        g.setFont (font (7.0f, true));
        g.drawText (processorRef.sourceIsFactorySample() ? "SKEER LIBRARY" : "USER SAMPLE",
                    waveCard.getX() + 80, waveCard.getY() + 4, waveCard.getWidth() - 160, 10,
                    juce::Justification::centred, false);
        g.setColour (c.muted);
        g.setFont (font (6.8f, true));
        g.drawText ("DRAG THE MARKER TO MOVE THE SELECTED PAD'S START", 11, 163, getWidth() - 22, 10,
                    juce::Justification::centredLeft, false);

        auto sliceArea = juce::Rectangle<int> (8, 202, getWidth() - 16, 88);
        g.setColour (c.panel);
        g.fillRoundedRectangle (sliceArea.toFloat(), 4.0f);
        g.setColour (c.line);
        g.drawRoundedRectangle (sliceArea.toFloat().reduced (0.5f), 4.0f, 1.0f);
        g.setColour (c.muted);
        g.setFont (font (7.0f, true));
        g.drawText ("HITS", sliceArea.getRight() - 80, sliceArea.getY() + 5, 36, 10,
                    juce::Justification::centredRight, false);
        g.setColour (c.accent);
        g.setFont (mono (8.0f));
        g.drawText (juce::String ((int) processorRef.getSlices().size()),
                    sliceArea.getRight() - 40, sliceArea.getY() + 5, 30, 10,
                    juce::Justification::centredRight, false);

        const auto pad = processorRef.getSelectedPad();
        g.setColour (c.muted);
        g.setFont (font (7.0f, true));
        g.drawText (juce::String (vrac::padNotes()[(size_t) pad]) + " START", sliceArea.getX() + 170, 266, 54, 10,
                    juce::Justification::centredLeft, false);
        g.setColour (c.ink);
        g.setFont (mono (9.0f));
        g.drawText (formatTime (processorRef.getPadStart (pad)), sliceArea.getX() + 224, 264, 100, 12,
                    juce::Justification::centredLeft, false);

        drawSectionLabel (g, { 13, 298 }, "GLOBAL");
        g.setColour (c.muted);
        g.setFont (font (6.8f, true));
        g.drawText ("KIT", getWidth() - 42, 298, 28, 14, juce::Justification::centredRight, false);

        auto quick = juce::Rectangle<int> (8, 490, getWidth() - 16, 90);
        g.setColour (c.line);
        g.fillRect (quick.getX(), quick.getY(), quick.getWidth(), 1);
        g.setColour (c.ink);
        g.setFont (narrow (10.0f, true));
        g.drawText ("QUICK START", quick.getX() + 3, quick.getY() + 8, 120, 12, juce::Justification::centredLeft, false);
        g.setFont (font (6.8f, true));
        g.setColour (c.muted);
        g.drawText ("01  DROP ANY SOUND", quick.getX() + 3, quick.getY() + 28, 280, 10, juce::Justification::centredLeft, false);
        g.drawText ("02  AUTO-SLICE THE HITS", quick.getX() + 3, quick.getY() + 44, 280, 10, juce::Justification::centredLeft, false);
        g.drawText ("03  MORPH EACH PAD", quick.getX() + 3, quick.getY() + 60, 280, 10, juce::Justification::centredLeft, false);

        auto info = juce::Rectangle<int> (8, 600, getWidth() - 16, 114);
        g.setColour (c.line);
        g.fillRect (info.getX(), info.getY(), info.getWidth(), 1);
        g.setColour (c.muted);
        g.setFont (font (6.8f, true));
        g.drawText ("DURATION", info.getX() + 2, info.getY() + 10, 60, 10, juce::Justification::centredLeft, false);
        g.drawText ("RATE", info.getX() + 82, info.getY() + 10, 50, 10, juce::Justification::centredLeft, false);
        g.drawText ("SLICES", info.getX() + 142, info.getY() + 10, 50, 10, juce::Justification::centredLeft, false);
        g.drawText ("PADS", info.getX() + 202, info.getY() + 10, 40, 10, juce::Justification::centredLeft, false);
        g.setColour (c.ink);
        g.setFont (mono (8.5f));
        if (const auto sample = processorRef.getSamplePtr())
        {
            g.drawText (juce::String (sample->getDurationSeconds(), 1) + "s", info.getX() + 2, info.getY() + 24, 60, 12,
                        juce::Justification::centredLeft, false);
            g.drawText (juce::String (sample->sampleRate / 1000.0, 1) + " kHz", info.getX() + 82, info.getY() + 24, 60, 12,
                        juce::Justification::centredLeft, false);
        }
        g.drawText (juce::String ((int) processorRef.getSlices().size()), info.getX() + 142, info.getY() + 24, 50, 12,
                    juce::Justification::centredLeft, false);
        g.drawText ("16", info.getX() + 202, info.getY() + 24, 40, 12, juce::Justification::centredLeft, false);

        g.setColour (processorRef.isRecording() ? c.red : c.green);
        g.fillEllipse ((float) info.getX() + 2.0f, (float) info.getY() + 51.0f, 4.0f, 4.0f);
        g.setColour (c.muted);
        g.setFont (font (6.8f, true));
        g.drawText (processorRef.isRecording() ? "RECORDING INPUT..." : "SKEER FACTORY LIBRARY / SWAP ANY TIME",
                    info.getX() + 11, info.getY() + 48, info.getWidth() - 12, 12,
                    juce::Justification::centredLeft, false);
        g.drawText (processorRef.getMidiLearnTarget().isNotEmpty()
                        ? "MIDI LEARN: " + processorRef.getMidiLearnTarget()
                        : "RIGHT-CLICK A KNOB TO MIDI LEARN",
                    info.getX() + 2, info.getY() + 68, info.getWidth() - 4, 12,
                    juce::Justification::centredLeft, false);
        g.setColour (c.muted);
        g.setFont (font (6.8f, true));
        g.drawText ("LOAD OR DROP A FILE, OR RECORD AN INPUT TO START A NEW KIT", info.getX() + 2, info.getY() + 88,
                    info.getWidth() - 4, 12, juce::Justification::centredLeft, false);
    }
} // namespace vrac::ui

