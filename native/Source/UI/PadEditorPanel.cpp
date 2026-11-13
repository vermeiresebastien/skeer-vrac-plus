#include "PadEditorPanel.h"
#include "Theme.h"

namespace vrac::ui
{
    PadEditorPanel::PadEditorPanel (VRACPlusAudioProcessor& processor)
        : processorRef (processor),
          originalKnob (processor, "ORIGINAL", "p01_original", 44),
          drumifyKnob (processor, "DRUMIFY", "p01_drumify", 80),
          drumifiedKnob (processor, "DRUMIFIED", "p01_drumified", 44)
    {
        addAndMakeVisible (waveform);
        waveform.setCompact (true);
        waveform.setFocusSeconds (1.0);
        waveform.onGestureStart = [this] { processorRef.pushUndoSnapshot(); };
        waveform.onStartChanged = [this] (double seconds) { processorRef.setPadStart (selectedPad, seconds); };

        addAndMakeVisible (originalKnob);
        addAndMakeVisible (drumifyKnob);
        addAndMakeVisible (drumifiedKnob);
        originalKnob.setAccent (palette().accent);
        drumifiedKnob.setAccent (palette().accent);

        stylePrimaryButton (auditionButton);
        styleChip (copyButton);
        styleChip (pasteButton);
        styleChip (resetButton);
        auditionButton.setButtonText ("PLAY");
        for (auto* b : { &auditionButton, &copyButton, &pasteButton, &resetButton })
            addAndMakeVisible (*b);
        auditionButton.onClick = [this] { audition(); };
        copyButton.onClick = [this] { copyPad(); };
        pasteButton.onClick = [this] { pastePad(); };
        resetButton.onClick = [this] { resetPad(); };

        const char* controlIds[] { "pitch", "length", "velocity", "pan", "volume", "tone" };
        const char* controlLabels[] { "PITCH", "LENGTH", "VELOCITY", "PAN", "VOLUME", "TONE" };
        for (int i = 0; i < 6; ++i)
        {
            controlKnobs[(size_t) i] = std::make_unique<ParamKnob> (processor, controlLabels[i],
                                                                    vrac::padParamId (0, controlIds[i]), 44);
            addAndMakeVisible (*controlKnobs[(size_t) i]);
        }
        const char* sculptIds[] { "warp", "impact", "instability", "super" };
        const char* sculptLabels[] { "WARP", "IMPACT", "INSTAB", "SUPER" };
        for (int i = 0; i < 4; ++i)
        {
            sculptKnobs[(size_t) i] = std::make_unique<ParamKnob> (processor, sculptLabels[i],
                                                                   vrac::padParamId (0, sculptIds[i]), 38);
            addAndMakeVisible (*sculptKnobs[(size_t) i]);
        }
        const char* eqIds[] { "eqLow", "eqMid", "eqHigh" };
        const char* eqLabels[] { "LOW", "MID", "HIGH" };
        for (int i = 0; i < 3; ++i)
        {
            eqKnobs[(size_t) i] = std::make_unique<ParamKnob> (processor, eqLabels[i],
                                                               vrac::padParamId (0, eqIds[i]), 34);
            addAndMakeVisible (*eqKnobs[(size_t) i]);
        }

        for (int i = 0; i < 5; ++i)
        {
            chokeButtons[(size_t) i].setButtonText (i == 0 ? "OFF" : juce::String (i));
            styleSmallPanelButton (chokeButtons[(size_t) i], palette().accent);
            addAndMakeVisible (chokeButtons[(size_t) i]);
            chokeButtons[(size_t) i].onClick = [this, i] { setPadParam ("choke", (float) i); updateButtonStates(); };
        }
        for (int i = 0; i < 8; ++i)
        {
            busButtons[(size_t) i].setButtonText (juce::String (i + 1));
            styleSmallPanelButton (busButtons[(size_t) i], palette().accent);
            addAndMakeVisible (busButtons[(size_t) i]);
            busButtons[(size_t) i].onClick = [this, i] { setPadParam ("bus", (float) (i + 1)); updateButtonStates(); };
        }

        for (int family = 0; family < vrac::kNumFamilies; ++family)
        {
            for (int model = 0; model < vrac::kModelsPerFamily; ++model)
            {
                const auto index = family * vrac::kModelsPerFamily + model;
                auto& button = modelButtons[(size_t) index];
                button.setButtonText (vrac::modelBank()[(size_t) family][(size_t) model].name);
                button.setColour (juce::TextButton::buttonOnColourId, palette().accent);
                addAndMakeVisible (button);
                button.onClick = [this, index] { setModel (index); };
            }
        }

        setSelectedPad (processorRef.getSelectedPad());
        refreshFromProcessor();
    }

    void PadEditorPanel::setSelectedPad (int pad)
    {
        const auto next = juce::jlimit (0, vrac::kNumPads - 1, pad);
        const bool changed = next != selectedPad;
        selectedPad = next;
        if (changed)
            updateParameterIds();
        refreshFromProcessor();
        if (changed)
            repaint();
    }

    void PadEditorPanel::updateParameterIds()
    {
        originalKnob.setParameterId (vrac::padParamId (selectedPad, "original"));
        drumifyKnob.setParameterId (vrac::padParamId (selectedPad, "drumify"));
        drumifiedKnob.setParameterId (vrac::padParamId (selectedPad, "drumified"));
        const char* ids[] { "pitch", "length", "velocity", "pan", "volume", "tone" };
        for (int i = 0; i < 6; ++i)
            controlKnobs[(size_t) i]->setParameterId (vrac::padParamId (selectedPad, ids[i]));
        const char* sculptIds[] { "warp", "impact", "instability", "super" };
        for (int i = 0; i < 4; ++i)
            sculptKnobs[(size_t) i]->setParameterId (vrac::padParamId (selectedPad, sculptIds[i]));
        const char* eqIds[] { "eqLow", "eqMid", "eqHigh" };
        for (int i = 0; i < 3; ++i)
            eqKnobs[(size_t) i]->setParameterId (vrac::padParamId (selectedPad, eqIds[i]));
        updateButtonStates();
    }

    void PadEditorPanel::refreshFromProcessor()
    {
        const auto pad = processorRef.getParameterCache().readPad (selectedPad);
        const auto accent = palette().accent;
        drumifyKnob.setAccent (accent);
        originalKnob.setAccent (accent);
        drumifiedKnob.setAccent (accent);
        for (auto& button : chokeButtons)
            button.setColour (juce::TextButton::buttonOnColourId, accent);
        for (auto& button : busButtons)
            button.setColour (juce::TextButton::buttonOnColourId, accent);
        waveform.setSample (processorRef.getSamplePtr());
        waveform.setSlices (processorRef.getSlices());
        waveform.setStart (processorRef.getPadStart (selectedPad));
        waveform.setFocusSeconds (juce::jmax (0.7, (double) pad.length * 3.2));
        updateButtonStates();
        repaint();
    }

    void PadEditorPanel::updateButtonStates()
    {
        const auto pad = processorRef.getParameterCache().readPad (selectedPad);
        for (int i = 0; i < 5; ++i)
            chokeButtons[(size_t) i].setToggleState (pad.choke == i, juce::dontSendNotification);
        for (int i = 0; i < 8; ++i)
            busButtons[(size_t) i].setToggleState (pad.bus == i + 1, juce::dontSendNotification);
        for (int i = 0; i < 16; ++i)
            modelButtons[(size_t) i].setToggleState (pad.modelIndex == i, juce::dontSendNotification);
        pasteButton.setEnabled (hasCopiedPad);
    }

    void PadEditorPanel::setPadParam (const juce::String& suffix, float value)
    {
        processorRef.pushUndoSnapshot();
        if (auto* parameter = processorRef.getValueTreeState().getParameter (vrac::padParamId (selectedPad, suffix.toRawUTF8())))
        {
            const auto& range = parameter->getNormalisableRange();
            parameter->setValueNotifyingHost (range.convertTo0to1 (value));
        }
    }

    void PadEditorPanel::setModel (int modelIndex)
    {
        setPadParam ("model", (float) modelIndex);
        processorRef.triggerPad (selectedPad, 0.9f);
        updateButtonStates();
        repaint();
    }

    void PadEditorPanel::audition()
    {
        processorRef.triggerPad (selectedPad, 0.9f);
    }

    void PadEditorPanel::copyPad()
    {
        copiedPad = processorRef.getParameterCache().readPad (selectedPad);
        hasCopiedPad = true;
        updateButtonStates();
        repaint();
    }

    void PadEditorPanel::pastePad()
    {
        if (! hasCopiedPad)
            return;
        processorRef.pushUndoSnapshot();
        processorRef.getParameterCache().writePad (selectedPad, copiedPad);
        refreshFromProcessor();
    }

    void PadEditorPanel::resetPad()
    {
        processorRef.pushUndoSnapshot();
        processorRef.getParameterCache().writePad (selectedPad, vrac::makeDefaultPad (selectedPad));
        refreshFromProcessor();
    }

    void PadEditorPanel::resized()
    {
        auto header = getLocalBounds().removeFromTop (44).reduced (8, 2);
        auto buttons = header.removeFromRight (160);
        const auto bw = 38;
        auditionButton.setBounds (buttons.removeFromLeft (bw));
        buttons.removeFromLeft (2);
        copyButton.setBounds (buttons.removeFromLeft (bw));
        buttons.removeFromLeft (2);
        pasteButton.setBounds (buttons.removeFromLeft (bw));
        buttons.removeFromLeft (2);
        resetButton.setBounds (buttons.removeFromLeft (bw));

        waveform.setBounds (8, 56, getWidth() - 16, 46);

        auto morph = juce::Rectangle<int> (8, 110, getWidth() - 16, 96);
        originalKnob.setBounds (morph.getX() + 2, morph.getY() + 12, 68, 76);
        drumifyKnob.setBounds (morph.getCentreX() - 48, morph.getY() + 4, 96, 88);
        drumifiedKnob.setBounds (morph.getRight() - 70, morph.getY() + 12, 68, 76);

        auto controls = juce::Rectangle<int> (8, 212, getWidth() - 16, 138);
        const auto colW = controls.getWidth() / 3;
        for (int row = 0; row < 2; ++row)
            for (int col = 0; col < 3; ++col)
                controlKnobs[(size_t) (row * 3 + col)]->setBounds (
                    controls.getX() + col * colW, controls.getY() + row * 68, colW, 66);

        auto chokeOut = juce::Rectangle<int> (8, 368, getWidth() - 16, 22);
        auto chokeArea = chokeOut.removeFromLeft (chokeOut.getWidth() / 2).reduced (0, 1);
        for (int i = 0; i < 5; ++i)
            chokeButtons[(size_t) i].setBounds (chokeArea.removeFromLeft (chokeArea.getWidth() / (5 - i)).reduced (1, 0));
        for (int i = 0; i < 8; ++i)
            busButtons[(size_t) i].setBounds (chokeOut.removeFromLeft (chokeOut.getWidth() / (8 - i)).reduced (1, 0));

        auto modelArea = juce::Rectangle<int> (8, 414, getWidth() - 16, 120);
        modelArea.removeFromLeft (42);
        const auto cellW = modelArea.getWidth() / 4;
        const auto cellH = modelArea.getHeight() / 4;
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
                modelButtons[(size_t) (row * 4 + col)].setBounds (
                    modelArea.getX() + col * cellW, modelArea.getY() + row * cellH, cellW, cellH);

        auto sculptArea = juce::Rectangle<int> (8, 558, getWidth() - 16, 56);
        for (int i = 0; i < 4; ++i)
            sculptKnobs[(size_t) i]->setBounds (sculptArea.removeFromLeft (sculptArea.getWidth() / (4 - i)));

        auto eqArea = juce::Rectangle<int> (8, 634, getWidth() - 16, 50);
        for (int i = 0; i < 3; ++i)
            eqKnobs[(size_t) i]->setBounds (eqArea.removeFromLeft (eqArea.getWidth() / (3 - i)));
    }

    void PadEditorPanel::paint (juce::Graphics& g)
    {
        const auto& c = palette();
        drawPanelBackground (g, getLocalBounds().toFloat(), true);
        drawSectionLabel (g, { 13, 6 }, "03 / SELECTED PAD");

        const auto pad = processorRef.getParameterCache().readPad (selectedPad);
        const auto family = vrac::familyForModel (pad.modelIndex);
        const auto model = vrac::localModelForIndex (pad.modelIndex);

        g.setColour (c.ink);
        g.setFont (display (25.0f));
        g.drawText (vrac::familyName (family), 10, 18, 120, 24, juce::Justification::centredLeft, false);
        g.setColour (c.accent);
        g.setFont (mono (8.0f));
        g.drawText (vrac::padNotes()[(size_t) selectedPad], 12, 40, 60, 10, juce::Justification::centredLeft, false);
        juce::Rectangle<int> modelBadge (122, 26, 58, 13);
        g.setColour (c.panel);
        g.fillRoundedRectangle (modelBadge.toFloat(), 2.5f);
        g.setColour (c.line);
        g.drawRoundedRectangle (modelBadge.toFloat().reduced (0.5f), 2.5f, 1.0f);
        g.setColour (c.muted);
        g.setFont (font (6.8f, true));
        g.drawText (vrac::modelBank()[(size_t) family][(size_t) model].name, modelBadge, juce::Justification::centred, false);

        auto wave = juce::Rectangle<int> (8, 44, getWidth() - 16, 62);
        drawInsetBackground (g, wave.toFloat());
        g.setColour (c.muted);
        g.setFont (mono (7.5f));
        g.drawText (formatTime (processorRef.getPadStart (selectedPad)), wave.getX() + 7, wave.getY() + 3, 75, 10,
                    juce::Justification::centredLeft, false);
        g.setFont (font (6.8f, true));
        g.drawText ("DRAG START", wave.getRight() - 80, wave.getY() + 3, 73, 10, juce::Justification::centredRight, false);

        auto morph = juce::Rectangle<int> (8, 110, getWidth() - 16, 96);
        drawInsetBackground (g, morph.toFloat());

        auto controls = juce::Rectangle<int> (8, 212, getWidth() - 16, 138);
        drawInsetBackground (g, controls.toFloat());

        g.setColour (c.muted);
        g.setFont (font (6.8f, true));
        g.drawText ("CHOKE", 8, 356, 40, 10, juce::Justification::centredLeft, false);
        g.drawText ("OUTPUT", getWidth() / 2 + 4, 356, 50, 10, juce::Justification::centredLeft, false);

        g.setFont (narrow (11.0f, true));
        g.drawText ("04 / MODEL BANK", 10, 396, 180, 14, juce::Justification::centredLeft, false);
        g.drawText ("05 / SCULPT", 10, 542, 160, 14, juce::Justification::centredLeft, false);
        g.drawText ("3-BAND EQ", 10, 618, 160, 14, juce::Justification::centredLeft, false);

        auto modelArea = juce::Rectangle<int> (8, 414, getWidth() - 16, 120);
        auto labels = modelArea.removeFromLeft (42);
        for (int f = 0; f < 4; ++f)
        {
            g.setColour (vrac::ui::familyColour (f));
            g.setFont (font (6.5f, true));
            g.drawText (vrac::familyName (f), labels.getX(), labels.getY() + f * 30 + 8, 42, 10,
                        juce::Justification::centredLeft, false);
        }

        auto summary = juce::Rectangle<int> (8, 690, getWidth() - 16, 28);
        g.setColour (c.line);
        g.fillRect (summary.getX(), summary.getY(), summary.getWidth(), 1);
        g.setColour (c.muted);
        g.setFont (narrow (6.8f, true));
        const auto blurb = vrac::modelBank()[(size_t) family][(size_t) model].blurb;
        g.drawText (juce::String (blurb), summary.getX() + 1, summary.getY() + 5, summary.getWidth() - 2, 10,
                    juce::Justification::centredLeft, false);
        g.setColour (c.inkSoft);
        g.setFont (mono (7.0f));
        g.drawText ("SEL " + juce::String (selectedPad + 1).paddedLeft ('0', 2)
                        + "  ·  " + juce::String ((int) std::round (processorRef.getPadStart (selectedPad) * 1000.0)) + " ms START"
                        + "  ·  OUT " + juce::String (pad.bus)
                        + "  ·  CHOKE " + (pad.choke == 0 ? juce::String ("OFF") : juce::String (pad.choke)),
                    summary.getX() + 1, summary.getY() + 15, summary.getWidth() - 2, 10,
                    juce::Justification::centredLeft, false);
    }
} // namespace vrac::ui

